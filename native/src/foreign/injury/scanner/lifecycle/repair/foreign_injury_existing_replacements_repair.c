#include "../foreign_injury_existing_replacements_internal.h"
#include "../player_state/foreign_injury_existing_replacements_player_state.h"

int kbo_foreign_injury_repair_closed_existing_replacement(
    KboForeignInjuryReplacement* rec,
    uint32_t today,
    const char* source,
    KboForeignInjuryReplacement* active_news,
    int active_capacity,
    int* active_count,
    KboForeignInjuryTeamLookupCacheEntry* team_cache,
    int* team_cache_count,
    int team_cache_capacity)
{
    if (rec == NULL || rec->status != KBO_FOREIGN_INJURY_STATUS_CLOSED) {
        return 0;
    }
    if (rec->close_choice == KBO_FOREIGN_INJURY_CLOSE_OFFSEASON_RESET) {
        return 0;
    }
    if (rec->converted != 0u || rec->replacement_player_id == 0u) {
        return 0;
    }

    uint8_t* top_team = kbo_foreign_injury_cached_team_lookup(
        rec->team_id,
        team_cache,
        team_cache_count,
        team_cache_capacity);
    int inactive_roster_present = top_team != NULL && memory_range_readable(top_team, OOTP27_KBO_TEAM_READABLE_BYTES)
        ? kbo_foreign_injury_team_inactive_roster_contains_player(top_team, rec->injured_player_id)
        : 0;
    int active_roster_present = top_team != NULL && memory_range_readable(top_team, OOTP27_KBO_TEAM_READABLE_BYTES)
        ? kbo_foreign_injury_team_active_roster_contains_player(top_team, rec->injured_player_id)
        : 0;
    uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
    KboForeignInjuryLiveMemory live_injury;
    memset(&live_injury, 0, sizeof(live_injury));
    kbo_foreign_injury_read_live_memory(injured, &live_injury);
    int runtime_injury_present = kbo_foreign_injury_runtime_injury_present(injured);
    int roster_hold_flags_present = kbo_foreign_injury_roster_hold_flags_present(injured);
    int return_close_allowed = injured != NULL && memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)
        ? kbo_foreign_injury_return_state_allows_close(
            live_injury.active,
            (int16_t)live_injury.days_left,
            injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
            active_roster_present,
            inactive_roster_present,
            roster_hold_flags_present,
            1)
        : 0;
    int repair_allowed = !return_close_allowed
        && !kbo_foreign_injury_replacement_player_reserved_locked(rec->replacement_player_id, rec)
        && kbo_foreign_injury_closed_record_can_repair_on_date(
            rec,
            &live_injury,
            today,
            inactive_roster_present,
            roster_hold_flags_present);
    if (repair_allowed) {
        uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
        repair_allowed = kbo_foreign_injury_replacement_player_attached_to_record(rec, replacement)
            || kbo_foreign_injury_replacement_player_can_restore_to_record(rec, replacement);
    }
    if (!repair_allowed) {
        return 0;
    }

    rec->status = KBO_FOREIGN_INJURY_STATUS_ACTIVE;
    rec->closed_on_yyyymmdd = 0u;
    rec->close_choice = 0u;
    if (rec->injury_id == 0u && live_injury.injury_id != 0u) {
        rec->injury_id = live_injury.injury_id;
    }
    kbo_foreign_injury_restore_active_replacement_player(rec, source);
    if (active_news != NULL && active_count != NULL && *active_count < active_capacity) {
        active_news[(*active_count)++] = *rec;
    }
    kbo_log_runtimef(
        "foreign injury replacement: repaired premature close source=%s team=%u injured=%u replacement=%u league=%u today=%u expected_end=%u reason=closed_before_real_return runtime_injury=%d active_roster=%d inactive_roster=%d roster_hold_flags=%d",
        source != NULL ? source : "",
        rec->team_id,
        rec->injured_player_id,
        rec->replacement_player_id,
        rec->league_id,
        today,
        rec->expected_end_yyyymmdd,
        runtime_injury_present,
        active_roster_present,
        inactive_roster_present,
        roster_hold_flags_present);
    return 1;
}
