#pragma once
#include "storage/schema.h"
#include "storage/row.h"
#include <string>
#include <vector>

class Serializer {
public:
    static int32_t encode_int(const std::string& val);
    static int64_t encode_datetime(const std::string& val);
    static std::vector<uint8_t> encode_varchar(const std::string& val);

    static void serialize_row(
        const Schema& schema,
        const Row& row,
        std::vector<std::vector<uint8_t>>& column_data
    );
};