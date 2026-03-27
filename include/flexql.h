#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Return codes
#define FLEXQL_OK 0
#define FLEXQL_ERROR -1

// Opaque DB handle
typedef struct FlexQL FlexQL;

// Open connection
int flexql_open(const char *host, int port, FlexQL **db);

// Close connection
int flexql_close(FlexQL *db);

// Execute SQL
int flexql_exec(
    FlexQL *db,
    const char *sql,
    int (*callback)(void*, int, char**, char**),
    void *arg,
    char **errmsg
);

// Free memory
void flexql_free(void *ptr);

#ifdef __cplusplus
}
#endif