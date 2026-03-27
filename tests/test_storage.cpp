#include "storage/pager.h"
#include "storage/row.h"
#include "storage/table.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

static void test_page_init() {
    Page pg;
    pg.init(42);
    assert(pg.header()->page_id == 42);
    assert(pg.header()->num_slots == 0);
    assert(pg.free_bytes() == PAGE_SIZE - sizeof(PageHeader));
    std::cout << "[PASS] page init\n";
}

static void test_page_insert() {
    Page pg;
    pg.init(0);
    uint8_t buf[64] = {};
    buf[0] = 0xAB;
    uint16_t slot = pg.insert_row(buf, 64);
    assert(slot == 0);
    assert(pg.header()->num_slots == 1);
    assert(pg.slot(0)->length == 64);
    assert(pg.row_data(pg.slot(0)->offset)[0] == 0xAB);
    std::cout << "[PASS] page insert row\n";
}

static void test_row_serialize() {
    Schema schema;
    ColumnDef cd1{}; std::strcpy(cd1.name,"id");   cd1.type=ColumnType::INT;     cd1.length=4;
    ColumnDef cd2{}; std::strcpy(cd2.name,"name"); cd2.type=ColumnType::CHAR;    cd2.length=32;
    ColumnDef cd3{}; std::strcpy(cd3.name,"score"); cd3.type=ColumnType::FLOAT;  cd3.length=4;
    ColumnDef cd4{}; std::strcpy(cd4.name,"exp");  cd4.type=ColumnType::DATETIME; cd4.length=8;
    schema.columns = {cd1, cd2, cd3, cd4};
    schema.expiry_col_idx = 3;
    schema.finalize();

    Row row;
    row.values.push_back(Value::from_int(1));
    row.values.push_back(Value::from_string("Alice", ColumnType::CHAR));
    row.values.push_back(Value::from_float(99.5f));
    row.values.push_back(Value::from_datetime(2000000000LL));  // Far future

    uint8_t buf[1024];
    uint16_t len = row.serialize(schema, buf, sizeof(buf));
    assert(len == 4 + 32 + 4 + 8);

    Row row2;
    bool ok = row2.deserialize(schema, buf, len);
    assert(ok);
    assert(row2.values[0].i32 == 1);
    assert(row2.values[1].str == "Alice");
    assert(row2.values[2].f32 == 99.5f);
    assert(!row2.is_expired(schema));
    std::cout << "[PASS] row serialize/deserialize\n";
}

static void test_table_insert_scan() {
    fs::remove_all("/tmp/fql_test_table");
    Schema schema;
    ColumnDef cd1{}; std::strcpy(cd1.name,"id"); cd1.type=ColumnType::INT; cd1.length=4;
    ColumnDef cd2{}; std::strcpy(cd2.name,"val"); cd2.type=ColumnType::FLOAT; cd2.length=4;
    schema.columns = {cd1, cd2};
    schema.finalize();

    Table tbl("/tmp/fql_test_table", "t1", schema);
    for (int i = 0; i < 100; ++i) {
        Row row;
        row.values.push_back(Value::from_int(i));
        row.values.push_back(Value::from_float(static_cast<float>(i) * 1.5f));
        tbl.insert_row(row);
    }

    int count = 0;
    tbl.scan([&](const Row&, RID){ ++count; return true; });
    assert(count == 100);
    std::cout << "[PASS] table insert+scan (" << count << " rows)\n";
}

int main() {
    test_page_init();
    test_page_insert();
    test_row_serialize();
    test_table_insert_scan();
    std::cout << "\nAll storage tests PASSED.\n";
    return 0;
}
