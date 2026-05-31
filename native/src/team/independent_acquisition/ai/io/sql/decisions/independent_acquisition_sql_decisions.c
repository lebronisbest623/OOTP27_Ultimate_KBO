#include "../independent_acquisition_sql_store_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../../../../core/logging/core_log.h"
#include "../../../../../../core/sql/save_state/save_state_sqlite.h"

int kbo_independent_acquisition_sql_decision_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || seller_team_id == 0u || player_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_decision_exists_schema")) {
        return 0;
    }

    char sql[384] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM independent_acquisition_decisions "
        "WHERE season=%u AND seller_team_id=%u AND player_id=%u LIMIT 1;",
        season,
        seller_team_id,
        player_id);
    KboIndependentAcquisitionSqlExistsResult result = {0};
    kbo_save_state_query(sql, kbo_independent_acquisition_sql_exists_cb, &result, "independent_acquisition_decision_exists");
    return result.found;
}

static int kbo_independent_acquisition_sql_decision_key_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboIndependentAcquisitionSqlDecisionKeyLoadContext* ctx =
        (KboIndependentAcquisitionSqlDecisionKeyLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 4 || ctx->count >= ctx->max_count) {
        return 0;
    }
    KboIndependentAcquisitionDecisionKey row;
    memset(&row, 0, sizeof(row));
    row.season = kbo_independent_acquisition_sql_u32(vals, 0);
    row.seller_team_id = kbo_independent_acquisition_sql_u32(vals, 1);
    row.player_id = kbo_independent_acquisition_sql_u32(vals, 2);
    row.transferred = kbo_independent_acquisition_sql_u32(vals, 3);
    if (row.season != 0u && row.seller_team_id != 0u && row.player_id != 0u) {
        ctx->rows[ctx->count++] = row;
    }
    return 0;
}

int kbo_independent_acquisition_sql_load_decision_keys(
    uint32_t season,
    KboIndependentAcquisitionDecisionKey* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_decision_keys_schema")) {
        return -1;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT season, seller_team_id, player_id, transferred "
        "FROM independent_acquisition_decisions WHERE season=%u ORDER BY id;",
        season);
    KboIndependentAcquisitionSqlDecisionKeyLoadContext ctx = {out, max_count, 0};
    if (!kbo_save_state_query(sql, kbo_independent_acquisition_sql_decision_key_cb, &ctx, "independent_acquisition_decision_keys")) {
        return -1;
    }
    return ctx.count;
}

int kbo_independent_acquisition_sql_load_seller_transfer_summaries(
    uint32_t season,
    KboIndependentAcquisitionTransferSummary* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_seller_summary_schema")) {
        return -1;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT seller_team_id, COUNT(*), MAX(date) "
        "FROM independent_acquisition_decisions "
        "WHERE season=%u AND transferred<>0 "
        "GROUP BY seller_team_id;",
        season);
    KboIndependentAcquisitionSqlTransferSummaryLoadContext ctx = {out, max_count, 0};
    if (!kbo_save_state_query(
            sql,
            kbo_independent_acquisition_sql_transfer_summary_cb,
            &ctx,
            "independent_acquisition_seller_summary")) {
        return -1;
    }
    return ctx.count;
}

int kbo_independent_acquisition_sql_load_buyer_transfer_summaries(
    uint32_t season,
    KboIndependentAcquisitionTransferSummary* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_buyer_summary_schema")) {
        return -1;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT buyer_team_id, COUNT(*), 0 "
        "FROM independent_acquisition_decisions "
        "WHERE season=%u AND transferred<>0 "
        "GROUP BY buyer_team_id;",
        season);
    KboIndependentAcquisitionSqlTransferSummaryLoadContext ctx = {out, max_count, 0};
    if (!kbo_save_state_query(
            sql,
            kbo_independent_acquisition_sql_transfer_summary_cb,
            &ctx,
            "independent_acquisition_buyer_summary")) {
        return -1;
    }
    return ctx.count;
}

int kbo_independent_acquisition_sql_transferred_count(
    uint32_t season,
    uint32_t team_id,
    int seller_side)
{
    if (season == 0u || team_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_decision_count_schema")) {
        return 0;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(*) FROM independent_acquisition_decisions "
        "WHERE season=%u AND %s=%u AND transferred<>0;",
        season,
        seller_side ? "seller_team_id" : "buyer_team_id",
        team_id);
    KboIndependentAcquisitionSqlCountResult result = {0};
    kbo_save_state_query(sql, kbo_independent_acquisition_sql_count_cb, &result, "independent_acquisition_decision_count");
    return result.count;
}

