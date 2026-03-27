#include "optimizer/optimizer.h"

ExecutionPlan Optimizer::optimize(const Query& query) {
    ExecutionPlan plan;

    plan.use_where = query.has_where;
    plan.use_join = query.has_join;

    // =========================
    // JOIN dominates
    // =========================
    if (query.has_join) {
        plan.type = PlanType::HASH_JOIN;
        return plan;
    }

    // =========================
    // WHERE clause
    // =========================
    if (query.has_where) {
        // assume primary key indexed
        if (query.where.column == "id") {
            plan.type = PlanType::INDEX_SCAN;
            plan.index_name = "id";
        } else {
            plan.type = PlanType::FULL_SCAN;
        }
        return plan;
    }

    // =========================
    // Default
    // =========================
    plan.type = PlanType::FULL_SCAN;
    return plan;
}