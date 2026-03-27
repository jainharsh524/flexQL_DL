#pragma once
#include <string>
#include <vector>

enum class StatementType {
    NONE,
    CREATE,
    INSERT,
    SELECT,
    DELETE
};

struct CreateStmt {
    std::string table_name;
    struct Col {
        std::string name;
        std::string type_str;
    };
    std::vector<Col> columns;
};

struct InsertStmt {
    std::string table_name;
    std::string raw_values; // Store the raw "(...),(...)" part for the executor to burst
};

struct WhereClause {
    std::string column_name;
    std::string op;
    std::string value_str;
};

struct OrderBy {
    std::string column;
    bool ascending = true;
};

struct Join {
    std::string table;
    std::string on_left;
    std::string on_right;
};

struct SelectStmt {
    std::vector<std::string> columns;
    std::string table_name;
    std::vector<WhereClause> where;
    Join join;
    OrderBy order_by;
    bool has_join = false;
    bool has_order = false;
};

struct DeleteStmt {
    std::string table_name;
};

struct Statement {
    StatementType type;
    CreateStmt create_stmt;
    InsertStmt insert_stmt;
    SelectStmt select_stmt;
    DeleteStmt delete_stmt;
};
