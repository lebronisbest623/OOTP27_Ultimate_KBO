#include "../independent_acquisition_sql_store_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../../../../core/logging/core_log.h"
#include "../../../../../../core/sql/save_state/save_state_sqlite.h"

int kbo_independent_acquisition_sql_request_exists(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || buyer_team_id == 0u || seller_team_id == 0u || player_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_request_exists_schema")) {
        return 0;
    }

    char sql[384] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM independent_acquisition_requests "
        "WHERE season=%u AND buyer_team_id=%u AND seller_team_id=%u AND player_id=%u LIMIT 1;",
        season,
        buyer_team_id,
        seller_team_id,
        player_id);
    KboIndependentAcquisitionSqlExistsResult result = {0};
    kbo_save_state_query(sql, kbo_independent_acquisition_sql_exists_cb, &result, "independent_acquisition_request_exists");
    return result.found;
}

int kbo_independent_acquisition_sql_cancel_request(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || buyer_team_id == 0u || seller_team_id == 0u || player_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_request_cancel_schema")) {
        return 0;
    }
    if (!kbo_independent_acquisition_sql_request_exists(season, buyer_team_id, seller_team_id, player_id)
            || kbo_independent_acquisition_sql_decision_exists(season, seller_team_id, player_id)) {
        return 0;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "DELETE FROM independent_acquisition_requests "
        "WHERE season=%u AND buyer_team_id=%u AND seller_team_id=%u AND player_id=%u;",
        season,
        buyer_team_id,
        seller_team_id,
        player_id);
    return kbo_save_state_exec(sql, "independent_acquisition_request_cancel");
}

int kbo_independent_acquisition_sql_append_request(
    uint32_t today,
    const KboIndependentAcquisitionCandidate* candidate,
    const KboIndependentAcquisitionBuyerState* buyer,
    const KboIndependentFuturesTeamLeague* seller,
    int32_t cash_cost,
    const char* slot_label,
    const char* source)
{
    if (today == 0u || candidate == NULL || buyer == NULL || seller == NULL
            || candidate->player_id == 0u || candidate->seller_team_id == 0u || buyer->team_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_request_append_schema")) {
        return 0;
    }

    char escaped_seller_csv_id[128] = {0};
    char escaped_slot_label[128] = {0};
    char escaped_source[512] = {0};
    if (!kbo_independent_acquisition_sql_escape(escaped_seller_csv_id, sizeof(escaped_seller_csv_id), seller->team_csv_id)
            || !kbo_independent_acquisition_sql_escape(escaped_slot_label, sizeof(escaped_slot_label), slot_label)
            || !kbo_independent_acquisition_sql_escape(escaped_source, sizeof(escaped_source), source)) {
        return 0;
    }

    char sql[4096] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO independent_acquisition_requests("
        "date, season, buyer_team_id, seller_team_id, seller_csv_id, player_id, "
        "nation_id, pitcher, asian_quota, cash_cost, value_score, request_score, "
        "effective_before, effective_after, effective_limit, slot_type, injured_player_id, "
        "buyer_active_count, buyer_foreign_effective, source"
        ") VALUES(%u, %u, %u, %u, '%s', %u, %u, %u, %u, %d, %d, %lld, %u, %u, %u, '%s', %u, %u, %u, '%s');",
        today,
        today / 10000u,
        buyer->team_id,
        candidate->seller_team_id,
        escaped_seller_csv_id,
        candidate->player_id,
        candidate->nation_id,
        (uint32_t)candidate->pitcher,
        (uint32_t)candidate->asian_quota,
        cash_cost,
        candidate->value_score,
        (long long)candidate->request_score,
        candidate->effective_before,
        candidate->effective_after,
        candidate->effective_limit,
        escaped_slot_label,
        candidate->injured_player_id,
        buyer->active_count,
        buyer->effective_foreign_count,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtime_line("independent acquisition request sqlite append skipped reason=sql_buffer_full");
        return 0;
    }
    return kbo_save_state_exec(sql, "independent_acquisition_request_append");
}

static int kbo_independent_acquisition_sql_queued_request_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboIndependentAcquisitionSqlQueuedLoadContext* ctx =
        (KboIndependentAcquisitionSqlQueuedLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 8 || ctx->count >= ctx->max_count) {
        return 0;
    }

    KboIndependentAcquisitionQueuedRequest row;
    memset(&row, 0, sizeof(row));
    row.date = kbo_independent_acquisition_sql_u32(vals, 0);
    row.season = kbo_independent_acquisition_sql_u32(vals, 1);
    row.buyer_team_id = kbo_independent_acquisition_sql_u32(vals, 2);
    row.seller_team_id = kbo_independent_acquisition_sql_u32(vals, 3);
    row.player_id = kbo_independent_acquisition_sql_u32(vals, 4);
    row.request_score = kbo_independent_acquisition_sql_i64(vals, 5);
    row.value_score = kbo_independent_acquisition_sql_i32(vals, 6);
    row.cash_cost = kbo_independent_acquisition_sql_i32(vals, 7);
    if (row.season != 0u && row.buyer_team_id != 0u && row.seller_team_id != 0u && row.player_id != 0u) {
        ctx->rows[ctx->count++] = row;
    }
    return 0;
}

