#include "foreign_injury_scanner_internal.h"
#include "../../common/policy/foreign_player_policy.h"
#include "../../../bootstrap/profiling/profiler.h"

static LONG g_kbo_foreign_injury_non_roster_log_count = 0;
static LONG g_kbo_foreign_injury_below_min_log_count = 0;
static void kbo_foreign_injury_replacement_scan_for_date_mode(
    const char* source,
    uint32_t today,
    int process_existing_replacements,
    int captured_live_date);
static int kbo_foreign_injury_replacement_scan_sql_discovery_only(
    const char* source,
    uint32_t today,
    uint32_t configured_league_id,
    int slot_opening_allowed);

void kbo_foreign_injury_replacement_scan_captured_date(const char* source, uint32_t today)
{
    kbo_foreign_injury_replacement_scan_for_date_mode(source, today, 1, 1);
}

void kbo_foreign_injury_replacement_scan_discovery_for_date(const char* source, uint32_t today)
{
    kbo_foreign_injury_replacement_scan_for_date_mode(source, today, 0, 0);
}

static int kbo_foreign_injury_replacement_scan_sql_discovery_only(
    const char* source,
    uint32_t today,
    uint32_t configured_league_id,
    int slot_opening_allowed)
{
    if (!slot_opening_allowed) {
        kbo_profiler_record_us("foreign_injury.scan.sql_discovery_skipped_closed_window", 0);
        return 0;
    }

    KboForeignInjurySqlDiscoveryRow rows[KBO_FOREIGN_INJURY_SQL_DISCOVERY_MAX];
    int row_count = 0;
    int rows_seen = 0;
    int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
    int sql_result = kbo_foreign_injury_collect_sql_long_term_injuries_on_date(
        today,
        min_days,
        rows,
        KBO_FOREIGN_INJURY_SQL_DISCOVERY_MAX,
        &row_count,
        &rows_seen);
    if (sql_result != 0 || row_count <= 0) {
        return 0;
    }

    int opened = 0;
    for (int i = 0; i < row_count; i++) {
        const KboForeignInjurySqlDiscoveryRow* evidence = &rows[i];
        if (evidence->player_id == 0u || evidence->days < min_days) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(evidence->player_id, NULL, NULL);
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        if (!kbo_player_is_foreign_for_kbo_rights(player)
                || !kbo_foreign_injury_player_has_baseball_position(player)) {
            continue;
        }

        uint32_t team_id = 0u;
        uint32_t league_id = 0u;
        int has_assignment = kbo_foreign_injury_resolve_player_team_assignment(
            player,
            evidence->player_id,
            configured_league_id,
            &team_id,
            &league_id);
        if (!has_assignment) {
            continue;
        }

        uint32_t opened_on = evidence->evidence_date != 0u ? evidence->evidence_date : today;
        uint32_t expected_end = kbo_foreign_injury_expected_end_from_duration(
            opened_on,
            evidence->days);
        if (expected_end == 0u || kbo_foreign_injury_expected_end_reached(today, expected_end)) {
            continue;
        }

        KboForeignInjuryReplacement created_rec;
        memset(&created_rec, 0, sizeof(created_rec));
        int created = 0;

        kbo_lock_foreign_injury_replacements();
        int existing = kbo_find_foreign_injury_replacement_locked(evidence->player_id, 1);
        int already_replacement = kbo_foreign_injury_replacement_player_reserved_locked(evidence->player_id, NULL);
        if (existing < 0
                && !already_replacement
                && g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
            KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++];
            rec->team_id = team_id;
            rec->league_id = league_id != 0u ? league_id : configured_league_id;
            rec->injured_player_id = evidence->player_id;
            rec->replacement_player_id = 0u;
            rec->opened_on_yyyymmdd = opened_on;
            rec->expected_end_yyyymmdd = expected_end;
            rec->slot_type = kbo_foreign_injury_slot_type_for_player(player);
            rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
            rec->converted = 0u;
            created_rec = *rec;
            created = kbo_persist_foreign_injury_replacements_locked();
        }
        kbo_unlock_foreign_injury_replacements();

        if (!created) {
            continue;
        }

        opened++;
        do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_log_field_u32(&audit_fields, "team_id", created_rec.team_id);
            kbo_log_field_u32(&audit_fields, "league_id", created_rec.league_id);
            kbo_log_field_u32(&audit_fields, "injured_player_id", created_rec.injured_player_id);
            kbo_log_field_i32(&audit_fields, "effective_days_left", evidence->days);
            kbo_log_field_i32(&audit_fields, "sql_evidence_days", evidence->days);
            kbo_log_field_u32(&audit_fields, "sql_evidence_date", opened_on);
            kbo_log_field_u32(&audit_fields, "opened_on", created_rec.opened_on_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "expected_end", created_rec.expected_end_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "slot_type", (uint32_t)created_rec.slot_type);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.lifecycle",
                "open_slot",
                "sql_date_locked_discovery",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign injury replacement: opened source=%s team=%u player=%u league=%u days_left=0 effective_days_left=%d trigger=injury_sql_date_locked slot=%s opened_on=%u expected_end=%u",
            source != NULL ? source : "",
            created_rec.team_id,
            created_rec.injured_player_id,
            created_rec.league_id,
            evidence->days,
            kbo_foreign_injury_slot_label(created_rec.slot_type),
            created_rec.opened_on_yyyymmdd,
            created_rec.expected_end_yyyymmdd);
    }

    if (opened > 0) {
        do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_log_field_i32(&audit_fields, "rows_seen", rows_seen);
            kbo_log_field_i32(&audit_fields, "sql_candidates", row_count);
            kbo_log_field_i32(&audit_fields, "opened", opened);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.scan",
                "process",
                "sql_date_locked_discovery",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign injury replacement: sql date-locked discovery source=%s date=%u rows=%d candidates=%d opened=%d",
            source != NULL ? source : "",
            today,
            rows_seen,
            row_count,
            opened);
    }
    return opened;
}

