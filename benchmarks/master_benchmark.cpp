// ============================================================
//  FlexQL Master Benchmark
//  Runs all performance phases against a live FlexQL server
//  and prints a consolidated results table.
//
//  Usage: ./bin/flexql_master_benchmark [ROW_COUNT] [QUERY_COUNT]
//    ROW_COUNT   – rows to insert for each table phase (default 10,000,000)
//    QUERY_COUNT – point queries to fire in the query phase (default 10,000,000)
// ============================================================

#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <sstream>
#include "flexql.h"

// ── Helpers ─────────────────────────────────────────────────

static int null_cb(void*, int, char**, char**) { return 0; }

using Clock = std::chrono::high_resolution_clock;
using Sec   = std::chrono::duration<double>;

struct Result {
    std::string phase;
    std::string metric;
    double      value;
    std::string unit;
    std::string note;
};

static void check(int rc, const char* ctx) {
    if (rc != FLEXQL_OK) {
        std::cerr << "[FATAL] " << ctx << " (rc=" << rc << ")\n";
        std::exit(1);
    }
}

// Reusable concurrent worker helper
static void run_concurrent(int N, int num_threads, std::function<void(int start, int end)> work) {
    std::vector<std::thread> workers;
    int chunk = std::max(1, N / num_threads);
    for (int t = 0; t < num_threads; t++) {
        int start = t * chunk;
        if (start >= N) break;
        int end = (t == num_threads - 1) ? N : start + chunk;
        workers.push_back(std::thread(work, start, end));
    }
    for (auto& w : workers) w.join();
}

static void concurrent_batch_insert(const std::string& table,
                                    const std::string& cols,
                                    const std::vector<std::string>& rows,
                                    int batch_size = 5000,
                                    int num_threads = 8) {
    run_concurrent(rows.size(), num_threads, [&](int start, int end) {
        FlexQL* local_db = nullptr;
        flexql_open("127.0.0.1", 5433, &local_db);
        char* err = nullptr;
        std::string buf;
        buf.reserve(batch_size * 64);
        
        for (int i = start; i < end; i++) {
            buf += "INSERT INTO " + table + " " + cols + " VALUES " + rows[i] + ";\n";
            if ((i + 1 - start) % batch_size == 0 || i == end - 1) {
                flexql_exec(local_db, buf.c_str(), nullptr, nullptr, &err);
                buf.clear();
            }
        }
        flexql_close(local_db);
    });
}

// ── Banner ───────────────────────────────────────────────────

static void banner() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║       FlexQL Master Benchmark Suite                 ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n\n";
}

static void section(const std::string& title) {
    std::cout << "\n── " << title << " ";
    for (int i = (int)title.size() + 4; i < 56; i++) std::cout << "─";
    std::cout << "\n";
}

// ── Phase 1: Pure Insert Throughput ─────────────────────────

static Result phase_insert(FlexQL* db, int N) {
    section("Phase 1 · Pure Insert Throughput");
    char* err = nullptr;
    flexql_exec(db, "DROP TABLE IF EXISTS P1;", nullptr, nullptr, &err);
    flexql_exec(db, "CREATE TABLE P1 (ID INT PRIMARY KEY, VAL VARCHAR);",
                nullptr, nullptr, &err);

    std::vector<std::string> vals;
    vals.reserve(N);
    for (int i = 0; i < N; i++)
        vals.push_back("(" + std::to_string(i) + ",'v" + std::to_string(i) + "')");

    std::cout << "  Inserting " << N << " rows...\n";
    auto t0 = Clock::now();
    concurrent_batch_insert("P1", "", vals, 500, 8);
    double elapsed = Sec(Clock::now() - t0).count();
    double tput = N / elapsed;

    std::cout << "  ✓ " << std::fixed << std::setprecision(0)
              << tput << " rows/sec  (" << std::setprecision(2)
              << elapsed << "s)\n";
    return {"Insert", "Throughput", tput, "rows/sec",
            std::to_string(N / 1000000) + "M rows, batch=500"};
}

// ── Phase 2: Mixed Workload (writes + concurrent reads) ──────

