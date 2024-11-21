#ifndef ORGANIZER_HPP
#define ORGANIZER_HPP

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <map>
#include <vector>
using namespace std;

#define REGISTER 0
#define SELECT_ROOM 1
#define PLAY 2

void handle_new_connection(int server_fd, std::vector<int> &clients, std::unordered_map<int, std::string> &player_info, std::map<int, std::string> &room_map, std::unordered_map<int, std::string> &scores);
void list_rooms(int client_socket, const std::map<int, std::string> &room_map);
bool join_room(int client_socket, int room_number, std::map<int, std::string> &room_map,unordered_map<int, string> player_info,vector<vector<string>> &room_info);
string play(int room_number, vector<vector<string>> room_info, unordered_map<int, string> player_info, unordered_map<string, int> &scores);
void print_scores(int sock, unordered_map<string, int> scores, struct sockaddr_in bc_address);
void delete_room(int room_number, vector<vector<string>> &room_info);

#endif
