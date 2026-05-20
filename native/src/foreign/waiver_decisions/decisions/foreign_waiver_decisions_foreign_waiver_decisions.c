#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/logging/rule_audit.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../waiver_core/api/foreign_waiver_core.h"
#include "../api/foreign_waiver_decisions.h"
#include "../internal/foreign_waiver_decisions_state_internal.h"
#include "../sql/foreign_waiver_decisions_sql_store.h"

static void kbo_audit_foreign_waiver_decision_record(
    const char* decision,
    const char* reason,
    const char* source,
    const char* action,
    uint32_t team_id,
    uint32_t player_id,
    int score,
    int forced,
    int executed,
    uint32_t date,
    uint32_t window_start,
    uint32_t window_end)
{
    KboLogFields fields;
    kbo_log_fields_init(&fields);
    kbo_log_field_str(&fields, "action", action != NULL ? action : "");
    kbo_log_field_u32(&fields, "team_id", team_id);
    kbo_log_field_u32(&fields, "player_id", player_id);
    if (score != 0) {
        kbo_log_field_i32(&fields, "score", score);
    }
    kbo_log_field_bool(&fields, "forced", forced);
    kbo_log_field_bool(&fields, "executed", executed);
    if (date != 0u) {
        kbo_log_field_u32(&fields, "date", date);
    }
    if (window_start != 0u) {
        kbo_log_field_u32(&fields, "window_start", window_start);
    }
    if (window_end != 0u) {
        kbo_log_field_u32(&fields, "window_end", window_end);
    }
    kbo_rule_audit_emit_fields(
        "foreign_waiver.decision_record",
        decision,
        reason,
        source,
        &fields);
}

int kbo_append_foreign_waiver_decision_record(
    const char* source,
    const char* action,
    uint32_t team_id,
    uint32_t player_id,
    int score,
    int forced,
    int executed)
{
    if (source == NULL || source[0] == '\0' || action == NULL || action[0] == '\0'
            || team_id == 0u || player_id == 0u) {
        return 0;
    }

    uint32_t today = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);

    uint32_t window_start = 0u;
    uint32_t window_end = 0u;
    kbo_current_foreign_waiver_window_dates(&window_start, &window_end);

    int ok = kbo_foreign_waiver_decisions_sql_append(
        source,
        action,
        today,
        window_start,
        window_end,
        team_id,
        player_id,
        score,
        forced ? 1 : 0,
        executed ? 1 : 0);
    kbo_audit_foreign_waiver_decision_record(
        ok ? "write_record" : "fail",
        ok ? "decision_recorded" : "sqlite_write_failed",
        source,
        action,
        team_id,
        player_id,
        score,
        forced ? 1 : 0,
        executed ? 1 : 0,
        today,
        window_start,
        window_end);
    return ok;
}

int kbo_foreign_waiver_decision_exists(uint32_t window_end, uint32_t team_id, uint32_t player_id)
{
    if (window_end == 0u || team_id == 0u || player_id == 0u) {
        return 0;
    }

    return kbo_foreign_waiver_decisions_sql_exists(window_end, team_id, player_id);
}

int kbo_foreign_waiver_latest_decision_action(
    uint32_t window_end,
    uint32_t team_id,
    uint32_t player_id,
    char* out_action,
    size_t out_action_size)
{
    if (out_action != NULL && out_action_size > 0u) {
        out_action[0] = '\0';
    }
    if (window_end == 0u || team_id == 0u || player_id == 0u || out_action == NULL || out_action_size == 0u) {
        return 0;
    }

    return kbo_foreign_waiver_decisions_sql_latest_action(
        window_end,
        team_id,
        player_id,
        out_action,
        out_action_size);
}

