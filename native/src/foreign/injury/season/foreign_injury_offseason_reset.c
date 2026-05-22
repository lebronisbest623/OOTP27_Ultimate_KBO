#include "../internal/foreign_injury_internal.h"

int kbo_foreign_injury_reset_open_replacements_for_offseason_locked(
    uint32_t close_date_yyyymmdd,
    const char* source)
{
    if (close_date_yyyymmdd == 0u) {
        return 0;
    }

    int closed = 0;
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (!kbo_foreign_injury_status_uses_slot(rec->status)
                && rec->status != KBO_FOREIGN_INJURY_STATUS_PENDING) {
            continue;
        }

        uint32_t replacement_player_id = rec->replacement_player_id;
        if (replacement_player_id != 0u) {
            kbo_foreign_injury_release_replacement_player(
                rec->team_id,
                replacement_player_id,
                source != NULL ? source : "offseason_reset");
        }

        rec->status = KBO_FOREIGN_INJURY_STATUS_CLOSED;
        rec->closed_on_yyyymmdd = close_date_yyyymmdd;
        rec->converted = 0u;
        rec->close_choice = KBO_FOREIGN_INJURY_CLOSE_KEEP_INJURED;
        closed++;

        do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", close_date_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "team_id", rec->team_id);
            kbo_log_field_u32(&audit_fields, "league_id", rec->league_id);
            kbo_log_field_u32(&audit_fields, "injured_player_id", rec->injured_player_id);
            kbo_log_field_u32(&audit_fields, "replacement_player_id", replacement_player_id);
            kbo_log_field_u32(&audit_fields, "opened_on", rec->opened_on_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "expected_end", rec->expected_end_yyyymmdd);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.offseason_reset",
                "close",
                "season_boundary",
                source,
                &audit_fields);
        } while (0);
    }

    return closed;
}

int kbo_foreign_injury_reset_open_replacements_for_offseason(
    uint32_t close_date_yyyymmdd,
    const char* source)
{
    if (close_date_yyyymmdd == 0u) {
        return 0;
    }

    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    int closed = kbo_foreign_injury_reset_open_replacements_for_offseason_locked(
        close_date_yyyymmdd,
        source != NULL ? source : "offseason_reset");
    if (closed > 0) {
        kbo_persist_foreign_injury_replacements_locked();
    }
    kbo_unlock_foreign_injury_replacements();

    if (closed > 0) {
        kbo_log_runtimef(
            "foreign injury replacement: offseason reset closed open slots source=%s close_date=%u closed=%d",
            source != NULL ? source : "",
            close_date_yyyymmdd,
            closed);
    }
    return closed;
}
