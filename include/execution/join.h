#pragma once

#include <vector>
#include <cstdint>

class JoinExecutor {
public:
    static std::vector<std::pair<uint64_t, uint64_t>>
    nested_loop_join(const std::vector<uint64_t>& left,
                     const std::vector<uint64_t>& right);
};