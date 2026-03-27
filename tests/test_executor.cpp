#include "executor/executor.h"
#include "parser/parser.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

static std::string DATA_DIR = "/tmp/fql_exec_test";

static int row_count_cb(void* arg, int, char**, char**) {
    (*static_cast<int*>(arg))++;
    return 0;
}

static void run(Executor& ex, const std::string& sql) {
    Parser p;
    Statement stmt = p.parse(sql);
    std::string err;
    int rc = ex.exec(stmt, nullptr, nullptr, err);
    if (rc != 0) throw std::runtime_error("exec failed: " + err + " on: " + sql);
}

static int run_count(Executor& ex, const std::string& sql) {
    Parser p;
    Statement stmt = p.parse(sql);
    std::string err;
    int count = 0;
    ex.exec(stmt, row_count_cb, &count, err);
    return count;
}

static void test_create_insert_select() {
    Executor ex(DATA_DIR);
    run(ex, "CREATE TABLE emp (id INT, name CHAR(32), age INT, expires DATETIME);");
    run(ex, "INSERT INTO emp VALUES (1, 'Alice', 30, '2099-01-01 00:00:00');");
    run(ex, "INSERT INTO emp VALUES (2, 'Bob',   25, '2099-01-01 00:00:00');");
    run(ex, "INSERT INTO emp VALUES (3, 'Carol', 35, '2099-01-01 00:00:00');");

    int cnt = run_count(ex, "SELECT * FROM emp;");
    assert(cnt == 3);
    std::cout << "[PASS] CREATE + INSERT + SELECT * (" << cnt << " rows)\n";
}

static void test_where_clause() {
    Executor ex(DATA_DIR);
    // Reuse table from previous test (executor re-opens .fql files)
    int cnt = run_count(ex, "SELECT * FROM emp WHERE id = 2;");
    assert(cnt == 1);
    std::cout << "[PASS] SELECT WHERE id=2 (index scan)\n";
}

static void test_expiry() {
    Executor ex(DATA_DIR);
    run(ex, "INSERT INTO emp VALUES (99, 'Ghost', 50, '2000-01-01 00:00:00');");
    // Ghost row has expired timestamp → should not appear
    int cnt = run_count(ex, "SELECT * FROM emp WHERE id = 99;");
    assert(cnt == 0);
    std::cout << "[PASS] expired row filtered (expiry check)\n";
}

static void test_inner_join() {
    Executor ex(DATA_DIR);
    run(ex, "CREATE TABLE dept (did INT, emp_id INT, dname CHAR(32), expires DATETIME);");
    run(ex, "INSERT INTO dept VALUES (10, 1, 'Engineering', '2099-01-01 00:00:00');");
    run(ex, "INSERT INTO dept VALUES (20, 2, 'Marketing',   '2099-01-01 00:00:00');");

    int cnt = run_count(ex, "SELECT * FROM emp INNER JOIN dept ON emp.id = dept.emp_id;");
    assert(cnt == 2);
    std::cout << "[PASS] INNER JOIN returns " << cnt << " matching rows\n";
}

int main() {
    fs::remove_all(DATA_DIR);
    test_create_insert_select();
    test_where_clause();
    test_expiry();
    test_inner_join();
    std::cout << "\nAll executor tests PASSED.\n";
    return 0;
}
