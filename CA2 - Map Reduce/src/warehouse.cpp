#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <unordered_map>
#include "log.h"

using namespace std;

struct Record {
    string name;
    float price;
    float quantity;
    string type;
};

vector<Record> read_csv(const string &filename) {
    vector<Record> records;
    ifstream file(filename);
    string line, word;

    while (getline(file, line)) {
        stringstream str(line);
        Record record;

        getline(str, record.name, ',');
        getline(str, word, ',');
        record.price = stof(word);
        getline(str, word, ',');
        record.quantity = stof(word);
        getline(str, record.type, ',');

        records.push_back(record);
    }
    return records;
}

void process_warehouse(const string &filename, int read_fd, int write_fd, int result_fd, const unordered_map<int, string> &pid_to_name, const vector<string> &named_pipes) {
    vector<Record> records = read_csv(filename);
    char buffer[256];

    log_message(INFO, "warehouse", "Reading selected PIDs from unnamed pipe (fd: " + to_string(read_fd) + ").");
    ssize_t n = read(read_fd, buffer, sizeof(buffer));
    if (n == -1) {
        perror("read");
        log_message(ERROR, "warehouse", "Failed to read from the unnamed pipe (fd: " + to_string(read_fd) + ").");
        exit(1);
    } else if (n == 0) {
        log_message(INFO, "warehouse", "Reached end of pipe, no more data to read.");
    }

    buffer[n] = '\0';
    log_message(INFO, "warehouse", "Successfully read selected PIDs from unnamed pipe: " + string(buffer));

    stringstream ss(buffer);
    string pid_str;

    vector<int> selected_pids;
    while (ss >> pid_str) {
        selected_pids.push_back(stoi(pid_str) - 1); // Convert to zero-based index
    }

    // Calculate profits for selected products
    float left_q = 0.0, left_v = 0.0, profit = 0.0;
    vector<float> quantities = {};
    vector<float> prices = {};
    for (const auto &pid : selected_pids) {
        left_q = 0;
        left_v = 0;
        string product_name = pid_to_name.at(pid + 1);

        for (int i = 0 ; i < records.size() ; i++) {
            if (records[i].name == product_name) {
                if (records[i].type.substr(0 , 5) == "input") {
                    left_q += records[i].quantity;
                    left_v += records[i].quantity * records[i].price;
                } else if (records[i].type.substr(0 , 6) == "output") {
                    if (records[i].quantity > left_q) {
                        left_q = 0;
                    }
                    else {
                        left_q -= records[i].quantity;
                    }
                    for (int k = 0; k < records.size() ; k++) {
                        if (records[k].type.substr(0 , 5) == "input" && records[k].name == product_name
                            && records[k].quantity > 0) {
                                if (records[i].quantity <= records[k].quantity) {
                                    profit += (records[i].price - records[k].price) * records[i].quantity;
                                    left_v -= (records[i].quantity * records[k].price);
                                    records[k].quantity -= records[i].quantity;
                                    break;

                                }
                                else {
                                    profit += ((records[i].price - records[k].price) * records[k].quantity);
                                    records[k].quantity = 0;
                                    left_v -= (records[k].quantity * records[k].price);
                                }
                        }
                    }
                }
            }
        }
        quantities.push_back(left_q);
        prices.push_back(left_v);
    }
    
    string result = to_string(profit) + "\n";
    write(result_fd, result.c_str(), result.size());

    // Calculate leftovers for each product and send to product processes
    log_message(INFO, "warehouse", "Sending leftovers to product processes via named pipes.");

    for (int i = 0; i < selected_pids.size() ; i++) {
        int fd = open(named_pipes[selected_pids[i]].c_str(), O_WRONLY);
        if (fd == -1) {
            log_message(ERROR, "warehouse", "Failed to open the named pipe " + named_pipes[selected_pids[i]]);
            exit(1);
        }
        string message = to_string(selected_pids[i]) + "," + to_string(prices[i]) + "," + to_string(quantities[i]) + "," + "\n";
        write(fd, message.c_str(), message.size());
        close(fd);
    }

    close(read_fd);
    close(write_fd);
    close(result_fd);
    log_message(INFO, "warehouse", "Warehouse processing completed.");
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0] << " <warehouse_file> <read_fd> <write_fd> <result_fd> <named_pipe_1> [<named_pipe_2> ...]" << endl;
        return 1;
    }

    string filename = argv[1];
    int read_fd = stoi(argv[2]);
    int write_fd = stoi(argv[3]);
    int result_fd = stoi(argv[4]);

    vector<string> named_pipes;
    for (int i = 5; i < argc; ++i) {
        named_pipes.push_back(argv[i]);
    }
    vector<string> parts = read_parts(PARTS_DIR);
    unordered_map<int, string> pid_to_name = {};
    for (int i = 0 ; i < parts.size() ; i++) {
        pid_to_name[i + 1] = parts[i];
    }

    log_message(INFO, "warehouse", "Starting warehouse processing for " + filename);
    process_warehouse(filename, read_fd, write_fd, result_fd, pid_to_name, named_pipes);

    return 0;
}
