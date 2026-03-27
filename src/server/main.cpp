#include "network/server.h"
#include "execution/executor.h"
#include "storage/table_manager.h"
#include "index/index_manager.h"

int main() {
    TableManager tm;
    IndexManager im;

    Executor executor(tm, im);

    Server server(9000, executor, 8); // 8 threads
    server.start();

    return 0;
}