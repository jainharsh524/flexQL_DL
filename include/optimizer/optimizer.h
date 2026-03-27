#pragma once

#include "parser/query.h"
#include <string>

enum class PlanType {
    FULL_SCAN,
    INDEX_SCAN,
    HASH_JOIN
};

struct ExecutionPlan {
    PlanType type;

    std::string index_name;
    bool use_where;
    bool use_join;
};

class Optimizer {
public:
    static ExecutionPlan optimize(const Query& query);
};