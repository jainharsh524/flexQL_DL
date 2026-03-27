#include "server/server.h"
#include "parser/parser.h"
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

Server::Server(const std::string& host, int port, const std::string& data_dir, size_t threads)
    : host_(host), port_(port), executor_(data_dir), pool_(threads) {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) exit(1);
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    addr.sin_port = htons(port);
    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) exit(1);
}

Server::~Server() { if (listen_fd_ >= 0) close(listen_fd_); }

void Server::run() {
    if (listen(listen_fd_, 128) < 0) return;
    running_ = true;
    while (running_) {
        int client_fd = accept(listen_fd_, nullptr, nullptr);
        if (client_fd < 0) continue;
        pool_.submit([this, client_fd] { handle_client(client_fd); });
    }
}

void Server::stop() { running_ = false; if (listen_fd_ >= 0) close(listen_fd_); listen_fd_ = -1; }

static int server_exec_callback(void* arg, int argc, char** argv, char** colnames) {
    (void)colnames;
    int client_fd = *(int*)arg;
    std::string row = "ROW ";
    for (int i = 0; i < argc; i++) {
        if (i > 0) row += "\t";
        row += (argv[i] ? argv[i] : "NULL");
    }
    row += "\n";
    send(client_fd, row.c_str(), row.size(), 0);
    return 0;
}

void Server::handle_client(int client_fd) {
    std::string accumulator;
    char buffer[8192];
    while (true) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;
        buffer[n] = 0;
        accumulator += buffer;

        size_t semi;
        while ((semi = accumulator.find(';')) != std::string::npos) {
            std::string sql = accumulator.substr(0, semi);
            accumulator.erase(0, semi + 1);
            
            Parser parser;
            Statement stmt = parser.parse(sql);
            std::string err;
            
            if (stmt.type != StatementType::CREATE && stmt.type != StatementType::INSERT &&
                stmt.type != StatementType::SELECT && stmt.type != StatementType::DELETE) {
                err = "Invalid SQL";
            } else {
                executor_.exec(stmt, server_exec_callback, &client_fd, err);
            }

            if (!err.empty()) {
                std::string emsg = "ERROR: " + err + "\n";
                send(client_fd, emsg.c_str(), emsg.size(), 0);
            }
            send(client_fd, "END\n", 4, 0);
        }
    }
    close(client_fd);
}
