#include "organizer.hpp"

using namespace std;

const string cont = "CON";

void handle_new_connection(int server_fd, vector<int> &clients, unordered_map<int, string> &player_info, map<int, string> &room_map, unordered_map<string, int> &scores) {
    int new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    clients.push_back(new_socket);
    string name_command = "Enter your name: ";
    send(new_socket, name_command.c_str(), name_command.size(), 0);
    char name_buffer[1024];
    int valread = read(new_socket, name_buffer, 1024);
    name_buffer[valread] = '\0';
    string player_name = name_buffer;
    if (scores.find(player_name) == scores.end()) {
        scores[player_name] = 0;
    }
    player_info[new_socket] = player_name;
    cout << "New connection, socket fd is " << new_socket << ", player name is: " << player_name << endl;
    list_rooms(new_socket, room_map);
}

void list_rooms(int client_socket, const map<int, string> &room_map) {
    string room_list = "Available rooms:\n";
    for (const auto &room : room_map) {
        room_list += to_string(room.first) + ". " + room.second + "\n";
    }
    room_list += "Enter room number to join: ";
    send(client_socket, room_list.c_str(), room_list.size(), 0);
}

bool join_room(int client_socket, int room_number, map<int, string> &room_map,unordered_map<int, string> player_info,vector<vector<string>> &room_info) {
    if (room_map.find(room_number) != room_map.end()) {
        if (room_info[room_number].size() < 2) {
            room_info[room_number].push_back(player_info[client_socket]);
            if (room_info[room_number].size() == 2) {
                for(auto player:player_info) {
                    if(player.second == room_info[room_number][0]) {
                        send(player.first, "Start", 5, 0);
                    }
                }
                send(client_socket, "Start", 5, 0);
                return true;
            }
        }
        else {
            string msg = "You can not join. Room " + to_string(room_number) + "is full!\n";
            send(client_socket, msg.c_str(), msg.size(), 0);

        }
        send(client_socket, "Wait", 4, 0);
        return false;
    } else {
        string msg = "Room not available. Please choose another room.\n";
        send(client_socket, msg.c_str(), msg.size(), 0);
    }
    return false;
}

string determine_winner(const string &choice1, const string &choice2, string player1_name, string player2_name) {
    if (choice1 == "timeout" && choice2 == "timeout") return "Draw";
    if (choice1 == "timeout") return "Player 2 <<<\n    " + player2_name + ">>> WINS! YAYY!\n";
    if (choice2 == "timeout") return "Player 1 <<<\n    " + player1_name + ">>> WINS! YAYY!\n";

    if (choice1 == choice2) return "Draw";
    if ((choice1 == "rock" && choice2 == "scissors") ||
        (choice1 == "scissors" && choice2 == "paper") ||
        (choice1 == "paper" && choice2 == "rock")) {
        return "Player 1 <<<\n    " + player1_name + ">>> WINS! YAYY!\n";
    }
    return "Player 2 <<<\n    " + player2_name + ">>> WINS! YAYY!\n";
}

string play(int room_number, vector<vector<string>> room_info,unordered_map<int, string> player_info, unordered_map <string, int> &scores) {
    string player1 = room_info[room_number][0], player2 = room_info[room_number][1];
    int val1, val2, sd1, sd2;
    char player1_buffer[1024], player2_buffer[1024];
    for(auto player: player_info) {
        if (player.second == player1) {
            sd1 = player.first;
        }
        if (player.second == player2) {
            sd2 = player.first;
        }
    }
    string play_command = "Enter your choice (rock, paper, scissors): ";
    send(sd1, play_command.c_str(), play_command.size(), 0);
    send(sd2, play_command.c_str(), play_command.size(), 0);
        for(auto player: player_info) {
        if (player.second == player1) {
            val1 = read(player.first, player1_buffer, 1024);
        }
        if (player.second == player2) {
            val2 = read(player.first, player2_buffer, 1024);
        }
    }
    player1_buffer[val1] = '\0';
    player2_buffer[val2] = '\0';
    string play1 = player1_buffer, play2 = player2_buffer;

    room_info[room_number].pop_back();
    room_info[room_number].pop_back();
    string winner = determine_winner(play1,play2,player1,player2);

    if (winner.find(player1) != string::npos) {
        scores[player1]++;
    }
    else if(winner.find(player2) != string::npos) {
        scores[player2]++;
    }
    write(2, winner.c_str(), winner.size());
    return winner;
}

