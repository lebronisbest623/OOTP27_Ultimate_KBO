#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ui.h"
#include "../../ai/io/sql/independent_acquisition_sql_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../runtime_memory/runtime_memory.h"

typedef struct KboIndependentAcquisitionUiDecisionRow {
    uint32_t date;
    uint32_t season;
    uint32_t seller_team_id;
    uint32_t player_id;
    uint32_t buyer_team_id;
    int32_t value_score;
    int32_t cash_cost;
    int32_t old_cash;
    int32_t new_cash;
    int64_t request_score;
    uint8_t transferred;
} KboIndependentAcquisitionUiDecisionRow;

static void kbo_independent_acquisition_ui_copy_text(
    char* out,
    size_t out_size,
    const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (text != NULL) {
        snprintf(out, out_size, "%s", text);
    }
}

static void kbo_independent_acquisition_ui_enrich_request_row(
    KboIndependentAcquisitionUiRequestRow* row)
{
    if (row == NULL || row->player_id == 0u) {
        return;
    }

    uint32_t current_team_id = 0u;
    uint32_t current_league_id = 0u;
    uint8_t* player = kbo_find_player_by_id(row->player_id, &current_team_id, &current_league_id);
    (void)current_team_id;
    (void)current_league_id;
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        row->player_ptr = (uintptr_t)player;
        if (memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))) {
            row->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        }
        if (row->nation_id == 0u
                && memory_range_readable(player + OOTP27_PLAYER_NATION_ID_OFFSET, sizeof(uint32_t))) {
            row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        }
    }
}

static void kbo_independent_acquisition_ui_request_from_sql(
    const KboIndependentAcquisitionSqlRequestRow* in,
    KboIndependentAcquisitionUiRequestRow* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (in == NULL) {
        return;
    }

    out->date = in->date;
    out->season = in->season;
    out->buyer_team_id = in->buyer_team_id;
    out->seller_team_id = in->seller_team_id;
    out->player_id = in->player_id;
    out->nation_id = in->nation_id;
    out->effective_before = in->effective_before;
    out->effective_after = in->effective_after;
    out->effective_limit = in->effective_limit;
    out->injured_player_id = in->injured_player_id;
    out->pitcher = in->pitcher ? 1u : 0u;
    out->asian_quota = in->asian_quota ? 1u : 0u;
    out->value_score = in->value_score;
    out->cash_cost = in->cash_cost;
    out->request_score = in->request_score;
    kbo_independent_acquisition_ui_copy_text(
        out->slot_label,
        sizeof(out->slot_label),
        in->slot_type[0] != '\0' ? in->slot_type : "-");
    kbo_independent_acquisition_ui_enrich_request_row(out);
}

static void kbo_independent_acquisition_ui_decision_from_sql(
    const KboIndependentAcquisitionSqlDecisionRow* in,
    KboIndependentAcquisitionUiDecisionRow* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (in == NULL) {
        return;
    }

    out->date = in->date;
    out->season = in->season;
    out->seller_team_id = in->seller_team_id;
    out->player_id = in->player_id;
    out->buyer_team_id = in->buyer_team_id;
    out->value_score = in->value_score;
    out->cash_cost = in->cash_cost;
    out->old_cash = in->old_cash;
    out->new_cash = in->new_cash;
    out->request_score = in->request_score;
    out->transferred = in->transferred ? 1u : 0u;
}

static const KboIndependentAcquisitionUiDecisionRow*
kbo_independent_acquisition_ui_find_decision(
    const KboIndependentAcquisitionUiDecisionRow* decisions,
    int decision_count,
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (decisions == NULL || decision_count <= 0) {
        return NULL;
    }
    for (int i = 0; i < decision_count; i++) {
        const KboIndependentAcquisitionUiDecisionRow* row = &decisions[i];
        if (row->season == season
                && row->seller_team_id == seller_team_id
                && row->player_id == player_id) {
            return row;
        }
    }
    return NULL;
}

