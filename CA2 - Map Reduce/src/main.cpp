#include <iostream>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>
#include "log.h"

using namespace std;


vector<string> get_warehouse_files(const string &directory) {
    vector<string> warehouse_files;
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(directory.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            string file_name = ent->d_name;
            if (file_name.find(".csv") != string::npos) {
                warehouse_files.push_back(directory + "/" + file_name);
            }
        }
        closedir(dir);
    } else {
        perror("opendir");
        exit(EXIT_FAILURE);
    }
    return warehouse_files;
}

void create_warehouse_process(const string &filename, int read_fd, int write_fd, int result_fd, const vector<string> &named_pipes) {
    log_message(INFO, "main", "Creating warehouse process for " + filename);
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        vector<const char*> args = {"./warehouse", filename.c_str(), to_string(read_fd).c_str(), to_string(write_fd).c_str(), to_string(result_fd).c_str()};
        for (const auto &pipe : named_pipes) {
            args.push_back(pipe.c_str());
        }
        args.push_back(NULL);
        execv("./warehouse", const_cast<char* const*>(args.data()));
        perror("execv");
        exit(1);
    } else if (pid < 0) {
        // Error forking
        perror("fork");
        log_message(ERROR, "main", "Failed to fork warehouse process for " + filename);
        exit(1);
    } else {
        log_message(INFO, "main", "Warehouse process created with PID " + to_string(pid));
    }
}

void create_product_process(const string &product, int read_fd, int write_fd, const string &named_pipe, int num_of_processes) {
    log_message(INFO, "main", "Creating product process for " + product);
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execl("./product", "./product", product.c_str(), to_string(read_fd).c_str(), to_string(write_fd).c_str(), named_pipe.c_str(), to_string(num_of_processes).c_str(), NULL);
        perror("execl");
        exit(1);
    } else if (pid < 0) {
        // Error forking
        perror("fork");
        log_message(ERROR, "main", "Failed to fork product process for " + product);
        exit(1);
    } else {
        log_message(INFO, "main", "Product process created with PID " + to_string(pid));
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <stores_directory>" << endl;
        return 1;
    }

    string stores_directory = argv[1];
    vector<string> warehouse_files = get_warehouse_files(stores_directory);
    vector<string> parts = read_parts(PARTS_DIR);
    unordered_map<int, string> pid_to_name = {};
    for (int i = 0 ; i < parts.size() ; i++) {
        pid_to_name[i + 1] = parts[i];
    }
    cout << "Available products: " << endl;
    for (size_t i = 0; i < parts.size(); ++i) {
        cout << i + 1 << ". " << parts[i] << endl;
    }

    string selected_pids;
    cout << "Enter the product numbers to calculate (separated by space): ";
    getline(cin, selected_pids);

    // Create unnamed pipes for warehouses
    vector<int> warehouse_pipes(warehouse_files.size() * 4);
    for (size_t i = 0; i < warehouse_files.size(); ++i) {
        if (pipe(&warehouse_pipes[4 * i]) == -1 || pipe(&warehouse_pipes[4 * i + 2]) == -1) {
            perror("pipe");
            log_message(ERROR, "main", "Failed to create pipes for warehouse " + warehouse_files[i]);
            exit(1);
        }
        log_message(INFO, "main", "Created pipes for warehouse " + warehouse_files[i]);
    }

    // Create named pipes for communication between warehouses and products
    vector<string> named_pipes(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        named_pipes[i] = "/tmp/product_pipe_" + to_string(i);
        if (mkfifo(named_pipes[i].c_str(), 0666) == -1) {
            if (errno == EEXIST) {
                unlink(named_pipes[i].c_str());
                if (mkfifo(named_pipes[i].c_str(), 0666) == -1) {
                    perror("mkfifo");
                    log_message(ERROR, "main", "Failed to create named pipe " + named_pipes[i]);
                    exit(1);
                }
            } else {
                perror("mkfifo");
                log_message(ERROR, "main", "Failed to create named pipe " + named_pipes[i]);
                exit(1);
            }
        }
        log_message(INFO, "main", "Created named pipe " + named_pipes[i]);
    }

    // Create warehouse processes
    for (size_t i = 0; i < warehouse_files.size(); ++i) {
        create_warehouse_process(warehouse_files[i], warehouse_pipes[4 * i], warehouse_pipes[4 * i + 1], warehouse_pipes[4 * i + 3], named_pipes);
    }
    stringstream ss(selected_pids);
    string pid_str;

    vector<int> product_ids = {};
    while (ss >> pid_str) {
        product_ids.push_back(stoi(pid_str) - 1);
    }

    // Create unnamed pipes for products
    vector<int> product_pipes(parts.size() * 2);
    for (size_t i = 0; i < product_ids.size(); ++i) {
        int pid = product_ids[i];
        if (pipe(&product_pipes[2 * pid]) == -1) {
            perror("pipe");
            log_message(ERROR, "main", "Failed to create pipe for product " + parts[pid]);
            exit(1);
        }
        log_message(INFO, "main", "Created pipe for product " + parts[pid]);
        create_product_process(parts[pid], product_pipes[2 * pid], product_pipes[2 * pid + 1], named_pipes[pid], product_ids.size());
    }

    // Send selected product IDs to warehouse processes
    for (size_t i = 0; i < warehouse_files.size(); ++i) {
        write(warehouse_pipes[4 * i + 1], selected_pids.c_str(), selected_pids.size());
        fsync(warehouse_pipes[4 * i + 1]); // Ensure data is flushed
        log_message(INFO, "main", "Wrote selected product IDs to warehouse pipe.");
    }

    // Read results from warehouse pipes
    char buffer_profit[256];
    vector<float> profits = {};
    ssize_t n;
    for (size_t i = 0; i < warehouse_files.size(); ++i) {
        if ((n = read(warehouse_pipes[4 * i + 2], buffer_profit, sizeof(buffer_profit))) > 0) {
            buffer_profit[n] = '\0';
            string prof = buffer_profit;
            profits.push_back(stof(prof));
            log_message(INFO, "main", "Read result from warehouse pipe: " + string(buffer_profit, n));
        }
        close(warehouse_pipes[4 * i + 2]);
    }

    // Read data from product pipes
    char buff_left[256];
    vector<pair<int,pair<string,string>>> leftover_messages = {};
    for (size_t i = 0; i < product_ids.size(); ++i) {
        if ((n = read(product_pipes[2 * product_ids[i]], buff_left, sizeof(buff_left))) > 0) {
            buff_left[n] = '\0';
            string first,second;
            stringstream ss(buff_left);
            getline(ss, first, ',');
            getline(ss, second, ',');
            leftover_messages.push_back({product_ids[i] + 1, {first,second}});
            log_message(INFO, "main", "Read result from product pipe: " + string(buff_left, n));
        }
        close(product_pipes[2 * product_ids[i]]);
    }
    // Remove named pipes
    for (const auto &pipe_name : named_pipes) {
        unlink(pipe_name.c_str());
        log_message(INFO, "main", "Removed named pipe " + pipe_name);
    }

    log_message(INFO, "main", "All processes completed successfully");

    float sum = 0;
    for (auto p: profits) {
        sum += p;
    }
    cout << "---" << endl
        <<  "The whole profit: " << sum << endl
        << "---" << endl;
    for (auto l: leftover_messages) {
        cout <<  pid_to_name[l.first] << endl << "\t"
            <<  "Total leftover quantity ---> " << l.second.second << endl << "\t"
            <<  "Total leftover price ---> " << l.second.first  << endl;
    }

    return 0;
}
