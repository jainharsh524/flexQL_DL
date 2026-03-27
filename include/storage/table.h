#pragma once
#include "storage/schema.h"
#include "storage/column_store.h"
#include "index/index_manager.h"
#include "storage/row.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>

class Table {
public:
    Table(const std::string& data_dir, const std::string& name, const Schema& schema);
    ~Table() = default;

    void insert_row(const Row& row);
    void clear();
    void flush_all();
    
    const Schema& schema() const { return schema_; }
    ColumnStore* get_column(size_t index);
    IndexManager* get_index() { return index_manager_.get(); }
    
    void scan(const std::function<bool(const Row&, RID)>& cb);
    bool fetch_row(RID rid, Row& out);

private:
    std::string name_;
    Schema schema_;
    std::vector<std::unique_ptr<ColumnStore>> columns_;
    std::unique_ptr<IndexManager> index_manager_;
};