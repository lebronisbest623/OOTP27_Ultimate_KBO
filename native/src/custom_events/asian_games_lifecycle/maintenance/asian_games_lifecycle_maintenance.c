#include "asian_games_lifecycle_maintenance.h"
#include "asian_games_lifecycle_maintenance_policy.h"

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../military_service/players/state/military_player_state.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../asian_games/player_eval/asian_games_player_eval.h"
#include "../../asian_games/roster/asian_games_roster_store.h"
#include "../../asian_games/state/asian_games_state.h"
#include "../../asian_games_schedule_seed/state/state_and_paths.h"
#include "../../asian_games_schedule_seed/query/query_helpers.h"

static uint32_t kbo_asian_games_lifecycle_year(uint32_t today_yyyymmdd)
{
    return g_kbo_asian_games_roster_year != 0u
        ? g_kbo_asian_games_roster_year
        : today_yyyymmdd / 10000u;
}

static int kbo_asian_games_fill_missing_lifecycle_dates(
    KboAsianGamesRosterEntry* entry,
    uint32_t today_yyyymmdd)
{
    if (entry == NULL) {
        return 0;
    }

    int changed = 0;
    uint32_t year = kbo_asian_games_lifecycle_year(today_yyyymmdd);
    KboAsianGamesScheduleSeed schedule;
    if ((entry->departure_date == 0u || entry->return_date == 0u)
            && kbo_get_asian_games_schedule_for_year(year, &schedule)) {
        if (entry->departure_date == 0u && schedule.departure_date != 0u) {
            entry->departure_date = schedule.departure_date;
            changed = 1;
        }

        uint32_t return_date = schedule.final_date != 0u
            ? schedule.final_date
            : schedule.tournament_end;
        if (entry->return_date == 0u && return_date != 0u) {
            entry->return_date = return_date;
            changed = 1;
        }
    }

    if (entry->return_date == 0u) {
        uint32_t return_date = kbo_asian_games_final_date_for_year(year);
        if (return_date != 0u) {
            entry->return_date = return_date;
            changed = 1;
        }
    }

    return changed;
}

static int kbo_asian_games_set_u32_field(uint8_t* player, uint32_t offset, uint32_t value)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint32_t))) {
        return 0;
    }
    uint32_t* slot = (uint32_t*)(player + offset);
    if (*slot == value) {
        return 0;
    }
    *slot = value;
    return 1;
}

static int kbo_asian_games_set_u8_field(uint8_t* player, uint32_t offset, uint8_t value)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint8_t))) {
        return 0;
    }
    if (player[offset] == value) {
        return 0;
    }
    player[offset] = value;
    return 1;
}

