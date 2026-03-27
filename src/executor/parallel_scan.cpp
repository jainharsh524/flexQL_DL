#include "execution/parallel_scan.h"
#include <thread>

std::vector<uint64_t> ParallelScanner::scan(
    ColumnReader& reader,
    int num_threads
) {
    uint64_t total = reader.get_total_rows();

    std::vector<std::vector<uint64_t>> partial(num_threads);
    std::vector<std::thread> threads;

    uint64_t chunk = total / num_threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            uint64_t start = t * chunk;
            uint64_t end = (t == num_threads - 1) ? total : start + chunk;

            for (uint64_t i = start; i < end; i++) {
                partial[t].push_back(i);
            }
        });
    }

    for (auto& th : threads) th.join();

    std::vector<uint64_t> result;

    for (auto& vec : partial) {
        result.insert(result.end(), vec.begin(), vec.end());
    }

    return result;
}