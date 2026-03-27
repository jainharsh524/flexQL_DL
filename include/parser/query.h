#pragma once

#include <string>
#include <vector>

enum class QueryType {
    CREATE,
    INSERT,
    SELECT
};

struct Condition {
    std::string column;
    std::string value;
};

struct JoinClause {
    std::string table;
    std::string left_col;
    std::string right_col;
};

struct Query {
    QueryType type;

    std::string table;

    // CREATE
    std::vector<std::pair<std::string, std::string>> columns;

    // INSERT
    std::vector<std::string> values;

    // SELECT
    std::vector<std::string> select_columns;
    Condition where;
    bool has_where = false;

    JoinClause join;
    bool has_join = false;
};