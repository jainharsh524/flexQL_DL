#pragma once

#include <string>

// opaque struct
struct FlexQL {
    int sockfd;
};

int client_open(const char *host, int port, FlexQL **db);
int client_close(FlexQL *db);
int client_exec(
    FlexQL *db,
    const char *sql,
    int (*callback)(void*, int, char**, char**),
    void *arg,
    char **errmsg
);

void flexql_free(void *ptr);