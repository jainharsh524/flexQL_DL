#include "parser/parser.h"
#include <cassert>
#include <iostream>

static void test_create() {
    Parser p;
    auto stmt = p.parse("CREATE TABLE users (id INT, name CHAR(32), score FLOAT, expires DATETIME);");
    auto& cs = std::get<CreateStmt>(stmt);
    assert(cs.table_name == "users");
    assert(cs.columns.size() == 4);
    assert(cs.columns[0].name == "id");
    assert(cs.columns[0].type_str == "INT");
    assert(cs.columns[2].type_str == "FLOAT");
    std::cout << "[PASS] parse CREATE TABLE\n";
}

static void test_insert() {
    Parser p;
    auto stmt = p.parse("INSERT INTO users VALUES (1, 'Alice', 99.5, '2030-01-01 00:00:00');");
    auto& is = std::get<InsertStmt>(stmt);
    assert(is.table_name == "users");
    assert(is.values.size() == 4);
    assert(is.values[0] == "1");
    assert(is.values[1] == "Alice");
    std::cout << "[PASS] parse INSERT INTO\n";
}

static void test_select_star() {
    Parser p;
    auto stmt = p.parse("SELECT * FROM users;");
    auto& ss = std::get<SelectStmt>(stmt);
    assert(ss.table_name == "users");
    assert(ss.star == true);
    assert(!ss.where);
    assert(!ss.join);
    std::cout << "[PASS] parse SELECT *\n";
}

static void test_select_where() {
    Parser p;
    auto stmt = p.parse("SELECT id, name FROM users WHERE id = 42;");
    auto& ss = std::get<SelectStmt>(stmt);
    assert(!ss.star);
    assert(ss.cols.size() == 2);
    assert(ss.where);
    assert(ss.where->col_name == "id");
    assert(ss.where->op == WhereOp::EQ);
    assert(ss.where->value_str == "42");
    std::cout << "[PASS] parse SELECT ... WHERE\n";
}

static void test_select_join() {
    Parser p;
    auto stmt = p.parse(
        "SELECT * FROM users INNER JOIN orders ON users.id = orders.uid WHERE users.id > 10;");
    auto& ss = std::get<SelectStmt>(stmt);
    assert(ss.join);
    assert(ss.join->right_table == "orders");
    assert(ss.join->left_col  == "id");
    assert(ss.join->right_col == "uid");
    assert(ss.where);
    assert(ss.where->op == WhereOp::GT);
    std::cout << "[PASS] parse INNER JOIN + WHERE\n";
}

int main() {
    test_create();
    test_insert();
    test_select_star();
    test_select_where();
    test_select_join();
    std::cout << "\nAll parser tests PASSED.\n";
    return 0;
}
