#include "index/index_manager.h"

void IndexManager::create_index(const std::string& name, const std::string& file) {
    indexes[name] = std::make_unique<BPTree>(file);
}

BPTree* IndexManager::get_index(const std::string& name) {
    if (indexes.find(name) == indexes.end())
        return nullptr;

    return indexes[name].get();
}