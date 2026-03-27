#pragma once

#include "bptree.h"
#include <unordered_map>
#include <memory>

class IndexManager {
private:
    std::unordered_map<std::string, std::unique_ptr<BPTree>> indexes;

public:
    void create_index(const std::string& name, const std::string& file);

    BPTree* get_index(const std::string& name);
};