#pragma once
#include "storage/pager.h"
#include "cache/cache_manager.h"
#include <string>
#include <memory>
#include <vector>

enum class BPTNodeType : uint8_t {
    LEAF = 0,
    INTERNAL = 1
};

struct BPTNodeHeader {
    BPTNodeType type;
    uint32_t num_keys;
    uint32_t next_leaf; // 0xFFFFFFFF if none
};

// Max keys per 4KB page: (4096 - 12) / 24 = 170 (approx, with 8-byte key, 8-byte value, 8-byte child)
// For simplicity, let's use 128 as the order.
#define BPT_ORDER 250

struct BPTNode {
    BPTNodeHeader header;
    uint64_t keys[BPT_ORDER];
    union {
        uint64_t values[BPT_ORDER]; // For Leaf
        uint32_t children[BPT_ORDER + 1]; // For Internal
    };
};

class BPTree {
public:
    BPTree(const std::string& filename);
    ~BPTree() = default;

    void insert(uint64_t key, uint64_t row_id);
    bool search(uint64_t key, uint64_t& result);
    void clear();

private:
    uint32_t find_leaf(uint64_t key);
    void insert_into_leaf(uint32_t pid, uint64_t key, uint64_t val);
    void insert_into_parent(uint32_t left_pid, uint64_t key, uint32_t right_pid);
    uint32_t create_node(BPTNodeType type);

    Pager pager_;
    CacheManager cache_manager_;
    uint32_t root_pid_;
};