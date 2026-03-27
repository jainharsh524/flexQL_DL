#include "index/btree.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

static void test_basic_insert_search() {
    fs::remove("/tmp/fql_test.idx");
    BPlusTree tree("/tmp/fql_test.idx");

    // Insert 1000 keys
    for (int i = 0; i < 1000; ++i) {
        tree.insert(i, RID{static_cast<uint32_t>(i), 0, 0});
    }

    // Exact search for each
    for (int i = 0; i < 1000; ++i) {
        RID rid;
        bool found = tree.search(i, rid);
        assert(found);
        assert(rid.page_id == static_cast<uint32_t>(i));
    }
    std::cout << "[PASS] B+ tree insert + exact search (1000 keys)\n";
}

static void test_range_scan() {
    fs::remove("/tmp/fql_range.idx");
    BPlusTree tree("/tmp/fql_range.idx");

    for (int i = 0; i < 500; ++i)
        tree.insert(i * 2, RID{static_cast<uint32_t>(i), 0, 0});  // Even numbers

    std::vector<int32_t> found_keys;
    tree.range_scan(100, 200, [&](int32_t k, RID) -> bool {
        found_keys.push_back(k);
        return true;
    });

    // Expected: 100, 102, ... 200  = 51 keys
    assert(found_keys.size() == 51);
    assert(found_keys.front() == 100);
    assert(found_keys.back() == 200);
    std::cout << "[PASS] B+ tree range scan [100,200] (" << found_keys.size() << " hits)\n";
}

static void test_persistence() {
    fs::remove("/tmp/fql_persist.idx");
    {
        BPlusTree tree("/tmp/fql_persist.idx");
        for (int i = 0; i < 200; ++i)
            tree.insert(i, RID{static_cast<uint32_t>(i), 0, 0});
        tree.sync();
    }
    {
        BPlusTree tree("/tmp/fql_persist.idx");
        RID rid;
        assert(tree.search(100, rid));
        assert(rid.page_id == 100);
        assert(tree.search(199, rid));
    }
    std::cout << "[PASS] B+ tree persistence across reopen\n";
}

int main() {
    test_basic_insert_search();
    test_range_scan();
    test_persistence();
    std::cout << "\nAll B+ tree tests PASSED.\n";
    return 0;
}
