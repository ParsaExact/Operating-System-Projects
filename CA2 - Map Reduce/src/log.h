#ifndef LOG_H
#define LOG_H

#include <string>
#include <vector>
#include <fstream>
using namespace std;
enum LogLevel {
    INFO,
    ERROR,
};

const string PARTS_DIR = "../files/goods/Parts.csv";

void log_message(LogLevel level, const std::string &process_name, const std::string &message);
vector<string> split(const string &str, char delimiter);
string get_warehouse_name(const string &file_path);
vector<string> read_parts(const string &filename);

#endif // LOG_H
