#include "executor/hash_join.h"
#include "storage/column_reader.h"
#include <cstring>

std::vector<Row> HashJoin::join(Table* t1, Table* t2, const std::string& col1, const std::string& col2) {
    int c1_idx = -1;
    for (size_t i = 0; i < t1->schema().columns.size(); i++) {
        if (std::string(t1->schema().columns[i].name) == col1) { c1_idx = i; break; }
    }
    int c2_idx = -1;
    for (size_t i = 0; i < t2->schema().columns.size(); i++) {
        if (std::string(t2->schema().columns[i].name) == col2) { c2_idx = i; break; }
    }
    if (c1_idx == -1 || c2_idx == -1) return {};

    std::vector<Row> result;
    std::unordered_map<uint64_t, std::vector<RID>> hash_table;

    // Build phase (t2)
    uint64_t total2 = t2->get_column(c2_idx)->get_total_rows();
    ColumnReader r2(t2->get_column(c2_idx)->get_pager(), t2->get_column(c2_idx)->get_cache());
    for (RID rid = 0; rid < total2; rid++) {
        std::vector<uint8_t> buf;
        uint64_t val = 0;
        if (r2.read_at(rid, buf, 8)) {
            memcpy(&val, buf.data(), 8);
            hash_table[val].push_back(rid);
        }
    }

    // Probe phase (t1)
    uint64_t total1 = t1->get_column(c1_idx)->get_total_rows();
    ColumnReader r1(t1->get_column(c1_idx)->get_pager(), t1->get_column(c1_idx)->get_cache());
    for (RID rid = 0; rid < total1; rid++) {
        std::vector<uint8_t> buf;
        uint64_t val = 0;
        if (r1.read_at(rid, buf, 8)) {
            memcpy(&val, buf.data(), 8);
            if (hash_table.count(val)) {
                // Materialize t1 row
                Row row1;
                for (size_t c = 0; c < t1->schema().columns.size(); c++) {
                    ColumnReader cr(t1->get_column(c)->get_pager(), t1->get_column(c)->get_cache());
                    std::vector<uint8_t> cbuf;
                    Value cv; cv.type = t1->schema().columns[c].type;
                    uint16_t fsz = (cv.type == ColumnType::VARCHAR) ? 128 : 8;
                    if (cr.read_at(rid, cbuf, fsz)) {
                        if (cv.type == ColumnType::VARCHAR) {
                            cbuf.push_back(0); // SAFETY
                            cv.str = std::string((char*)cbuf.data());
                        } else memcpy(&cv.i64, cbuf.data(), 8);
                    }
                    row1.values.push_back(cv);
                }
                
                for (RID rid2 : hash_table[val]) {
                    Row joined = row1;
                    // Materialize t2 row
                    for (size_t c = 0; c < t2->schema().columns.size(); c++) {
                        ColumnReader cr(t2->get_column(c)->get_pager(), t2->get_column(c)->get_cache());
                        std::vector<uint8_t> cbuf;
                        Value cv; cv.type = t2->schema().columns[c].type;
                        uint16_t fsz = (cv.type == ColumnType::VARCHAR) ? 128 : 8;
                        if (cr.read_at(rid2, cbuf, fsz)) {
                            if (cv.type == ColumnType::VARCHAR) {
                                cbuf.push_back(0); // SAFETY
                                cv.str = std::string((char*)cbuf.data());
                            } else memcpy(&cv.i64, cbuf.data(), 8);
                        }
                        joined.values.push_back(cv);
                    }
                    result.push_back(joined);
                }
            }
        }
    }

    return result;
}