#include "log.h"
#include <iostream>
#include <vector>
#include <sstream>
using namespace std;

void log_message(LogLevel level, const string &process_name, const string &message) {
    string level_str;
    if (level == INFO) {
        level_str = "INFO";
    } else if (level == ERROR) {
        level_str = "ERROR";
    }

    cout << "[" << level_str << ": " << process_name << "] " << message << endl;
}

// Function to split string based on delimiter
vector<string> split(const string &str, char delimiter) {
    vector<string> tokens;
    string token;
    stringstream ss(str);
    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to extract warehouse name from file path
string get_warehouse_name(const string &file_path) {
    vector<string> tokens = split(file_path, '/');
    string file_name = tokens.back(); // Get the last element which is file name
    tokens = split(file_name, '.');
    return tokens[0]; // Get the first part before the extension
}

vector<string> read_parts(const string &filename) {
    vector<string> parts;
    ifstream file(filename);
    string line;

    if (getline(file, line)) {
        stringstream str(line);
        string part;
        while (getline(str, part, ',')) {
            parts.push_back(part);
        }
    }

    return parts;
}