static int kbo_asian_games_maintain_restricted_entry(
    KboAsianGamesRosterEntry* entry,
    uint32_t today_yyyymmdd,
    const char* source,
    LONG roster_index,
    int* out_missing,
    int* out_no_team,
    int* out_restricted_full)
{
    uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        if (out_missing != NULL) {
            (*out_missing)++;
        }
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t hold_team_id = entry->original_team_id != 0u
        ? entry->original_team_id
        : current_team_id;
    uint32_t hold_league_id = entry->original_league_id != 0u
        ? entry->original_league_id
        : current_league_id;
    if (hold_team_id == 0u) {
        if (out_no_team != NULL) {
            (*out_no_team)++;
        }
        return 0;
    }

    uint8_t* hold_team = find_kbo_team_by_numeric_id_any_league(hold_team_id, 0);
    if (hold_team == NULL) {
        if (out_no_team != NULL) {
            (*out_no_team)++;
        }
        return 0;
    }

    uint8_t* current_team = NULL;
    if (current_team_id != 0u && current_team_id != hold_team_id) {
        current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
    }

    uint8_t* active_team = NULL;
    if (active_team_id != 0u && active_team_id != hold_team_id && active_team_id != current_team_id) {
        active_team = find_kbo_team_by_numeric_id_any_league(active_team_id, 0);
    }

    int restricted_present_before = kbo_team_fixed_array_contains_player(
        hold_team,
        OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET,
        entry->player_id);
    uint8_t before_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t before_secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t before_injury = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    int32_t before_days_left = kbo_military_days_left(player);

    int removed_hold_active = kbo_remove_player_id_from_team_active_roster_arrays(hold_team, entry->player_id);
    int removed_current = current_team != NULL
        ? kbo_remove_player_id_from_known_team_roster_arrays(current_team, entry->player_id)
        : 0;
    int removed_active = active_team != NULL
        ? kbo_remove_player_id_from_known_team_roster_arrays(active_team, entry->player_id)
        : 0;
    int added_restricted = kbo_add_player_id_to_team_fixed_array(
        hold_team,
        OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET,
        entry->player_id);
    if (!restricted_present_before && !added_restricted && out_restricted_full != NULL) {
        (*out_restricted_full)++;
    }

    int field_changes = 0;
    field_changes += kbo_asian_games_set_u32_field(
        player,
        OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET,
        hold_team_id);
    if (hold_league_id != 0u) {
        field_changes += kbo_asian_games_set_u32_field(
            player,
            OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET,
            hold_league_id);
    }
    field_changes += kbo_asian_games_set_u32_field(
        player,
        OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET,
        0u);
    field_changes += kbo_asian_games_set_u8_field(
        player,
        OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET,
        1u);
    field_changes += kbo_asian_games_set_u8_field(
        player,
        OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET,
        1u);
    field_changes += kbo_asian_games_set_u8_field(
        player,
        OOTP27_PLAYER_INJURY_ACTIVE_OFFSET,
        1u);

    int32_t days_left = kbo_asian_games_restricted_days_left(today_yyyymmdd, entry->return_date);
    if (before_days_left != days_left) {
        kbo_set_military_days_left(player, days_left);
        field_changes++;
    }

    int repaired = removed_hold_active > 0
        || removed_current > 0
        || removed_active > 0
        || !restricted_present_before
        || field_changes > 0;
    if (repaired) {
        kbo_log_runtimef(
            "KBO Asian Games restricted hold maintained source=%s date=%u index=%ld player_id=%u hold_team=%u hold_league=%u current_team_before=%u active_team_before=%u removed_hold_active=%d removed_current=%d removed_active=%d restricted_before=%d added_restricted=%d old_restricted=%u old_secondary=%u old_injury=%u old_days=%d days_left=%d field_changes=%d",
            source != NULL ? source : "",
            today_yyyymmdd,
            roster_index + 1,
            entry->player_id,
            hold_team_id,
            hold_league_id,
            current_team_id,
            active_team_id,
            removed_hold_active,
            removed_current,
            removed_active,
            restricted_present_before,
            added_restricted,
            (uint32_t)before_restricted,
            (uint32_t)before_secondary,
            (uint32_t)before_injury,
            (int)before_days_left,
            (int)days_left,
            field_changes);
    }
    return repaired ? 1 : 0;
}

int kbo_maintain_asian_games_restricted_players(uint32_t today_yyyymmdd, const char* source)
{
    if (today_yyyymmdd == 0u) {
        return 0;
    }

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_load_asian_games_roster_csv(source);
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        return 0;
    }

    int date_repairs = 0;
    int active = 0;
    int repaired = 0;
    int missing = 0;
    int no_team = 0;
    int restricted_full = 0;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->departed == 0u || entry->returned != 0u) {
            continue;
        }

        date_repairs += kbo_asian_games_fill_missing_lifecycle_dates(entry, today_yyyymmdd);
        if (!kbo_asian_games_roster_entry_needs_restricted_hold(entry, today_yyyymmdd)) {
            continue;
        }

        active++;
        repaired += kbo_asian_games_maintain_restricted_entry(
            entry,
            today_yyyymmdd,
            source,
            i,
            &missing,
            &no_team,
            &restricted_full);
    }

    if (date_repairs > 0) {
        kbo_save_asian_games_roster_csv(source);
    }
    if (active > 0 && (repaired > 0 || missing > 0 || no_team > 0 || restricted_full > 0)) {
        kbo_log_runtimef(
            "KBO Asian Games restricted hold maintenance source=%s date=%u active=%d repaired=%d date_repairs=%d missing=%d no_team=%d restricted_full=%d",
            source != NULL ? source : "",
            today_yyyymmdd,
            active,
            repaired,
            date_repairs,
            missing,
            no_team,
            restricted_full);
    }
    return repaired + date_repairs;
}
