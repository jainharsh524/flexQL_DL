#include "executor/executor.h"
#include "parser/parser.h"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <string>

namespace fs    = std::filesystem;
namespace chrono = std::chrono;

static std::string DATA_DIR = "/tmp/fql_benchmark";

// ─── Timing helper ─────────────────────────────────────────────────────────
struct Timer {
    chrono::steady_clock::time_point t0 = chrono::steady_clock::now();
    double elapsed_ms() const {
        auto t1 = chrono::steady_clock::now();
        return chrono::duration<double, std::milli>(t1 - t0).count();
    }
};

static void bench_insert(Executor& ex, int N) {
    // Pre-build SQL strings in batch chunks
    // We call exec_insert N times to measure per-row overhead.
    std::cout << "Inserting " << N << " rows...\n";
    Timer t;
    for (int i = 0; i < N; ++i) {
        std::string sql = "INSERT INTO bench VALUES (" +
                          std::to_string(i) + ", 'User" + std::to_string(i) +
                          "', " + std::to_string(i % 100) +
                          ", '2099-01-01 00:00:00');";
        Parser p;
        Statement stmt = p.parse(sql);
        std::string err;
        ex.exec(stmt, nullptr, nullptr, err);
    }
    double ms = t.elapsed_ms();
    std::cout << "  INSERT: " << N << " rows in " << ms << " ms  ("
              << static_cast<int>(N / (ms / 1000.0)) << " rows/sec)\n";
}

static int count_rows_cb(void* arg, int, char**, char**) {
    (*static_cast<int*>(arg))++;
    return 0;
}

static void bench_scan(Executor& ex, int N) {
    std::cout << "Full-table scan (" << N << " rows)...\n";
    Parser p;
    Statement stmt = p.parse("SELECT * FROM bench;");
    std::string err;
    int count = 0;
    Timer t;
    ex.exec(stmt, count_rows_cb, &count, err);
    double ms = t.elapsed_ms();
    std::cout << "  SCAN: " << count << " rows in " << ms << " ms  ("
              << static_cast<int>(count / (ms / 1000.0)) << " rows/sec)\n";
}

static void bench_index_lookup(Executor& ex) {
    std::cout << "Index lookup (PK = 500000)...\n";
    Parser p;
    Statement stmt = p.parse("SELECT * FROM bench WHERE id = 500000;");
    std::string err;
    int count = 0;
    Timer t;
    ex.exec(stmt, count_rows_cb, &count, err);
    double ms = t.elapsed_ms();
    std::cout << "  INDEX SCAN: found=" << count
              << " in " << ms << " ms\n";
}

int main(int argc, char* argv[]) {
    int N = 1'000'000;  // Default 1M rows; set via arg for 10M
    if (argc > 1) N = std::stoi(argv[1]);

    fs::remove_all(DATA_DIR);
    Executor ex(DATA_DIR);

    {
        Parser p;
        Statement stmt = p.parse(
            "CREATE TABLE bench (id INT, name CHAR(32), age INT, expires DATETIME);");
        std::string err;
        ex.exec(stmt, nullptr, nullptr, err);
    }

    bench_insert(ex, N);
    bench_scan(ex, N);
    bench_index_lookup(ex);

    std::cout << "\nBenchmark complete.\n";
    return 0;
}
