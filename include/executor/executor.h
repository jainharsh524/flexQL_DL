#pragma once
#include "storage/table.h"
#include "parser/ast.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

typedef int (*ExecCallback)(void* arg, int argc, char** argv, char** col_names);

class Executor {
public:
    explicit Executor(const std::string& data_dir);
    ~Executor() = default;

    int exec(const Statement& stmt, ExecCallback cb, void* arg, std::string& errmsg);

private:
    std::string data_dir_;
    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
    std::mutex tables_mu_;

    int exec_create(const CreateStmt& s, std::string& err);
    int exec_insert(const InsertStmt& s, std::string& err);
    int exec_select(const SelectStmt& s, ExecCallback cb, void* arg, std::string& err);
    int exec_delete(const DeleteStmt& s, std::string& err);

    Table* get_table(const std::string& name, std::string& err);
    Row materialize_row(Table* table, RID rid);
};