uint32_t kbo_independent_acquisition_sql_last_transfer_date(
    uint32_t season,
    uint32_t seller_team_id)
{
    if (season == 0u || seller_team_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_last_transfer_schema")) {
        return 0u;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT MAX(date) FROM independent_acquisition_decisions "
        "WHERE season=%u AND seller_team_id=%u AND transferred<>0;",
        season,
        seller_team_id);
    KboIndependentAcquisitionSqlDateResult result = {0};
    kbo_save_state_query(sql, kbo_independent_acquisition_sql_date_cb, &result, "independent_acquisition_last_transfer");
    return result.date;
}

int kbo_independent_acquisition_sql_append_decision(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int transferred,
    int32_t old_cash,
    int32_t new_cash,
    int32_t seller_transfer_fee,
    int32_t seller_old_cash,
    int32_t seller_new_cash,
    const char* source)
{
    if (today == 0u || request == NULL
            || request->season == 0u || request->seller_team_id == 0u || request->player_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_decision_append_schema")) {
        return 0;
    }

    char escaped_source[512] = {0};
    if (!kbo_independent_acquisition_sql_escape(escaped_source, sizeof(escaped_source), source)) {
        return 0;
    }

    char sql[2048] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO independent_acquisition_decisions("
        "date, season, seller_team_id, player_id, buyer_team_id, request_score, value_score, cash_cost, "
        "old_cash, new_cash, seller_transfer_fee, seller_old_cash, seller_new_cash, transferred, source"
        ") VALUES(%u, %u, %u, %u, %u, %lld, %d, %d, %d, %d, %d, %d, %d, %u, '%s');",
        today,
        request->season,
        request->seller_team_id,
        request->player_id,
        request->buyer_team_id,
        (long long)request->request_score,
        request->value_score,
        request->cash_cost,
        old_cash,
        new_cash,
        seller_transfer_fee,
        seller_old_cash,
        seller_new_cash,
        transferred ? 1u : 0u,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtime_line("independent acquisition decision sqlite append skipped reason=sql_buffer_full");
        return 0;
    }
    return kbo_save_state_exec(sql, "independent_acquisition_decision_append");
}

static int kbo_independent_acquisition_sql_decision_row_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboIndependentAcquisitionSqlDecisionLoadContext* ctx =
        (KboIndependentAcquisitionSqlDecisionLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 14 || ctx->count >= ctx->max_count) {
        return 0;
    }
    KboIndependentAcquisitionSqlDecisionRow row;
    memset(&row, 0, sizeof(row));
    row.date = kbo_independent_acquisition_sql_u32(vals, 0);
    row.season = kbo_independent_acquisition_sql_u32(vals, 1);
    row.seller_team_id = kbo_independent_acquisition_sql_u32(vals, 2);
    row.player_id = kbo_independent_acquisition_sql_u32(vals, 3);
    row.buyer_team_id = kbo_independent_acquisition_sql_u32(vals, 4);
    row.request_score = kbo_independent_acquisition_sql_i64(vals, 5);
    row.value_score = kbo_independent_acquisition_sql_i32(vals, 6);
    row.cash_cost = kbo_independent_acquisition_sql_i32(vals, 7);
    row.old_cash = kbo_independent_acquisition_sql_i32(vals, 8);
    row.new_cash = kbo_independent_acquisition_sql_i32(vals, 9);
    row.seller_transfer_fee = kbo_independent_acquisition_sql_i32(vals, 10);
    row.seller_old_cash = kbo_independent_acquisition_sql_i32(vals, 11);
    row.seller_new_cash = kbo_independent_acquisition_sql_i32(vals, 12);
    row.transferred = (uint8_t)(kbo_independent_acquisition_sql_u32(vals, 13) ? 1u : 0u);
    if (row.season != 0u && row.seller_team_id != 0u && row.player_id != 0u) {
        ctx->rows[ctx->count++] = row;
    }
    return 0;
}

int kbo_independent_acquisition_sql_load_decision_rows(
    uint32_t season,
    KboIndependentAcquisitionSqlDecisionRow* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_decision_ui_load_schema")) {
        return 0;
    }

    char sql[1024] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT date, season, seller_team_id, player_id, buyer_team_id, request_score, value_score, cash_cost, "
        "old_cash, new_cash, seller_transfer_fee, seller_old_cash, seller_new_cash, transferred "
        "FROM independent_acquisition_decisions WHERE season=%u ORDER BY date DESC, id DESC;",
        season);
    KboIndependentAcquisitionSqlDecisionLoadContext ctx = {out, max_count, 0};
    if (!kbo_save_state_query(sql, kbo_independent_acquisition_sql_decision_row_cb, &ctx, "independent_acquisition_decision_ui_load")) {
        return 0;
    }
    return ctx.count;
}
