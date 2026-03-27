#include "executor/executor.h"
#include "parser/parser.h"
#include <iostream>
#include <filesystem>

int main() {
    std::string data_dir = "./data_persistence_test";
    std::filesystem::remove_all(data_dir);
    std::filesystem::create_directories(data_dir);

    try {
        Executor exec(data_dir);
        std::string err;
        Parser parser;
        
        // 1. Create table
        auto stmt1 = parser.parse("CREATE TABLE persist_test (id INT);");
        int rc = exec.exec(stmt1, nullptr, nullptr, err);
        if (rc != 0) { std::cerr << "Create failed: " << err << "\n"; return 1; }
        
        // 2. Insert row
        auto stmt2 = parser.parse("INSERT INTO persist_test VALUES (101);");
        rc = exec.exec(stmt2, nullptr, nullptr, err);
        if (rc != 0) { std::cerr << "Insert failed: " << err << "\n"; return 1; }

        std::cout << "Success: Data files created in " << data_dir << "\n";
        
        // 3. Final check
        if (std::filesystem::exists(data_dir + "/persist_test.fql")) {
            std::cout << "FOUND: persist_test.fql\n";
        } else {
            std::cout << "NOT FOUND: persist_test.fql\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