static int kbo_independent_acquisition_ui_load_decision_rows(
    uint32_t season,
    KboIndependentAcquisitionUiDecisionRow* out_rows,
    int max_rows)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);

    KboIndependentAcquisitionSqlDecisionRow sql_rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS];
    int limit = max_rows < KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS
        ? max_rows
        : KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS;
    int count = kbo_independent_acquisition_sql_load_decision_rows(
        season,
        sql_rows,
        limit);
    for (int i = 0; i < count; i++) {
        kbo_independent_acquisition_ui_decision_from_sql(&sql_rows[i], &out_rows[i]);
    }
    return count;
}

static int kbo_independent_acquisition_ui_request_row_cmp_desc(const void* a, const void* b)
{
    const KboIndependentAcquisitionUiRequestRow* left = (const KboIndependentAcquisitionUiRequestRow*)a;
    const KboIndependentAcquisitionUiRequestRow* right = (const KboIndependentAcquisitionUiRequestRow*)b;
    if (left->date < right->date) { return 1; }
    if (left->date > right->date) { return -1; }
    if (left->request_score < right->request_score) { return 1; }
    if (left->request_score > right->request_score) { return -1; }
    return 0;
}

static int kbo_independent_acquisition_ui_result_row_cmp_desc(const void* a, const void* b)
{
    const KboIndependentAcquisitionUiResultRow* left = (const KboIndependentAcquisitionUiResultRow*)a;
    const KboIndependentAcquisitionUiResultRow* right = (const KboIndependentAcquisitionUiResultRow*)b;
    if (left->decision_date < right->decision_date) { return 1; }
    if (left->decision_date > right->decision_date) { return -1; }
    if (left->request.date < right->request.date) { return 1; }
    if (left->request.date > right->request.date) { return -1; }
    return 0;
}

int kbo_independent_acquisition_ui_load_pending_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiRequestRow* out_rows,
    int max_rows)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);

    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)) {
        return 0;
    }

    KboIndependentAcquisitionSqlRequestRow sql_rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS];
    int limit = max_rows < KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS
        ? max_rows
        : KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS;
    int count = kbo_independent_acquisition_sql_load_request_rows(
        context.season,
        buyer_team_id,
        1,
        sql_rows,
        limit);
    for (int i = 0; i < count; i++) {
        kbo_independent_acquisition_ui_request_from_sql(&sql_rows[i], &out_rows[i]);
    }

    if (count > 1) {
        qsort(out_rows, (size_t)count, sizeof(out_rows[0]), kbo_independent_acquisition_ui_request_row_cmp_desc);
    }
    return count;
}

int kbo_independent_acquisition_ui_load_result_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiResultRow* out_rows,
    int max_rows)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);

    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)) {
        return 0;
    }

    KboIndependentAcquisitionUiDecisionRow decisions[KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS];
    int decision_count = kbo_independent_acquisition_ui_load_decision_rows(
        context.season,
        decisions,
        KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS);
    if (decision_count <= 0) {
        return 0;
    }

    KboIndependentAcquisitionSqlRequestRow request_rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS];
    int request_count = kbo_independent_acquisition_sql_load_request_rows(
        context.season,
        buyer_team_id,
        0,
        request_rows,
        KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS);
    int count = 0;
    for (int i = 0; i < request_count && count < max_rows; i++) {
        KboIndependentAcquisitionUiRequestRow request;
        kbo_independent_acquisition_ui_request_from_sql(&request_rows[i], &request);
        const KboIndependentAcquisitionUiDecisionRow* decision =
            kbo_independent_acquisition_ui_find_decision(
                decisions,
                decision_count,
                request.season,
                request.seller_team_id,
                request.player_id);
        if (decision != NULL) {
            KboIndependentAcquisitionUiResultRow result;
            memset(&result, 0, sizeof(result));
            result.request = request;
            result.decision_date = decision->date;
            result.winning_buyer_team_id = decision->buyer_team_id;
            result.old_cash = decision->old_cash;
            result.new_cash = decision->new_cash;
            result.transferred = decision->transferred;
            out_rows[count++] = result;
        }
    }

    if (count > 1) {
        qsort(out_rows, (size_t)count, sizeof(out_rows[0]), kbo_independent_acquisition_ui_result_row_cmp_desc);
    }
    return count;
}
