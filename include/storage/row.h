#pragma once
#include "storage/page.h"
#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

// ─────────────────────────────────────────────────────────────────────────────
// COLUMN TYPES
// ─────────────────────────────────────────────────────────────────────────────
enum class ColumnType : uint8_t {
    INT      = 0,   // 4 bytes, signed 32-bit
    FLOAT    = 1,   // 4 bytes, IEEE 754
    DECIMAL  = 2,   // 8 bytes, stored as int64 (value * 1e6 fixed-point)
    CHAR     = 3,   // Fixed n bytes (null-padded)
    VARCHAR  = 4,   // 2-byte length prefix + up to n bytes
    DATETIME = 5,   // 8 bytes, Unix timestamp (int64)
};

// ─────────────────────────────────────────────────────────────────────────────
// COLUMN DEFINITION
// ─────────────────────────────────────────────────────────────────────────────
struct ColumnDef {
    char     name[32];      // Column name (null-terminated)
    ColumnType type;        // 1 byte
    uint8_t    is_primary;  // 1 byte
    uint16_t   length;      // CHAR(n)/VARCHAR(n) → n; others ignored
    uint32_t   _pad;        // 4 bytes padding to reach 40 bytes total
};
static_assert(sizeof(ColumnDef) == 40, "ColumnDef must be 40 bytes");

// ─────────────────────────────────────────────────────────────────────────────
// SCHEMA  – table-level column descriptors
// On-disk format (page 0 of the table file):
//   [uint16_t num_cols][ColumnDef × num_cols][uint16_t expiry_col_idx]
// ─────────────────────────────────────────────────────────────────────────────
struct Schema {
    std::vector<ColumnDef> columns;
    uint16_t               expiry_col_idx{UINT16_MAX}; // DATETIME col = expiry
    uint16_t               primary_col_idx{0};

    // Fixed serialized size of one row (0 = variable-width)
    uint32_t               fixed_row_size{0};

    // Byte offset of each column within the serialized row buffer
    std::vector<uint16_t>  col_offsets;

    // Compute fixed_row_size and col_offsets; call after building columns.
    void finalize();

    size_t column_count() const { return columns.size(); }

    // Is this schema purely fixed-width?
    bool is_fixed() const { return fixed_row_size > 0; }
};

// ─────────────────────────────────────────────────────────────────────────────
// VALUE  – runtime union for a single column value
// ─────────────────────────────────────────────────────────────────────────────
struct Value {
    ColumnType type;
    union {
        int32_t  i32;
        float    f32;
        int64_t  i64;  // DECIMAL (fixed-point ×1e6) or DATETIME (unix ts)
    };
    std::string str;   // CHAR / VARCHAR

    // Convenience constructors
    static Value from_int(int32_t v)         { Value r; r.type=ColumnType::INT;      r.i32=v; return r; }
    static Value from_float(float v)          { Value r; r.type=ColumnType::FLOAT;    r.f32=v; return r; }
    static Value from_decimal(int64_t v)      { Value r; r.type=ColumnType::DECIMAL;  r.i64=v; return r; }
    static Value from_string(const std::string& s, ColumnType t) {
        Value r; r.type=t; r.str=s; return r;
    }
    static Value from_datetime(int64_t ts)    { Value r; r.type=ColumnType::DATETIME; r.i64=ts; return r; }

    // Compare for WHERE clause evaluation
    bool operator==(const Value& o) const;
    bool operator<(const Value& o)  const;
    bool operator>(const Value& o)  const;
    std::string to_string() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// ROW  – a vector of Values matching a Schema
// ─────────────────────────────────────────────────────────────────────────────
struct Row {
    std::vector<Value> values;

    // Serialize row into buf according to schema.
    // Returns number of bytes written (≤ buf_len), or 0 on error.
    uint16_t serialize(const Schema& schema, uint8_t* buf, uint16_t buf_len) const;

    // Deserialize from buf into this Row.
    bool deserialize(const Schema& schema, const uint8_t* buf, uint16_t buf_len);

    // Returns true if the row's expiry timestamp is in the past.
    bool is_expired(const Schema& schema) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// HELPERS
// ─────────────────────────────────────────────────────────────────────────────

// Parse a "YYYY-MM-DD HH:MM:SS" string into a Unix timestamp.
int64_t parse_datetime(const std::string& s);

// Format a Unix timestamp into "YYYY-MM-DD HH:MM:SS".
std::string format_datetime(int64_t ts);

// Current Unix timestamp (seconds).
inline int64_t now_ts() { return static_cast<int64_t>(std::time(nullptr)); }