void print_scores(int sock, unordered_map<string, int> scores, struct sockaddr_in bc_address) {
    string scoreboard = "\nFINAL SCOREBOARD\n----------------\n";
    for(auto score: scores) {
        scoreboard += (score.first + to_string(score.second) + "\n----\n");
    }
    write(2, scoreboard.c_str(), scoreboard.size());
    sendto(sock, scoreboard.c_str(), scoreboard.size(), 0,(struct sockaddr *)&bc_address, sizeof(bc_address));
}

void delete_room(int room_number, vector<vector<string>> &room_info) {
    room_info[room_number].clear();
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <IP> <Port> <#Rooms>" << endl;
        return 1;
    }
    map<string, int> players = {};
    vector<vector<string>> room_info(100);
    unordered_map<string, int> scores = {};
    string ip = argv[1];
    int port = atoi(argv[2]);
    int num_rooms = atoi(argv[3]);

    map<int, string> room_map;
    for (int i = 1; i <= num_rooms; ++i) {
        room_map[i] = "Room " + to_string(i);
    }

    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    unordered_map<int, string> player_info;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    vector<int> clients = {0, server_fd};

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip.c_str());
    address.sin_port = htons(port);


    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, num_rooms) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    int sock, brc = 1, op = 1;
    struct sockaddr_in bc_address;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &brc, sizeof(brc));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &op, sizeof(op));

    bc_address.sin_family = AF_INET;
    bc_address.sin_port = htons(8080);
    bc_address.sin_addr.s_addr = inet_addr("192.168.8.255");

    bind(sock, (struct sockaddr *)&bc_address, sizeof(bc_address));

    fd_set readfds;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(0,&readfds);
        int max_sd = server_fd;

        for (int i = 0; i < clients.size(); i++) {
            int sd = clients[i];

            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            cout << "select error" << endl;
        }
        for (int i = 0; i < clients.size(); i++) {
            int sd = clients[i];
            if (FD_ISSET(sd, &readfds)) {
                if (sd == 0) //STDIN
                {
                    char buffer[1024] = {0};
                    read(0 , buffer, 1024);
                    string buffer_string(buffer);
                    buffer_string.pop_back();
                    if (buffer_string == "end_game")
                    {
                        cerr << "najk\n";
                        string end = "END";
                        print_scores(sock, scores,bc_address);
                        exit(0);
                    }
                }
                else if (sd == server_fd) {
                    handle_new_connection(server_fd, clients, player_info, room_map, scores);
                }
                else {
                    int  sd1 = sd, sd2 = sd;
                    bool flag = false;
                    int i = 0;
                    do
                    {
                    if (i > 0) {
                        list_rooms(sd1, room_map);
                        list_rooms(sd2, room_map);
                        break;
                    }
                    i++;
                    char buffer[1025];
                    int valread = read(sd, buffer, 1024);
                    buffer[valread] = '\0';
                    int room_number = atoi(buffer);

                    if (join_room(sd, room_number, room_map,player_info,room_info)){
                        string play_msg = play(room_number, room_info, player_info, scores);
                        sendto(sock, play_msg.c_str(), play_msg.size(), 0,(struct sockaddr *)&bc_address, sizeof(bc_address));
                        flag = true;
                        string play_end = "play_end";
                        string player1 = room_info[room_number][0], player2 = room_info[room_number][1];
                        for(auto player: player_info) {
                            if (player.second == player1) {
                                sd1 = player.first;
                            }
                            if (player.second == player2) {
                                sd2 = player.first;
                            }
                        }
                        send(sd1, play_end.c_str(), play_end.size(), 0);
                        send(sd2, play_end.c_str(), play_end.size(), 0);
                        delete_room(room_number, room_info);
                    }
                    } while (flag);
                }

            }
        }
    }
    return 0;
}
