#pragma once
#include "executor/executor.h"
#include "server/thread_pool.h"
#include <string>
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// SERVER
//
// TCP server that:
//   1. Listens on host:port
//   2. For each accepted connection, dispatches to the thread pool
//   3. Each handler: reads a length-prefixed SQL string, executes via Executor,
//      writes length-prefixed result CSV back to the client.
//
// Wire protocol:
//   Request:   [uint32_t sql_len][sql_bytes]
//   Response:  [uint32_t msg_len][msg_bytes]  (newline-separated result rows,
//                                              "OK\n" on non-SELECT success,
//                                              "ERR:<msg>\n" on error)
// ─────────────────────────────────────────────────────────────────────────────
class Server {
public:
    Server(const std::string& host, int port, const std::string& data_dir,
           size_t num_threads = 0);
    ~Server();

    // Blocking: runs the accept loop until stop() is called.
    void run();

    // Signal graceful shutdown.
    void stop();

private:
    std::string   host_;
    int           port_;
    int           listen_fd_{-1};

    Executor      executor_;
    ThreadPool    pool_;
    std::atomic<bool> running_{false};

    // Handle one client connection (runs inside thread pool).
    void handle_client(int client_fd);

    // Read exactly n bytes from fd; returns false on EOF/error.
    static bool read_all(int fd, char* buf, size_t n);

    // Write exactly n bytes to fd; returns false on error.
    static bool write_all(int fd, const char* buf, size_t n);
};
