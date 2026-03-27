#include "execution/join.h"

std::vector<std::pair<uint64_t, uint64_t>>
JoinExecutor::nested_loop_join(const std::vector<uint64_t>& left,
                               const std::vector<uint64_t>& right) {

    std::vector<std::pair<uint64_t, uint64_t>> result;

    for (auto l : left) {
        for (auto r : right) {
            if (l == r) { // placeholder condition
                result.emplace_back(l, r);
            }
        }
    }

    return result;
}