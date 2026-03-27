#pragma once
#include "storage/row.h"
#include <string>
#include <vector>

// TableSchema is now merged into the main Schema struct in row.h
// This alias is for compatibility with the columnar stores that use TableSchema.
using TableSchema = Schema;