#include "server/server.h"
#include <iostream>
#include <cstdlib>
#include <string>

int main(int argc, char* argv[]) {
    std::string host     = "127.0.0.1";
    int         port     = 9000;
    std::string data_dir = "./data";
    size_t      threads  = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host"     && i+1 < argc) host     = argv[++i];
        else if (arg == "--port"     && i+1 < argc) port     = std::atoi(argv[++i]);
        else if (arg == "--data-dir" && i+1 < argc) data_dir = argv[++i];
        else if (arg == "--threads"  && i+1 < argc) threads  = std::stoul(argv[++i]);
    }

    std::cout << "[FlexQL Server] Listening on " << host << ":" << port
              << "  data=" << data_dir << std::endl;
    try {
        Server server(host, port, data_dir, threads);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
