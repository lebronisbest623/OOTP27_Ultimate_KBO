#include "../foreign_injury_scanner_player_loop_internal.h"

#include "../../../../common/policy/foreign_player_policy.h"
#include "../../../../../team/add_player_guard/team_add_player_guard_ai_roster.h"

static LONG g_kbo_foreign_injury_non_roster_log_count = 0;
static LONG g_kbo_foreign_injury_below_min_log_count = 0;

KboForeignInjuryScannerPlayerLoopResult kbo_foreign_injury_scan_player_for_replacement(
    uintptr_t player_ptr,
    const KboForeignInjuryScannerPlayerLoopContext* context)
{
    KboForeignInjuryScannerPlayerLoopResult result = {0};
    if (context == NULL) {
        return result;
    }
    uint32_t configured_league_id = context->configured_league_id;
    uint32_t today = context->today;
    uint32_t live_date = context->live_date;
    int live_injury_fields_available = context->live_injury_fields_available;
    int process_existing_replacements = context->process_existing_replacements;
    int captured_live_date = context->captured_live_date;
    const char* source = context->source;
    if (!kbo_player_pointer_plausible(player_ptr)) {
        return result;
    }
    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_active_for_roster_scan(player)) {
        return result;
    }
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return result;
    }
    result.scanned++;
    int has_baseball_position = kbo_foreign_injury_player_has_baseball_position(player);
    if (!has_baseball_position) {
        return result;
    }
    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    int has_assignment = kbo_foreign_injury_resolve_player_team_assignment(
        player,
        player_id,
        configured_league_id,
        &team_id,
        &league_id);
    if (!has_assignment) {
        return result;
    }
    KboForeignInjuryLiveMemory live_injury;
    memset(&live_injury, 0, sizeof(live_injury));
    int live_memory_read = live_injury_fields_available
        ? kbo_foreign_injury_read_live_memory(player, &live_injury)
        : 0;
    uint8_t injury_active = live_injury.active;
    int days_left = live_injury.days_left;
    int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
    int direct_injury_eligible = live_injury_fields_available
        && live_memory_read
        && kbo_foreign_injury_live_memory_has_long_term_basis(&live_injury, min_days);
    int inactive_roster_present = live_injury_fields_available
        ? kbo_foreign_injury_player_on_inactive_replacement_roster(player, player_id, team_id, today)
        : 0;
    if (live_injury_fields_available) {
        kbo_foreign_injury_memory_probe_observe(
            player,
            player_id,
            team_id,
            league_id,
            kbo_foreign_injury_slot_type_for_player(player),
            today,
            injury_active,
            (int16_t)days_left,
            inactive_roster_present,
            has_assignment,
            source);
    }
    if (!inactive_roster_present && injury_active == 0u && days_left <= 0) {
        kbo_foreign_injury_pending_diagnosis_clear(player_id);
    }
    if (!direct_injury_eligible) {
        if (injury_active != 0u || days_left > 0 || inactive_roster_present) {
            if (inactive_roster_present
                    && has_assignment
                    && has_baseball_position) {
                kbo_foreign_injury_note_pending_diagnosis(
                    player_id,
                    team_id,
                    league_id,
                    kbo_foreign_injury_slot_type_for_player(player),
                    today,
                    source);
            }
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
                    kbo_log_field_i32(&audit_fields, "days_left", days_left);
                    kbo_log_field_i32(&audit_fields, "active_injury_count", live_injury.active_count);
                    kbo_log_field_u32(&audit_fields, "injury_id", live_injury.injury_id);
                    kbo_log_field_ptr(&audit_fields, "injury_object", (const void*)live_injury.injury_object);
                    kbo_log_field_u32(&audit_fields, "pending_diagnosis", live_injury.pending_diagnosis ? 1u : 0u);
                    kbo_log_field_u32(&audit_fields, "day_to_day", live_injury.day_to_day ? 1u : 0u);
                    kbo_log_field_u32(&audit_fields, "career_ending", live_injury.career_ending ? 1u : 0u);
                    kbo_log_field_i32(&audit_fields, "min_days", min_days);
                    kbo_log_field_u32(&audit_fields, "inactive_roster", inactive_roster_present ? 1u : 0u);
                    kbo_log_field_u32(&audit_fields, "assignment", has_assignment ? 1u : 0u);
                    kbo_rule_audit_emit_fields(
                        "foreign_injury.replacement.lifecycle",
                        "skip_candidate",
                        live_injury.pending_diagnosis
                            ? "live_injury_pending_diagnosis"
                            : (inactive_roster_present
                            ? "inactive_roster_without_long_term_injury_days"
                            : "live_injury_below_min_days"),
                        source,
                        &audit_fields);
                } while (0);
                kbo_log_runtimef(
                    "foreign injury replacement: skipped live injury candidate source=%s player=%u current=%u active=%u loan=%u original=%u default=%u resolved_team=%u league=%u assignment=%d injury=%u injury_count=%d injury_id=%u days_left=%d min_days=%d pending=%u day_to_day=%u inactive_roster=%d",
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
                    live_injury.active_count,
                    live_injury.injury_id,
                    days_left,
                    min_days,
                    (uint32_t)live_injury.pending_diagnosis,
                    (uint32_t)live_injury.day_to_day,
                    inactive_roster_present);
            }
        }
        return result;
    }
    int effective_days_left = days_left;
    if (effective_days_left < min_days) {
        effective_days_left = min_days;
    }
    if (!has_assignment || !has_baseball_position) {
        LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_non_roster_log_count);
        if (log_slot <= 60 || (log_slot % 100) == 0) {
            kbo_log_runtimef(
                "foreign injury replacement: skipped non-roster injury candidate source=%s player=%u current=%u active=%u loan=%u original=%u default=%u resolved_team=%u league=%u position=%u role=%u assignment=%d injury=%u injury_count=%d injury_id=%u days_left=%d inactive_roster=%d",
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
                live_injury.active_count,
                live_injury.injury_id,
                days_left,
                inactive_roster_present);
        }
        return result;
    }
    kbo_lock_foreign_injury_replacements_shared();
    int already_replacement = kbo_foreign_injury_replacement_player_reserved_locked(player_id, NULL);
    kbo_unlock_foreign_injury_replacements_shared();
    if (already_replacement) {
        do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_log_field_u32(&audit_fields, "player_id", player_id);
            kbo_log_field_u32(&audit_fields, "team_id", team_id);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_i32(&audit_fields, "effective_days_left", effective_days_left);
            kbo_log_field_i32(&audit_fields, "active_injury_count", live_injury.active_count);
            kbo_log_field_u32(&audit_fields, "injury_id", live_injury.injury_id);
            kbo_log_field_ptr(&audit_fields, "injury_object", (const void*)live_injury.injury_object);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.lifecycle",
                "skip_candidate",
                "replacement_player_cannot_open_nested_slot",
                source,
                &audit_fields);
        } while (0);
        return result;
    }
    KboForeignInjuryReplacement created_rec;
    memset(&created_rec, 0, sizeof(created_rec));
    int created = 0;
    int updated_expected_end = 0;
    KboForeignInjuryReplacement updated_rec;
    memset(&updated_rec, 0, sizeof(updated_rec));
    uint32_t candidate_expected_end = direct_injury_eligible
        ? kbo_foreign_injury_expected_end_from_duration(today, effective_days_left)
        : 0u;
    uint32_t candidate_opened_on = today;
    int roster_hold_flags_present = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u;
    kbo_lock_foreign_injury_replacements();
    int existing = kbo_find_foreign_injury_replacement_locked(player_id, 0);
    if (existing < 0
            && kbo_foreign_injury_closed_record_should_repair_locked(
                player_id,
                &live_injury,
                today,
                inactive_roster_present,
                roster_hold_flags_present)) {
        kbo_unlock_foreign_injury_replacements();
        return result;
    }
    if (existing >= 0 && direct_injury_eligible && candidate_expected_end != 0u) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[existing];
        if (rec->status != KBO_FOREIGN_INJURY_STATUS_CLOSED
                && (rec->expected_end_yyyymmdd != candidate_expected_end
                    || (rec->injury_id == 0u && live_injury.injury_id != 0u))) {
            if (rec->expected_end_yyyymmdd != candidate_expected_end) {
                rec->expected_end_yyyymmdd = candidate_expected_end;
            }
            if (rec->injury_id == 0u && live_injury.injury_id != 0u) {
                rec->injury_id = live_injury.injury_id;
            }
            updated_rec = *rec;
            updated_expected_end = kbo_persist_foreign_injury_replacements_locked();
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
        rec->injury_id = live_injury.injury_id;
        rec->closed_on_yyyymmdd = 0u;
        rec->slot_type = kbo_foreign_injury_slot_type_for_player(player);
        rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
        rec->converted = 0u;
        rec->close_choice = 0u;
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
            kbo_log_field_i32(&audit_fields, "live_days_left", effective_days_left);
            kbo_log_field_u32(&audit_fields, "injury_id", live_injury.injury_id);
            kbo_log_field_ptr(&audit_fields, "injury_object", (const void*)live_injury.injury_object);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.lifecycle",
                "update_slot",
                "live_memory_expected_end_refined",
                source,
                &audit_fields);
        } while (0);
    }
    if (created) {
        kbo_foreign_injury_pending_diagnosis_clear(player_id);
        result.opened++;
        kbo_mark_foreign_ai_roster_daily_callup_dirty("foreign_injury_slot_opened");
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
                "open",
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
            kbo_log_field_i32(&audit_fields, "days_left", days_left);
            kbo_log_field_i32(&audit_fields, "active_injury_count", live_injury.active_count);
            kbo_log_field_u32(&audit_fields, "injury_id", live_injury.injury_id);
            kbo_log_field_ptr(&audit_fields, "injury_object", (const void*)live_injury.injury_object);
            kbo_log_field_u32(&audit_fields, "pending_diagnosis", live_injury.pending_diagnosis ? 1u : 0u);
            kbo_log_field_u32(&audit_fields, "day_to_day", live_injury.day_to_day ? 1u : 0u);
            kbo_log_field_u32(&audit_fields, "career_ending", live_injury.career_ending ? 1u : 0u);
            kbo_log_field_i32(&audit_fields, "min_days", min_days);
            kbo_log_field_i32(&audit_fields, "effective_days_left", effective_days_left);
            kbo_log_field_u32(&audit_fields, "inactive_roster", inactive_roster_present ? 1u : 0u);
            kbo_log_field_u32(&audit_fields, "opened_on", created_rec.opened_on_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "expected_end", created_rec.expected_end_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "slot_type", (uint32_t)created_rec.slot_type);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.lifecycle",
                "open_slot",
                "live_memory_injury_min_days_met",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign injury replacement: opened source=%s team=%u player=%u league=%u injury_count=%d injury_id=%u days_left=%d effective_days_left=%d trigger=%s slot=%s opened_on=%u expected_end=%u pending=%u day_to_day=%u career_ending=%u",
            source != NULL ? source : "",
            created_rec.team_id,
            created_rec.injured_player_id,
            created_rec.league_id,
            live_injury.active_count,
            live_injury.injury_id,
            days_left,
            effective_days_left,
            "live_injury_memory",
            kbo_foreign_injury_slot_label(created_rec.slot_type),
            created_rec.opened_on_yyyymmdd,
            created_rec.expected_end_yyyymmdd,
            (uint32_t)live_injury.pending_diagnosis,
            (uint32_t)live_injury.day_to_day,
            (uint32_t)live_injury.career_ending);
    }
    return result;
}