int kbo_independent_acquisition_sql_load_pending_requests(
    uint32_t season,
    KboIndependentAcquisitionQueuedRequest* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_request_load_schema")) {
        return 0;
    }

    char sql[1024] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT date, season, buyer_team_id, seller_team_id, player_id, request_score, value_score, cash_cost "
        "FROM independent_acquisition_requests r "
        "WHERE r.season=%u AND NOT EXISTS("
        "SELECT 1 FROM independent_acquisition_decisions d "
        "WHERE d.season=r.season AND d.seller_team_id=r.seller_team_id AND d.player_id=r.player_id"
        ") ORDER BY date, request_score DESC, id;",
        season);
    KboIndependentAcquisitionSqlQueuedLoadContext ctx = {out, max_count, 0};
    if (!kbo_save_state_query(sql, kbo_independent_acquisition_sql_queued_request_cb, &ctx, "independent_acquisition_request_load")) {
        return 0;
    }
    return ctx.count;
}

static int kbo_independent_acquisition_sql_request_row_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboIndependentAcquisitionSqlRequestLoadContext* ctx =
        (KboIndependentAcquisitionSqlRequestLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 16 || ctx->count >= ctx->max_count) {
        return 0;
    }

    KboIndependentAcquisitionSqlRequestRow row;
    memset(&row, 0, sizeof(row));
    row.date = kbo_independent_acquisition_sql_u32(vals, 0);
    row.season = kbo_independent_acquisition_sql_u32(vals, 1);
    row.buyer_team_id = kbo_independent_acquisition_sql_u32(vals, 2);
    row.seller_team_id = kbo_independent_acquisition_sql_u32(vals, 3);
    row.player_id = kbo_independent_acquisition_sql_u32(vals, 4);
    row.nation_id = kbo_independent_acquisition_sql_u32(vals, 5);
    row.pitcher = (uint8_t)(kbo_independent_acquisition_sql_u32(vals, 6) ? 1u : 0u);
    row.asian_quota = (uint8_t)(kbo_independent_acquisition_sql_u32(vals, 7) ? 1u : 0u);
    row.cash_cost = kbo_independent_acquisition_sql_i32(vals, 8);
    row.value_score = kbo_independent_acquisition_sql_i32(vals, 9);
    row.request_score = kbo_independent_acquisition_sql_i64(vals, 10);
    row.effective_before = kbo_independent_acquisition_sql_u32(vals, 11);
    row.effective_after = kbo_independent_acquisition_sql_u32(vals, 12);
    row.effective_limit = kbo_independent_acquisition_sql_u32(vals, 13);
    kbo_independent_acquisition_sql_text(vals, 14, row.slot_type, sizeof(row.slot_type));
    row.injured_player_id = kbo_independent_acquisition_sql_u32(vals, 15);
    if (row.season != 0u && row.buyer_team_id != 0u && row.seller_team_id != 0u && row.player_id != 0u) {
        ctx->rows[ctx->count++] = row;
    }
    return 0;
}

int kbo_independent_acquisition_sql_load_request_rows(
    uint32_t season,
    uint32_t buyer_team_id,
    int pending_only,
    KboIndependentAcquisitionSqlRequestRow* out,
    int max_count)
{
    if (season == 0u || buyer_team_id == 0u || out == NULL || max_count <= 0
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_request_ui_load_schema")) {
        return 0;
    }

    char sql[1536] = {0};
    size_t cursor = 0u;
    int ok = kbo_independent_acquisition_sql_append_text(
        sql,
        sizeof(sql),
        &cursor,
        "SELECT date, season, buyer_team_id, seller_team_id, player_id, nation_id, pitcher, asian_quota, "
        "cash_cost, value_score, request_score, effective_before, effective_after, effective_limit, slot_type, injured_player_id "
        "FROM independent_acquisition_requests r WHERE r.season=%u AND r.buyer_team_id=%u ",
        season,
        buyer_team_id);
    if (ok && pending_only) {
        ok = kbo_independent_acquisition_sql_append_text(
            sql,
            sizeof(sql),
            &cursor,
            "AND NOT EXISTS(SELECT 1 FROM independent_acquisition_decisions d "
            "WHERE d.season=r.season AND d.seller_team_id=r.seller_team_id AND d.player_id=r.player_id) ");
    }
    if (ok) {
        ok = kbo_independent_acquisition_sql_append_text(
            sql,
            sizeof(sql),
            &cursor,
            "ORDER BY date DESC, request_score DESC, id DESC;");
    }
    if (!ok) {
        return 0;
    }

    KboIndependentAcquisitionSqlRequestLoadContext ctx = {out, max_count, 0};
    if (!kbo_save_state_query(sql, kbo_independent_acquisition_sql_request_row_cb, &ctx, "independent_acquisition_request_ui_load")) {
        return 0;
    }
    return ctx.count;
}