static Result phase_mixed(FlexQL* db, int N) {
    section("Phase 2 · Mixed Workload  (inserts + concurrent point reads)");
    char* err = nullptr;
    flexql_exec(db, "DROP TABLE IF EXISTS P2;", nullptr, nullptr, &err);
    flexql_exec(db, "CREATE TABLE P2 (ID INT PRIMARY KEY);",
                nullptr, nullptr, &err);

    std::atomic<bool> inserting{true};
    std::atomic<int>  read_ops{0};

    // Concurrent reader — point queries
    std::thread reader([&]() {
        FlexQL* rdb = nullptr;
        flexql_open("127.0.0.1", 5433, &rdb);
        int probe = 0;
        while (inserting.load()) {
            std::string q = "SELECT * FROM P2 WHERE ID = "
                          + std::to_string(probe) + ";";
            flexql_exec(rdb, q.c_str(), null_cb, nullptr, nullptr);
            probe = (probe + 997) % std::max(1, std::min(N, 1000000));
            read_ops++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        flexql_close(rdb);
    });

    std::cout << "  Inserting " << N << " rows with concurrent reader...\n";
    auto t0 = Clock::now();
    
    run_concurrent(N, 8, [&](int start, int end) {
        FlexQL* ldb = nullptr;
        flexql_open("127.0.0.1", 5433, &ldb);
        char* err = nullptr;
        std::string row_batch;
        row_batch.reserve(500 * 32);
        for (int i = start; i < end; i++) {
            row_batch += "INSERT INTO P2 VALUES (" + std::to_string(i) + ");\n";
            if ((i + 1 - start) % 5000 == 0 || i == end - 1) {
                flexql_exec(ldb, row_batch.c_str(), nullptr, nullptr, &err);
                row_batch.clear();
            }
        }
        flexql_close(ldb);
    });

    inserting.store(false);
    reader.join();
    double elapsed = Sec(Clock::now() - t0).count();
    double tput = N / elapsed;

    std::cout << "  ✓ " << std::fixed << std::setprecision(0)
              << tput << " inserts/sec  (" << std::setprecision(2)
              << elapsed << "s)  concurrent reads: " << read_ops.load() << "\n";
    return {"Mixed", "Insert throughput under reads", tput, "rows/sec",
            std::to_string(read_ops.load()) + " concurrent reads"};
}

// ── Phase 3: Point Query Throughput (index lookups) ──────────

static Result phase_query(FlexQL* db, int N, int Q) {
    section("Phase 3 · Point Query Throughput  (PK index lookups)");
    char* err = nullptr;
    flexql_exec(db, "DROP TABLE IF EXISTS P3;", nullptr, nullptr, &err);
    flexql_exec(db, "CREATE TABLE P3 (ID INT PRIMARY KEY, VAL VARCHAR);",
                nullptr, nullptr, &err);

    // Pre-fill
    std::cout << "  Pre-filling " << N << " rows...\n";
    std::vector<std::string> vals;
    vals.reserve(N);
    for (int i = 0; i < N; i++)
        vals.push_back("(" + std::to_string(i) + ",'Val" + std::to_string(i) + "')");
    concurrent_batch_insert("P3", "", vals, 1000, 8);

    // Query phase
    std::cout << "  Firing " << Q << " batched point queries...\n";
    auto t0 = Clock::now();
    
    run_concurrent(Q, 8, [&](int start, int end) {
        FlexQL* ldb = nullptr;
        flexql_open("127.0.0.1", 5433, &ldb);
        char* err = nullptr;
        std::string batch;
        batch.reserve(200 * 50);
        for (int i = start; i < end; i++) {
            int id = i % N;
            batch += "SELECT * FROM P3 WHERE ID = " + std::to_string(id) + ";\n";
            if ((i + 1 - start) % 500 == 0 || i == end - 1) {
                flexql_exec(ldb, batch.c_str(), null_cb, nullptr, &err);
                batch.clear();
            }
        }
        flexql_close(ldb);
    });

    double elapsed = Sec(Clock::now() - t0).count();
    double tput    = Q / elapsed;
    double lat_us  = elapsed * 1e6 / Q;

    std::cout << "  ✓ " << std::fixed << std::setprecision(0)
              << tput << " queries/sec  lat=" << std::setprecision(2)
              << lat_us << "µs  (" << elapsed << "s)\n";
    return {"Point Query", "Throughput", tput, "queries/sec",
            std::to_string(Q / 1000000) + "M queries, lat=" + std::to_string((int)lat_us) + "us"};
}

// ── Phase 4: INNER JOIN  (hash join) ─────────────────────────

static Result phase_join(FlexQL* db, int emp_count, int dept_count, int iters) {
    section("Phase 4 · INNER JOIN Performance  (hash join)");
    char* err = nullptr;
    flexql_exec(db, "DROP TABLE IF EXISTS DEPTS2;", nullptr, nullptr, &err);
    flexql_exec(db, "DROP TABLE IF EXISTS EMPS2;", nullptr, nullptr, &err);
    flexql_exec(db, "CREATE TABLE DEPTS2 (ID INT PRIMARY KEY, DNAME VARCHAR);",
                nullptr, nullptr, &err);
    flexql_exec(db, "CREATE TABLE EMPS2  (ID INT PRIMARY KEY, DEPTID INT, ENAME VARCHAR);",
                nullptr, nullptr, &err);

    // Fill depts
    std::vector<std::string> dv;
    dv.reserve(dept_count);
    for (int i = 0; i < dept_count; i++)
        dv.push_back("(" + std::to_string(i) + ",'Dept" + std::to_string(i) + "')");
    std::cout << "  Pre-filling " << dept_count << " departments...\n";
    concurrent_batch_insert("DEPTS2", "", dv, 500, 4);

    // Fill emps
    std::vector<std::string> ev;
    ev.reserve(emp_count);
    for (int i = 0; i < emp_count; i++)
        ev.push_back("(" + std::to_string(i) + ","
                     + std::to_string(i % dept_count)
                     + ",'Emp"  + std::to_string(i) + "')");
    std::cout << "  Pre-filling " << emp_count << " employees...\n";
    concurrent_batch_insert("EMPS2", "", ev, 1000, 8);

    std::string jq = "SELECT EMPS2.ENAME, DEPTS2.DNAME "
                     "FROM EMPS2 INNER JOIN DEPTS2 ON EMPS2.DEPTID = DEPTS2.ID;";

    std::cout << "  Executing " << iters << " JOIN queries...\n";
    auto t0 = Clock::now();
    for (int i = 0; i < iters; i++)
        flexql_exec(db, jq.c_str(), null_cb, nullptr, &err);
    double elapsed = Sec(Clock::now() - t0).count();
    double tput    = iters / elapsed;
    double lat_ms  = elapsed * 1000.0 / iters;

    std::cout << "  ✓ " << std::fixed << std::setprecision(3)
              << tput << " JOINs/sec  lat=" << std::setprecision(1)
              << lat_ms << "ms  (" << std::setprecision(2) << elapsed << "s)\n";
    return {"JOIN", "Throughput", tput, "JOINs/sec",
            std::to_string(emp_count/1000) + "K x " + std::to_string(dept_count) + " rows"};
}

// ── Final Summary Table ──────────────────────────────────────

static void print_summary(const std::vector<Result>& results) {
    std::cout << "\n\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                   FlexQL Master Benchmark — Results                         ║\n";
    std::cout << "╠═════════════╦═══════════════════════════════╦═══════════════╦════════════════╣\n";
    std::cout << "║ Phase       ║ Metric                        ║ Value         ║ Notes          ║\n";
    std::cout << "╠═════════════╬═══════════════════════════════╬═══════════════╬════════════════╣\n";
    for (const auto& r : results) {
        std::ostringstream val;
        if (r.value > 1000000)
            val << std::fixed << std::setprecision(2) << r.value / 1000000.0 << "M " << r.unit;
        else if (r.value > 1000)
            val << std::fixed << std::setprecision(1) << r.value / 1000.0 << "K " << r.unit;
        else
            val << std::fixed << std::setprecision(2) << r.value << " " << r.unit;

        std::cout << "║ " << std::left << std::setw(12)  << r.phase
                  << "║ "            << std::setw(30) << r.metric
                  << "║ "            << std::setw(14) << val.str()
                  << "║ "            << std::setw(15) << r.note.substr(0, 15)
                  << "║\n";
    }
    std::cout << "╚═════════════╩═══════════════════════════════╩═══════════════╩════════════════╝\n\n";
}

// ── Main ─────────────────────────────────────────────────────

int main(int argc, char** argv) {
    int ROW_COUNT   = 10'000'000;
    int QUERY_COUNT = 10'000'000;
    if (argc > 1) {
        try { ROW_COUNT = std::stoi(argv[1]); }
        catch (...) { 
            std::cout << "Usage: " << argv[0] << " [ROW_COUNT] [QUERY_COUNT]\n";
            return 0; 
        }
    }
    if (argc > 2) {
        try { QUERY_COUNT = std::stoi(argv[2]); }
        catch (...) {}
    }

    banner();
    std::cout << "  Config: ROW_COUNT=" << ROW_COUNT
              << "  QUERY_COUNT=" << QUERY_COUNT << "\n";
    std::cout << "  Server : 127.0.0.1:5433\n";

    // Single persistent connection for non-threaded phases
    FlexQL* db = nullptr;
    check(flexql_open("127.0.0.1", 5433, &db), "connect to FlexQL server");
    std::cout << "  ✓ Connected\n";

    std::vector<Result> results;

    auto t_total = Clock::now();

    results.push_back(phase_insert(db, ROW_COUNT));
    results.push_back(phase_mixed (db, ROW_COUNT));
    results.push_back(phase_query (db, ROW_COUNT, QUERY_COUNT));

    // JOIN: 100K employees × 100 depts, 10 iterations
    int emp   = std::min(ROW_COUNT, 100'000);
    int dept  = 100;
    int iters = 10;
    results.push_back(phase_join(db, emp, dept, iters));

    double total_elapsed = Sec(Clock::now() - t_total).count();

    flexql_close(db);

    print_summary(results);
    std::cout << "  Total wall time: " << std::fixed << std::setprecision(1)
              << total_elapsed << "s\n\n";
    return 0;
}
