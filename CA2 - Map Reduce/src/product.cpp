#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include "log.h"
#include <unordered_map>
using namespace std;

void process_product(const string &name, const string &pipe_name, int write_fd, int num_of_products) {
    vector<string> products = read_parts(PARTS_DIR);
    unordered_map<int, string> pid_to_name = {};
    for (int i = 0 ; i < products.size() ; i++) {
        pid_to_name[i + 1] = products[i];
    }
    int fd = open(pipe_name.c_str(), O_RDONLY);
    if (fd == -1) {
        log_message(ERROR, "product", "Failed to open the named pipe " + pipe_name);
        exit(1);
    }

    char buffer[256];
    float total_leftovers = 0;
    float total_left_quant = 0;
    log_message(INFO, "product", "Reading data from the named pipe (pipe_name: " + pipe_name + ").");
    for (int i = 0 ; i < num_of_products ; i++) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n == 0) break;
        buffer[n] = '\0';
        stringstream ss(buffer);
        string pid_str, leftovers_v, leftovers_q;
        getline(ss, pid_str, ',');
        getline(ss, leftovers_v, ',');
        getline(ss, leftovers_q, ',');
        total_leftovers += stof(leftovers_v);
        total_left_quant += stof(leftovers_q);
    }
    string message = to_string(total_leftovers) + "," + to_string(total_left_quant) + ",";
    write(write_fd, message.c_str(), message.size());
    close(fd);

    log_message(INFO, "product", "Total leftovers for product " + name + ": " + to_string(total_leftovers));
    string result = to_string(total_leftovers) + "," + to_string(total_left_quant) + "\n";
    write(write_fd, result.c_str(), result.size());

    close(write_fd);
    log_message(INFO, "product", "Product processing completed for " + name);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        cerr << "Usage: " << argv[0] << " <product_name> <read_fd> <write_fd> <pipe_name> <num_of_products>" << endl;
        return 1;
    }

    string product_name = argv[1];
    int read_fd = stoi(argv[2]);
    int write_fd = stoi(argv[3]);
    string pipe_name = argv[4];
    int num_of_products = stoi(argv[5]);
    log_message(INFO, "product", "Starting product processing for " + product_name);
    process_product(product_name, pipe_name, write_fd, num_of_products);

    return 0;
}
