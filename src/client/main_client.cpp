#include "client/flexql.h"
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <iomanip>

// Simple pretty-printer callback
static int print_row(void* /*arg*/, int argc, char** argv, char** /*col_names*/) {
    for (int i = 0; i < argc; ++i) {
        if (i) std::cout << " | ";
        std::cout << (argv[i] ? argv[i] : "NULL");
    }
    std::cout << "\n";
    return 0;
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int         port = 5433;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--host" && i+1 < argc) host = argv[++i];
        else if (arg == "--port" && i+1 < argc) port = std::atoi(argv[++i]);
    }

    FlexQL* db = nullptr;
    int rc = flexql_open(host.c_str(), port, &db);
    if (rc != 0) {
        std::cerr << "Cannot connect to " << host << ":" << port
                  << " (code=" << rc << ")\n";
        return 1;
    }
    std::cout << "FlexQL connected to " << host << ":" << port << "\n";

    std::string line;
    while (true) {
        std::cout << "FlexQL> ";
        std::cout.flush();
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "exit" || line == "quit") break;

        char* errmsg = nullptr;
        int res = flexql_exec(db, line.c_str(), print_row, nullptr, &errmsg);
        if (res != 0) {
            std::cerr << "Error: " << (errmsg ? errmsg : "unknown") << "\n";
            flexql_free(errmsg);
        }
    }

    flexql_close(db);
    return 0;
}