static void kbo_foreign_injury_replacement_scan_for_date_mode(
    const char* source,
    uint32_t today,
    int process_existing_replacements,
    int captured_live_date)
{
    KBO_PROFILE_BEGIN(profile_foreign_injury_scan);
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "foreign_injury_replacement_scan")) {
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.save_pause_abort");
        return;
    }
    if (!kbo_foreign_injury_replacement_enabled()) {
        kbo_rule_audit_emit_fields("foreign_injury.replacement.scan", "skip", "disabled", source, NULL);
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.disabled");
        return;
    }
    if (kbo_foreign_injury_replacement_scan_source_is_read_only(source)) {
        KBO_PROFILE_BEGIN(profile_foreign_injury_load_readonly);
        kbo_ensure_foreign_injury_replacements_loaded();
        KBO_PROFILE_END(profile_foreign_injury_load_readonly, "foreign_injury.scan.readonly_load");
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.readonly");
        return;
    }
    if (today == 0u) {
        kbo_rule_audit_emit_fields("foreign_injury.replacement.scan", "skip", "date_unavailable", source, NULL);
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.no_date");
        return;
    }
    uint32_t live_date = 0u;
    kbo_get_current_yyyymmdd(&live_date);
    int live_injury_fields_available = live_date == today;
    if (captured_live_date && live_date != 0u && live_date != today) {
        kbo_log_runtimef(
            "foreign injury replacement: date-locked scan source=%s event_date=%u live_date=%u reason=stale_hook_uses_sql_only",
            source != NULL ? source : "",
            today,
            live_date);
    }
    if (kbo_foreign_injury_same_date_idle_scan_cached(today, source)) {
        kbo_profiler_record_us("foreign_injury.scan.same_date_idle_cached", 0);
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.same_date_idle_cached");
        return;
    }
    KBO_PROFILE_BEGIN(profile_foreign_injury_load);
    kbo_ensure_foreign_injury_replacements_loaded();
    KBO_PROFILE_END(profile_foreign_injury_load, "foreign_injury.scan.load_records");
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.scan",
                "skip",
                "player_vector_unavailable",
                source,
                &audit_fields);
        } while (0);
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.no_player_vector");
        return;
    }
    uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
    if (configured_league_id == 0u) {
        configured_league_id = kbo_resolve_kbo_league_id();
    }
    int slot_opening_allowed = kbo_foreign_injury_replacement_in_season_window(
        configured_league_id,
        today,
        source,
        "scan_open_slot");
    int scanned = 0;
    int opened = 0;
    if (!slot_opening_allowed) {
        kbo_profiler_record_us("foreign_injury.scan.player_loop_skipped_closed_window", 0);
    } else if (!process_existing_replacements && !live_injury_fields_available) {
        KBO_PROFILE_BEGIN(profile_foreign_injury_player_loop);
        opened = kbo_foreign_injury_replacement_scan_sql_discovery_only(
            source,
            today,
            configured_league_id,
            slot_opening_allowed);
        KBO_PROFILE_END(profile_foreign_injury_player_loop, "foreign_injury.scan.sql_date_locked_discovery");
    } else {
        KBO_PROFILE_BEGIN(profile_foreign_injury_player_loop);
        for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }
        scanned++;
        uint8_t injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        int16_t days_left = *(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
        int direct_injury_eligible = live_injury_fields_available
            && kbo_foreign_injury_duration_meets_minimum(days_left, min_days);
        uint32_t team_id = 0u;
        uint32_t league_id = 0u;
        int has_assignment = kbo_foreign_injury_resolve_player_team_assignment(
            player,
            player_id,
            configured_league_id,
            &team_id,
            &league_id);
        int inactive_roster_present = live_injury_fields_available && !direct_injury_eligible && has_assignment
            ? kbo_foreign_injury_player_on_inactive_replacement_roster(player, player_id, team_id, today)
            : 0;
        /* Message body files do not carry a reliable game date; lifecycle evidence must be date-locked. */
        int message_evidence_days = 0;
        int message_injury_eligible = 0;
        int sql_evidence_days = 0;
        uint32_t sql_evidence_date = 0u;
        int sql_injury_eligible = has_assignment
            ? kbo_foreign_injury_recent_sql_has_long_term_injury_date_on_date(
                player_id,
                min_days,
                today,
                &sql_evidence_days,
                &sql_evidence_date)
            : 0;
        int inactive_roster_eligible = kbo_foreign_injury_inactive_roster_has_long_term_injury_basis(
            injury_active,
            days_left,
            min_days,
            inactive_roster_present);
        if (!direct_injury_eligible && !inactive_roster_eligible && !message_injury_eligible && !sql_injury_eligible) {
            if (injury_active != 0u || days_left > 0 || inactive_roster_present) {
                LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_below_min_log_count);
                if (log_slot <= 80 || (log_slot % 250) == 0) {
                    uint32_t current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                    uint32_t active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
                    uint32_t loan_team = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
                    uint32_t original_team = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
                    uint32_t default_team = memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
                        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
                        : 0u;
                    do {
                        KboLogFields audit_fields;
                        kbo_log_fields_init(&audit_fields);
                        kbo_log_field_u32(&audit_fields, "date", today);
                        kbo_log_field_u32(&audit_fields, "player_id", player_id);
                        kbo_log_field_u32(&audit_fields, "team_id", team_id);
                        kbo_log_field_u32(&audit_fields, "league_id", league_id);
                        kbo_log_field_u32(&audit_fields, "current_team_id", current_team);
                        kbo_log_field_u32(&audit_fields, "active_team_id", active_team);
                        kbo_log_field_u32(&audit_fields, "loan_team_id", loan_team);
                        kbo_log_field_u32(&audit_fields, "original_team_id", original_team);
                        kbo_log_field_u32(&audit_fields, "default_team_id", default_team);
                        kbo_log_field_u32(&audit_fields, "injury_active", (uint32_t)injury_active);
                        kbo_log_field_i32(&audit_fields, "days_left", (int)days_left);
                        kbo_log_field_i32(&audit_fields, "min_days", min_days);
                        kbo_log_field_u32(&audit_fields, "inactive_roster", inactive_roster_present ? 1u : 0u);
                        kbo_log_field_u32(&audit_fields, "assignment", has_assignment ? 1u : 0u);
                        kbo_log_field_i32(&audit_fields, "message_evidence_days", message_evidence_days);
                        kbo_log_field_i32(&audit_fields, "sql_evidence_days", sql_evidence_days);
                        kbo_log_field_u32(&audit_fields, "sql_evidence_date", sql_evidence_date);
                        kbo_rule_audit_emit_fields(
                            "foreign_injury.replacement.lifecycle",
                            "skip_candidate",
                            inactive_roster_present
                                ? "inactive_roster_without_long_term_injury_days"
                                : "injury_below_min_days",
                            source,
                            &audit_fields);
                    } while (0);
                    kbo_log_runtimef(
                        "foreign injury replacement: skipped below-min injury candidate source=%s player=%u current=%u active=%u loan=%u original=%u default=%u resolved_team=%u league=%u assignment=%d injury=%u days_left=%d min_days=%d inactive_roster=%d",
                        source != NULL ? source : "",
                        player_id,
                        current_team,
                        active_team,
                        loan_team,
                        original_team,
                        default_team,
                        team_id,
                        league_id,
                        has_assignment,
                        (uint32_t)injury_active,
                        (int)days_left,
                        min_days,
                        inactive_roster_present);
                }
            }
            continue;
        }
        int effective_days_left = days_left;
        if (!direct_injury_eligible && message_injury_eligible && message_evidence_days > effective_days_left) {
            effective_days_left = message_evidence_days;
        }
        if (!direct_injury_eligible && sql_injury_eligible && sql_evidence_days > effective_days_left) {
            effective_days_left = sql_evidence_days;
        }
        if (effective_days_left < min_days) {
            effective_days_left = min_days;
        }
        if (!has_assignment || !kbo_foreign_injury_player_has_baseball_position(player)) {
            LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_non_roster_log_count);
            if (log_slot <= 60 || (log_slot % 100) == 0) {
                kbo_log_runtimef(
                    "foreign injury replacement: skipped non-roster injury candidate source=%s player=%u current=%u active=%u loan=%u original=%u default=%u resolved_team=%u league=%u position=%u role=%u assignment=%d injury=%u days_left=%d inactive_roster=%d",
                    source != NULL ? source : "",
                    player_id,
                    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET),
                    *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
                    memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
                        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
                        : 0u,
                    team_id,
                    league_id,
                    (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
                    (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
                    has_assignment,
                    (uint32_t)injury_active,
                    (int)days_left,
                    inactive_roster_eligible);
            }
            continue;
        }
        if (!slot_opening_allowed) {
            continue;
        }
        kbo_lock_foreign_injury_replacements();
        int already_replacement = kbo_foreign_injury_replacement_player_reserved_locked(player_id, NULL);
        kbo_unlock_foreign_injury_replacements();
        if (already_replacement) {
            do {
                KboLogFields audit_fields;
                kbo_log_fields_init(&audit_fields);
                kbo_log_field_u32(&audit_fields, "date", today);
                kbo_log_field_u32(&audit_fields, "player_id", player_id);
                kbo_log_field_u32(&audit_fields, "team_id", team_id);
                kbo_log_field_u32(&audit_fields, "league_id", league_id);
                kbo_log_field_i32(&audit_fields, "effective_days_left", effective_days_left);
                kbo_log_field_i32(&audit_fields, "message_evidence_days", message_evidence_days);
                kbo_log_field_i32(&audit_fields, "sql_evidence_days", sql_evidence_days);
                kbo_log_field_u32(&audit_fields, "sql_evidence_date", sql_evidence_date);
                kbo_rule_audit_emit_fields(
                    "foreign_injury.replacement.lifecycle",
                    "skip_candidate",
                    "replacement_player_cannot_open_nested_slot",
                    source,
                    &audit_fields);
            } while (0);
            continue;
        }
        KboForeignInjuryReplacement created_rec;
        memset(&created_rec, 0, sizeof(created_rec));
        int created = 0;
        int updated_expected_end = 0;
        KboForeignInjuryReplacement updated_rec;
        memset(&updated_rec, 0, sizeof(updated_rec));
        uint32_t candidate_expected_end = (direct_injury_eligible || message_injury_eligible || sql_injury_eligible)
            ? kbo_foreign_injury_expected_end_from_duration(
                sql_injury_eligible && sql_evidence_date != 0u ? sql_evidence_date : today,
                effective_days_left)
            : 0u;
        uint32_t candidate_opened_on = sql_injury_eligible && sql_evidence_date != 0u
            ? sql_evidence_date
            : today;
        if ((message_injury_eligible || sql_injury_eligible)
                && candidate_expected_end != 0u
                && kbo_foreign_injury_expected_end_reached(today, candidate_expected_end)) {
            do {
                KboLogFields audit_fields;
                kbo_log_fields_init(&audit_fields);
                kbo_log_field_u32(&audit_fields, "date", today);
                kbo_log_field_u32(&audit_fields, "player_id", player_id);
                kbo_log_field_u32(&audit_fields, "team_id", team_id);
                kbo_log_field_u32(&audit_fields, "league_id", league_id);
                kbo_log_field_i32(&audit_fields, "effective_days_left", effective_days_left);
                kbo_log_field_u32(&audit_fields, "opened_on", candidate_opened_on);
                kbo_log_field_u32(&audit_fields, "expected_end", candidate_expected_end);
                kbo_log_field_i32(&audit_fields, "message_evidence_days", message_evidence_days);
                kbo_log_field_i32(&audit_fields, "sql_evidence_days", sql_evidence_days);
                kbo_log_field_u32(&audit_fields, "sql_evidence_date", sql_evidence_date);
                kbo_rule_audit_emit_fields(
                    "foreign_injury.replacement.lifecycle",
                    "skip_candidate",
                    "long_term_evidence_already_expired",
                    source,
                    &audit_fields);
            } while (0);
            continue;
        }
        kbo_lock_foreign_injury_replacements();
        int existing = kbo_find_foreign_injury_replacement_locked(player_id, 0);
        int closed_existing = existing < 0 ? kbo_find_foreign_injury_replacement_locked(player_id, 1) : -1;
        if (existing < 0 && closed_existing >= 0) {
            KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[closed_existing];
            if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED) {
                if (!direct_injury_eligible) {
                    kbo_unlock_foreign_injury_replacements();
                    continue;
                }
                existing = closed_existing;
            }
        }
        if (existing >= 0 && (message_injury_eligible || sql_injury_eligible) && candidate_expected_end != 0u) {
            KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[existing];
            uint32_t evidence_anchor = sql_injury_eligible && sql_evidence_date != 0u
                ? sql_evidence_date
                : (rec->opened_on_yyyymmdd != 0u
                ? rec->opened_on_yyyymmdd
                : today);
            uint32_t evidence_expected_end = kbo_foreign_injury_expected_end_from_duration(
                evidence_anchor,
                effective_days_left);
            if (rec->status != KBO_FOREIGN_INJURY_STATUS_CLOSED
                    && evidence_expected_end != 0u
                    && (rec->expected_end_yyyymmdd == 0u || evidence_expected_end > rec->expected_end_yyyymmdd)) {
                rec->expected_end_yyyymmdd = evidence_expected_end;
                updated_rec = *rec;
                updated_expected_end = kbo_persist_foreign_injury_replacements_locked();
            }
        }
        if (existing >= 0) {
            KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[existing];
            if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED && direct_injury_eligible) {
                rec->team_id = team_id;
                rec->league_id = league_id != 0u ? league_id : configured_league_id;
                rec->replacement_player_id = 0u;
                rec->opened_on_yyyymmdd = candidate_opened_on;
                rec->expected_end_yyyymmdd = candidate_expected_end;
                rec->slot_type = kbo_foreign_injury_slot_type_for_player(player);
                rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
                rec->converted = 0u;
                created_rec = *rec;
                created = kbo_persist_foreign_injury_replacements_locked();
            }
        }
        if (existing < 0 && g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
            KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++];
            rec->team_id = team_id;
            rec->league_id = league_id != 0u ? league_id : configured_league_id;
            rec->injured_player_id = player_id;
            rec->replacement_player_id = 0u;
            rec->opened_on_yyyymmdd = candidate_opened_on;
            rec->expected_end_yyyymmdd = candidate_expected_end;
            rec->slot_type = kbo_foreign_injury_slot_type_for_player(player);
            rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
            rec->converted = 0u;
            created_rec = *rec;
            created = kbo_persist_foreign_injury_replacements_locked();
        }
        kbo_unlock_foreign_injury_replacements();
        if (updated_expected_end) {
            do {
                KboLogFields audit_fields;
                kbo_log_fields_init(&audit_fields);
                kbo_log_field_u32(&audit_fields, "date", today);
                kbo_log_field_u32(&audit_fields, "team_id", updated_rec.team_id);
                kbo_log_field_u32(&audit_fields, "league_id", updated_rec.league_id);
                kbo_log_field_u32(&audit_fields, "injured_player_id", updated_rec.injured_player_id);
                kbo_log_field_u32(&audit_fields, "expected_end", updated_rec.expected_end_yyyymmdd);
                kbo_log_field_i32(&audit_fields, "message_evidence_days", message_evidence_days);
                kbo_log_field_i32(&audit_fields, "sql_evidence_days", sql_evidence_days);
                kbo_rule_audit_emit_fields(
                    "foreign_injury.replacement.lifecycle",
                    "update_slot",
                    sql_injury_eligible ? "sql_expected_end_refined" : "message_expected_end_refined",
                    source,
                    &audit_fields);
            } while (0);
        }
        if (created) {
            opened++;
            int open_news_allowed = kbo_foreign_injury_open_news_allowed(
                today,
                live_date,
                created_rec.opened_on_yyyymmdd,
                process_existing_replacements,
                captured_live_date);
            if (open_news_allowed) {
                kbo_emit_foreign_injury_replacement_news_on_date(
                    &created_rec,
                    effective_days_left,
                    direct_injury_eligible ? "open" : "open_roster",
                    today);
            } else {
                do {
                    KboLogFields audit_fields;
                    kbo_log_fields_init(&audit_fields);
                    kbo_log_field_u32(&audit_fields, "date", today);
                    kbo_log_field_u32(&audit_fields, "live_date", live_date);
                    kbo_log_field_u32(&audit_fields, "team_id", created_rec.team_id);
                    kbo_log_field_u32(&audit_fields, "league_id", created_rec.league_id);
                    kbo_log_field_u32(&audit_fields, "injured_player_id", created_rec.injured_player_id);
                    kbo_log_field_u32(&audit_fields, "opened_on", created_rec.opened_on_yyyymmdd);
                    kbo_log_field_u32(&audit_fields, "discovery_only", process_existing_replacements ? 0u : 1u);
                    kbo_log_field_u32(&audit_fields, "captured_live_date", captured_live_date ? 1u : 0u);
                    kbo_rule_audit_emit_fields(
                        "foreign_injury.replacement.lifecycle",
                        "suppress_news",
                        "open_slot_not_live_date",
                        source,
                        &audit_fields);
                } while (0);
                kbo_log_runtimef(
                    "foreign injury replacement: suppressed open news source=%s team=%u injured=%u league=%u scan_date=%u live_date=%u opened_on=%u discovery_only=%d captured_live_date=%d",
                    source != NULL ? source : "",
                    created_rec.team_id,
                    created_rec.injured_player_id,
                    created_rec.league_id,
                    today,
                    live_date,
                    created_rec.opened_on_yyyymmdd,
                    process_existing_replacements ? 0 : 1,
                    captured_live_date ? 1 : 0);
            }
            do {
                KboLogFields audit_fields;
                kbo_log_fields_init(&audit_fields);
                kbo_log_field_u32(&audit_fields, "date", today);
                kbo_log_field_u32(&audit_fields, "team_id", created_rec.team_id);
                kbo_log_field_u32(&audit_fields, "league_id", created_rec.league_id);
                kbo_log_field_u32(&audit_fields, "injured_player_id", created_rec.injured_player_id);
                kbo_log_field_u32(&audit_fields, "injury_active", (uint32_t)injury_active);
                kbo_log_field_i32(&audit_fields, "days_left", (int)days_left);
                kbo_log_field_i32(&audit_fields, "min_days", min_days);
                kbo_log_field_i32(&audit_fields, "effective_days_left", effective_days_left);
                kbo_log_field_u32(&audit_fields, "inactive_roster", inactive_roster_present ? 1u : 0u);
                kbo_log_field_i32(&audit_fields, "message_evidence_days", message_evidence_days);
                kbo_log_field_i32(&audit_fields, "sql_evidence_days", sql_evidence_days);
                kbo_log_field_u32(&audit_fields, "sql_evidence_date", sql_evidence_date);
                kbo_log_field_u32(&audit_fields, "opened_on", created_rec.opened_on_yyyymmdd);
                kbo_log_field_u32(&audit_fields, "expected_end", created_rec.expected_end_yyyymmdd);
                kbo_log_field_u32(&audit_fields, "slot_type", (uint32_t)created_rec.slot_type);
                kbo_rule_audit_emit_fields(
                    "foreign_injury.replacement.lifecycle",
                    "open_slot",
                    direct_injury_eligible
                        ? "injury_min_days_met"
                        : (sql_injury_eligible
                            ? "inactive_roster_long_term_injury_sql"
                            : (message_injury_eligible
                            ? "inactive_roster_long_term_injury_news"
                            : "inactive_roster_long_term_il")),
                    source,
                    &audit_fields);
            } while (0);
            kbo_log_runtimef(
                "foreign injury replacement: opened source=%s team=%u player=%u league=%u days_left=%d effective_days_left=%d trigger=%s slot=%s",
                source != NULL ? source : "",
                created_rec.team_id,
                created_rec.injured_player_id,
                created_rec.league_id,
                (int)days_left,
                effective_days_left,
                direct_injury_eligible ? "injury_fields" : (sql_injury_eligible ? "injury_sql" : (message_injury_eligible ? "injury_news" : "inactive_roster")),
                kbo_foreign_injury_slot_label(created_rec.slot_type));
        }
        }
        KBO_PROFILE_END(profile_foreign_injury_player_loop, "foreign_injury.scan.player_loop");
    }
    int active_count = 0;
    int closed_count = 0;
    if (process_existing_replacements) {
        KBO_PROFILE_BEGIN(profile_foreign_injury_existing);
        kbo_foreign_injury_process_existing_replacements(today, source, &active_count, &closed_count);
        KBO_PROFILE_END(profile_foreign_injury_existing, "foreign_injury.scan.existing_replacements");
    } else {
        kbo_profiler_record_us("foreign_injury.scan.discovery_only_existing_skipped", 0);
    }
    if (opened > 0 || active_count > 0 || closed_count > 0) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_log_field_i32(&audit_fields, "scanned_foreign", scanned);
            kbo_log_field_i32(&audit_fields, "opened", opened);
            kbo_log_field_i32(&audit_fields, "activated", active_count);
            kbo_log_field_i32(&audit_fields, "closed", closed_count);
            kbo_log_field_u32(&audit_fields, "discovery_only", process_existing_replacements ? 0u : 1u);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.scan",
                "process",
                "lifecycle_changes",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign injury replacement: scan source=%s scanned_foreign=%d opened=%d active=%d pending=%d closed=%d discovery_only=%d",
            source != NULL ? source : "",
            scanned,
            opened,
            active_count,
            0,
            closed_count,
            process_existing_replacements ? 0 : 1);
    }
    kbo_foreign_injury_note_same_date_idle_scan(
        today,
        source,
        opened == 0 && active_count == 0 && closed_count == 0);
    KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.total");
}
