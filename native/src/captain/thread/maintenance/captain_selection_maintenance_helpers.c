#include "captain_selection_maintenance_helpers.h"

#include "../../internal/captain_selection_internal.h"
#include "../../../core/logging/rule_audit.h"

void kbo_captain_audit_maintenance(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint32_t league_season,
    uint8_t phase,
    int csv_exists,
    int calendar_recovery,
    int calendar_preseason,
    int seed_startup,
    int calendar_preseason_start)
{
    KboLogFields audit_fields;
    kbo_log_fields_init(&audit_fields);
    kbo_log_field_u32(&audit_fields, "date", date);
    kbo_log_field_u32(&audit_fields, "season", season);
    kbo_log_field_u32(&audit_fields, "league_id", league_id);
    kbo_log_field_u32(&audit_fields, "league_season", league_season);
    kbo_log_field_u32(&audit_fields, "phase", (unsigned)phase);
    kbo_log_field_i32(&audit_fields, "csv_exists", csv_exists);
    kbo_log_field_i32(&audit_fields, "calendar_recovery", calendar_recovery);
    kbo_log_field_i32(&audit_fields, "calendar_preseason", calendar_preseason);
    kbo_log_field_i32(&audit_fields, "seed_startup", seed_startup);
    kbo_log_field_i32(&audit_fields, "calendar_preseason_start", calendar_preseason_start);
    kbo_rule_audit_emit_fields(
        "captain.maintenance",
        decision,
        reason,
        source,
        &audit_fields);
}

int kbo_captain_emit_initial_selection_news_from_csv_or_defer(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint32_t league_season,
    uint8_t phase,
    int csv_exists,
    int calendar_recovery,
    int calendar_preseason,
    int seed_startup,
    int calendar_preseason_start,
    const char* source)
{
    if (kbo_captain_initial_selection_news_exists(season, league_id)) {
        return 0;
    }

    KboCaptainSelectionRow summary_rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(summary_rows, 0, sizeof(summary_rows));
    int summary_count = kbo_captain_load_selection_csv(season, summary_rows, KBO_CAPTAIN_MAX_TEAMS);
    if (summary_count <= 0) {
        kbo_captain_audit_maintenance(
            "defer",
            "csv_summary_unavailable",
            source,
            date,
            season,
            league_id,
            league_season,
            phase,
            csv_exists,
            calendar_recovery,
            calendar_preseason,
            seed_startup,
            calendar_preseason_start);
        return -1;
    }

    int listed = 0;
    for (int i = 0; i < summary_count; i++) {
        if (summary_rows[i].team_id != 0u && summary_rows[i].player_id != 0u) {
            listed++;
        }
    }
    if (listed <= 0) {
        kbo_captain_audit_maintenance(
            "skip",
            "csv_summary_no_listed_captains",
            source,
            date,
            season,
            league_id,
            league_season,
            phase,
            csv_exists,
            calendar_recovery,
            calendar_preseason,
            seed_startup,
            calendar_preseason_start);
        return 0;
    }

    kbo_captain_audit_maintenance(
        "emit_initial_selection_news",
        "csv_summary",
        source,
        date,
        season,
        league_id,
        league_season,
        phase,
        csv_exists,
        calendar_recovery,
        calendar_preseason,
        seed_startup,
        calendar_preseason_start);
    int result = kbo_emit_captain_initial_selection_news(
        date,
        season,
        league_id,
        summary_rows,
        summary_count,
        source != NULL ? source : "captain_summary_maintenance");
    if (result > 0 || kbo_captain_initial_selection_news_exists(season, league_id)) {
        return result;
    }

    kbo_captain_audit_maintenance(
        "defer",
        "initial_news_not_created",
        source,
        date,
        season,
        league_id,
        league_season,
        phase,
        csv_exists,
        calendar_recovery,
        calendar_preseason,
        seed_startup,
        calendar_preseason_start);
    return -1;
}

int kbo_captain_write_missing_csv_or_defer(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint8_t phase,
    const char* source)
{
    int result = kbo_captain_write_missing_selection_csv(
        date,
        season,
        league_id,
        phase,
        source);
    return result > 0 || kbo_captain_selection_csv_exists(season) ? result : -1;
}
