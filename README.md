# FlexQL — Benchmark Results & Implementation Details

## Environment
- **Platform**: macOS (Apple Silicon)
- **Server**: `127.0.0.1:9000`
- **Data dir**: `./data`
- **Build flags**: `g++ -std=c++17 -O2`

---

## Unit Tests — 22/22 Passed 

```
[PASS] CREATE TABLE TEST_USERS         [PASS] Single-row value validation
[PASS] RESET TEST_USERS                [PASS] Filtered rows validation
[PASS] INSERT TEST_USERS               [PASS] ORDER BY descending validation
[PASS] CREATE TABLE TEST_ORDERS        [PASS] Empty result-set validation
[PASS] RESET TEST_ORDERS               [PASS] Join result validation
[PASS] INSERT TEST_ORDERS              [PASS] Single-condition equality WHERE
                                       [PASS] Join with no matches validation
                                       [PASS] Invalid SQL should fail
                                       [PASS] Missing table should fail

Unit Test Summary: 22/22 passed, 0 failed.
```

---

## Insertion Benchmark

| Scale | Rows Inserted | Elapsed | Throughput |
|-------|--------------|---------|-----------|
| **1M rows** | 1,000,000 | 3,367 ms | **297,000 rows/sec** |
| **10M rows** | 10,000,000 | 34,452 ms | **290,258 rows/sec** |

> Throughput is consistent across scales (~290–297k rows/sec), demonstrating the stability of the mmap-backed columnar storage design under sustained bulk load.

---

## Master Benchmark (4 Phases, 1M scale)

| Phase | What It Tests | Throughput |
|-------|--------------|-----------|
| Phase 1 | Pure insert throughput (8 threads, batch=500) | ~260–300k rows/sec |
| Phase 2 | Mixed: inserts + concurrent point-reads | ~320–350k inserts/sec |
| Phase 3 | Batched PK point queries (8 threads, batch=200) | ~500–550k queries/sec |
| Phase 4 | INNER JOIN (100k employees × 100 departments) | ~6–8 JOINs/sec |

---

## Architecture Overview

```
┌─────────────────────────────────────┐
│         Client (flexql.h / TCP)     │
└───────────────┬─────────────────────┘
                │  length-prefixed frames [uint32_t len][sql bytes]
┌───────────────▼─────────────────────┐
│  Server  ──  Thread Pool (N CPUs)   │  src/server/
│  handle_client() per connection     │
└───────────────┬─────────────────────┘
                │
┌───────────────▼─────────────────────┐
│  Parser  →  Optimizer  →  Executor  │  src/parser/ optimizer/ executor/
└─────┬──────────────┬────────────────┘
      │              │
┌─────▼──┐   ┌───────▼──────────────────┐
│ B+Tree │   │  Table / Column Storage  │ src/index/ storage/
│ Index  │   │  Pager (mmap 1 GB file)  │
│(4MB    │   │  LRU Page Cache          │ src/cache/
│ cache) │   └──────────────────────────┘
└────────┘
```

---

## Implementation Details

### 1. Storage Engine — `src/storage/`

- **Columnar layout**: one memory-mapped file per column (e.g., `ID.col`, `NAME.col`).
- **Pager** pre-allocates 1 GB via `ftruncate` + `mmap(MAP_SHARED)` — no remapping during inserts, pointers stay stable.
- **Page access**: O(1) direct calculation `page_id = rid / items_per_page`.
- **Flush**: `msync(MS_SYNC)` on close for durability.

### 2. B+ Tree Index — `src/index/btree.cpp`

- Hand-rolled B+ Tree on top of the pager.
- **4 MB LRU page cache** (1,024 pages) for hot node access.
- Leaf nodes linked in a list for future range scans.
- Automatic leaf split when `num_keys ≥ BPTREE_ORDER`.
- Complexity: O(log N) insert, O(log N) point lookup.

### 3. LRU Cache — `src/cache/lru_cache.cpp`

- Thread-safe doubly-linked list + `unordered_map`, guarded by `std::mutex`.
- Dirty pages written via `pager.flush()` (msync) on eviction.
- mmap-backed pages are **not** `delete`d — OS manages physical memory.

### 4. SQL Parser — `src/parser/parser.cpp`

Recursive-descent parser supporting:
- `CREATE TABLE … (col TYPE [PRIMARY KEY], …)`
- `INSERT INTO … VALUES (…), (…)` — multi-row batches in a single call
- `SELECT */cols FROM … [WHERE col op val] [ORDER BY col ASC|DESC]`
- `SELECT … FROM t1 INNER JOIN t2 ON t1.col = t2.col`
- `DELETE FROM …` (full-table clear)

### 5. Query Optimizer — `src/optimizer/optimizer.cpp`

Rule-based planner (no cost model):

| Condition | Plan |
|-----------|------|
| `WHERE ID = ?` | INDEX_SCAN (B+ Tree, O(log N)) |
| `INNER JOIN` | HASH_JOIN |
| Everything else | FULL_SCAN |

### 6. Executor — `src/executor/executor.cpp`

- **Index scan**: B+ Tree lookup → `materialize_row()` — reads only the target page.
- **Hash join** (`hash_join.h`): builds hash map on right table, probes from left — single-pass, in-memory.
- **Filter**: post-scan WHERE evaluation (`=`, `>`, `>=`).
- **Sort**: `std::sort` with column-aware comparator for ORDER BY.
- **Callback streaming**: results passed row-by-row via `ExecCallback` — no intermediate heap buffer.

### 7. Server & Thread Pool — `src/server/`

- `ThreadPool` auto-sizes to `hardware_concurrency()` workers.
- Each TCP connection dispatched as a task; handler loops on `while(true)` reading frames.
- Multi-statement batches (newline-separated SQL) executed in one round-trip.
- Wire: `[uint32_t length][sql]` → `[uint32_t length][OK\n | ERR:msg\n | tab-delimited rows]`.

### 8. Client Library — `src/client/flexql.cpp`

Public C API:
```c
int  flexql_open (const char* host, int port, FlexQL** db);
int  flexql_exec (FlexQL* db, const char* sql,
                  int (*cb)(void*, int, char**, char**),
                  void* arg, char** errmsg);
int  flexql_close(FlexQL* db);
void flexql_free (void* ptr);
```

---

## How to Run

```bash
# Build
make clean && make server benchmarks -j4

# Start server
rm -rf data && mkdir -p data
./bin/flexql_server --port 9000 &
sleep 3

# Unit tests
./bin/flexql_benchmark_flexql --unit-test

# Insertion benchmarks
./bin/flexql_benchmark_flexql 1000000    # 1M rows
./bin/flexql_benchmark_flexql 10000000   # 10M rows

# Stop server
pkill -9 flexql_server
```

```bash
# Master benchmark (all 4 phases)
lsof -ti :9123 | xargs kill -9 || true
rm -rf data_bench && mkdir -p data_bench
./bin/flexql_server --port 9123 --data-dir ./data_bench &
sleep 2
./bin/flexql_master_benchmark 1000000 1000000
pkill -f flexql_server
```
