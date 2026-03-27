#include "flexql.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>

struct FlexQL {
    int fd;
    std::string accumulator;
};

extern "C" {

int flexql_open(const char *host, int port, FlexQL **db) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return FLEXQL_ERROR;
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return FLEXQL_ERROR;
    }
    *db = new FlexQL{fd, ""};
    return FLEXQL_OK;
}

int flexql_close(FlexQL *db) {
    if (db) {
        close(db->fd);
        delete db;
    }
    return FLEXQL_OK;
}

void flexql_free(void *ptr) {
    if (ptr) free(ptr);
}

int flexql_exec(FlexQL *db, const char *sql, int (*callback)(void*, int, char**, char**), void *data, char **errmsg) {
    if (!db) return FLEXQL_ERROR;
    std::string q = sql;
    if (q.empty()) return FLEXQL_OK;
    if (q.back() != ';') q += ";";
    if (send(db->fd, q.c_str(), q.size(), 0) < 0) return FLEXQL_ERROR;

    bool error_seen = false;
    char buffer[8192];
    while (true) {
        size_t nl;
        while ((nl = db->accumulator.find('\n')) != std::string::npos) {
            std::string line = db->accumulator.substr(0, nl);
            db->accumulator.erase(0, nl + 1);
            while (!line.empty() && std::isspace(line.back())) line.pop_back();
            while (!line.empty() && std::isspace(line[0])) line.erase(0, 1);

            if (line == "END") return error_seen ? FLEXQL_ERROR : FLEXQL_OK;
            if (line.find("ERROR:") == 0) {
                if (errmsg) {
                    size_t colon = line.find(':');
                    std::string msg = line.substr(colon + 1);
                    while (!msg.empty() && std::isspace(msg[0])) msg.erase(0, 1);
                    *errmsg = strdup(msg.c_str());
                }
                error_seen = true;
            } else if (line.find("ROW ") == 0) {
                std::string row_data = line.substr(4);
                std::vector<std::string> vals;
                std::stringstream ss(row_data);
                std::string v;
                while (std::getline(ss, v, '\t')) vals.push_back(v);
                std::vector<char*> argv;
                for (auto& vs : vals) argv.push_back((char*)vs.c_str());
                if (callback && !error_seen) callback(data, (int)argv.size(), argv.data(), nullptr);
            }
        }
        ssize_t n = recv(db->fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) return FLEXQL_ERROR;
        buffer[n] = 0;
        db->accumulator += buffer;
    }
}

} // extern "C"
