#pragma once

#include "execution/executor.h"
#include "parser/parser.h"
#include "parser/tokenizer.h"
#include "concurrency/thread_pool.h"

#include <string>

class Server {
private:
    int port;
    int server_fd;

    ThreadPool thread_pool;

    Executor& executor;

public:
    Server(int port, Executor& executor, int threads = 4);

    void start();

private:
    void handle_client(int client_socket);
};