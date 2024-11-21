#include "player.hpp"

using namespace std;

volatile sig_atomic_t timed_out = 0;

void handle_alarm(int sig) {
    if (sig == SIGALRM) {
        timed_out = 1;
    }
}

string get_player_choice() {
    string choice;
    timed_out = 0;
    signal(SIGALRM, handle_alarm);
    alarm(10);
    if (cin >> choice) {
        alarm(0);
    } else {
        cin.clear();
    }
    return timed_out ? "timeout" : choice;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <IP> <Port>" << endl;
        return 1;
    }

    string ip = argv[1];
    int port = atoi(argv[2]);

    // main server
    int sock = 0, opt = 1;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "\n Socket creation error \n";
        return -1;
    }
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &opt, sizeof(opt));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // broadcast
    int bc_sock, bc = 1, op = 1;
    struct sockaddr_in bc_address;
    bc_sock = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(bc_sock, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc));
    setsockopt(bc_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &op, sizeof(op));
    bc_address.sin_family = AF_INET;
    bc_address.sin_port = htons(8080);
    bc_address.sin_addr.s_addr = inet_addr("192.168.8.255");
    bind(bc_sock, (struct sockaddr *)&bc_address, sizeof(bc_address));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "\nConnection Failed \n";
        return -1;
    }

    int opcode = REGISTER;
    unordered_map<char, bool> full_rooms;

    struct pollfd fds[3];
    fds[0].fd = bc_sock;  // Broadcast socket
    fds[0].events = POLLIN;
    fds[1].fd = sock;     // Server socket
    fds[1].events = POLLIN;
    fds[2].fd = fileno(stdin);
    fds[2].events = POLLIN;
    while (true) {
        int ret = poll(fds, 3, -1);

        if (ret > 0) {
            // Handle broadcast messages
            if (fds[0].revents & POLLIN) {
                char end[1024] = {0};
                int val = recv(bc_sock, end, 1024, 0);
                end[val] = '\0';
                string end_command = end;
                write(1, end, 1024);
                // exit(0);
            }

            // Handle server messages
            else if (fds[1].revents & POLLIN) {
                if (opcode == REGISTER) {
                    char buffer[1024] = {0};
                    recv(sock, buffer, 1024, 0);
                    write(1, buffer, 1024);
                }
                else if (opcode == SELECT_ROOM) {
                    char room_command[1024] = {0};
                    int v = read(sock, room_command, 1024);
                    room_command[v] = '\0';
                    if (string(room_command) == "play_end") {
                        write(1, "Game ended!\n", 12);
                        char room_select[1024] = {0};
                        recv(sock, room_select, 1024, 0);
                        write(1, room_select, 1024);
                    } else {
                        write(1, room_command, 1024);
                    }
                }
                else if (opcode == WAITING) {
                    char status[1024] = {0};
                    int val = read(sock, status, 1024);
                    status[val] = '\0';
                    if (string(status) == "Start") {
                        opcode = PLAY;
                    }
                }
                else if (opcode == PLAY) {
                    char play_command[1024] = {0};
                    recv(sock, play_command, 1024, 0);
                    write(1, play_command, 1024);
                    string player_choice = get_player_choice();
                    send(sock, player_choice.c_str(), player_choice.size(), 0);
                    // char play[1024] = {0};
                    // recv(sock, play, 1024, 0);
                    // write(1, play, 1024);
                    opcode = SELECT_ROOM;
                }
            }
            else if (fds[2].revents & POLLIN) {
                if (opcode == REGISTER) {
                    char name[1024] = {0};
                    read(0, name, 1024);
                    send(sock, name, 1024, 0);
                    opcode = SELECT_ROOM;
                }
                else if (opcode == SELECT_ROOM) {
                    char selected_room[1024] = {0};
                    read(0, selected_room, 1024);
                    send(sock, selected_room, 1024, 0);
                    char status[1024] = {0};
                    int val = read(sock, status, 1024);
                    status[val] = '\0';
                    if (string(status) == "Start") {
                        opcode = PLAY;
                    } else if (string(status) == "Wait") {
                        opcode = WAITING;
                    }
                }
                else if (opcode == WAITING) {
                    char status[1024] = {0};
                    int val = read(sock, status, 1024);
                    status[val] = '\0';
                    if (string(status) == "Start") {
                        opcode = PLAY;
                    }
                }
                else if (opcode == PLAY) {
                    string player_choice = get_player_choice();
                    send(sock, player_choice.c_str(), player_choice.size(), 0);
                    char play[1024] = {0};
                    recv(bc_sock, play, 1024, 0);
                    write(1, play, 1024);
                    opcode = SELECT_ROOM;
                }
            }
            }
        }
    close(bc_sock);
    close(sock);
    return 0;
}
