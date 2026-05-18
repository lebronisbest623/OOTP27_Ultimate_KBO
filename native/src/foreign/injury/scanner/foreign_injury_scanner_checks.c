#include "foreign_injury_scanner_internal.h"
#include "../../rights/query/foreign_waiver_rights_query.h"

int kbo_foreign_injury_player_has_baseball_position(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET, sizeof(uint8_t))) {
        return 0;
    }

    uint8_t position = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
    uint8_t role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
    return (position >= 1u && position <= 10u) || (role >= 1u && role <= 13u);
}

int kbo_foreign_injury_player_matches_team(uint8_t* player, uint32_t team_id)
{
    if (player == NULL || team_id == 0u || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == team_id
        || *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == team_id;
}

int kbo_foreign_injury_candidate_matches_slot(uint8_t* player, uint8_t slot_type)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    uint8_t player_slot = kbo_foreign_injury_slot_type_for_player(player);
    return player_slot == slot_type;
}

int kbo_foreign_injury_team_active_roster_contains_player(uint8_t* team, uint32_t player_id)
{
    if (team == NULL
            || player_id == 0u
            || !memory_range_readable(team + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t))) {
        return 0;
    }

    uint32_t* active_ids = (uint32_t*)(team + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (active_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

static int kbo_foreign_injury_team_roster_array_contains_player(
    uint8_t* team,
    uint32_t array_offset,
    uint32_t player_id)
{
    if (team == NULL
            || player_id == 0u
            || !memory_range_readable(team + array_offset, OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t))) {
        return 0;
    }

    uint32_t* ids = (uint32_t*)(team + array_offset);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

int kbo_foreign_injury_team_inactive_roster_contains_player(uint8_t* team, uint32_t player_id)
{
    return kbo_foreign_injury_team_roster_array_contains_player(
        team,
        OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET,
        player_id);
}

static int kbo_foreign_injury_inactive_candidate_team_matches(uint32_t team_id, uint32_t player_id)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    if (kbo_foreign_injury_team_active_roster_contains_player(team, player_id)) {
        return 0;
    }
    return kbo_foreign_injury_team_inactive_roster_contains_player(team, player_id);
}

int kbo_foreign_injury_player_on_inactive_replacement_roster(
    uint8_t* player,
    uint32_t player_id,
    uint32_t top_team_id,
    uint32_t today_yyyymmdd)
{
    if (player == NULL
            || player_id == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    if (top_team_id != 0u
            && today_yyyymmdd != 0u
            && kbo_has_active_foreign_waiver_right(top_team_id, player_id, today_yyyymmdd)) {
        return 0;
    }

    uint32_t candidates[6] = {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
        memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
            : 0u,
        top_team_id
    };

    for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); i++) {
        uint32_t candidate_team_id = candidates[i];
        if (candidate_team_id == 0u) {
            continue;
        }

        int duplicate = 0;
        for (int j = 0; j < i; j++) {
            if (candidates[j] == candidate_team_id) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        if (kbo_foreign_injury_inactive_candidate_team_matches(candidate_team_id, player_id)) {
            return 1;
        }

        uint8_t* team = find_kbo_team_by_numeric_id_any_league(candidate_team_id, 1);
        if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u
                && parent_team_id != candidate_team_id
                && kbo_foreign_injury_inactive_candidate_team_matches(parent_team_id, player_id)) {
            return 1;
        }
    }

    return 0;
}

int kbo_foreign_injury_team_known_roster_contains_player(uint8_t* team, uint32_t player_id)
{
    return kbo_foreign_injury_team_roster_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, player_id)
        || kbo_foreign_injury_team_roster_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, player_id)
        || kbo_foreign_injury_team_roster_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_2DA0_OFFSET, player_id)
        || kbo_foreign_injury_team_roster_array_contains_player(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id)
        || kbo_foreign_injury_team_roster_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_33E0_OFFSET, player_id)
        || kbo_foreign_injury_team_roster_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_3700_OFFSET, player_id);
}

static uint32_t kbo_foreign_injury_org_team_id_for_team(uint32_t team_id, uint32_t* out_league_id)
{
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (team_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    if (parent_team_id != 0u) {
        uint8_t* parent = find_kbo_team_by_numeric_id_any_league(parent_team_id, 1);
        if (parent != NULL && memory_range_readable(parent, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            if (out_league_id != NULL) {
                *out_league_id = *(uint32_t*)(parent + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            }
            return parent_team_id;
        }
        if (out_league_id != NULL) {
            *out_league_id = league_id;
        }
        return parent_team_id;
    }

    if (out_league_id != NULL) {
        *out_league_id = league_id;
    }
    return team_id;
}

static int kbo_foreign_injury_try_player_team_assignment(
    uint8_t* player,
    uint32_t player_id,
    uint32_t candidate_team_id,
    uint32_t configured_league_id,
    uint32_t* out_team_id,
    uint32_t* out_league_id)
{
    if (player == NULL || player_id == 0u || candidate_team_id == 0u) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(candidate_team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t org_league_id = 0u;
    uint32_t org_team_id = kbo_foreign_injury_org_team_id_for_team(candidate_team_id, &org_league_id);
    uint8_t* org_team = org_team_id != 0u ? find_kbo_team_by_numeric_id_any_league(org_team_id, 1) : NULL;
    int rostered = kbo_foreign_injury_team_known_roster_contains_player(team, player_id)
        || (org_team != NULL
            && memory_range_readable(org_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            && kbo_foreign_injury_team_known_roster_contains_player(org_team, player_id));

    uint32_t league_id = org_league_id;
    if (league_id == 0u) {
        league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    }
    if (league_id == 0u) {
        league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    }
    if (configured_league_id != 0u && league_id != 0u && league_id != configured_league_id) {
        return 0;
    }

    if (!rostered) {
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t player_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (candidate_team_id != current_team_id
                && candidate_team_id != active_team_id
                && org_team_id != current_team_id
                && org_team_id != active_team_id) {
            return 0;
        }
        if (configured_league_id != 0u
                && player_league_id != 0u
                && player_league_id != configured_league_id) {
            return 0;
        }
    }

    if (out_team_id != NULL) {
        *out_team_id = org_team_id != 0u ? org_team_id : candidate_team_id;
    }
    if (out_league_id != NULL) {
        *out_league_id = league_id != 0u ? league_id : configured_league_id;
    }
    return 1;
}

int kbo_foreign_injury_resolve_player_team_assignment(
    uint8_t* player,
    uint32_t player_id,
    uint32_t configured_league_id,
    uint32_t* out_team_id,
    uint32_t* out_league_id)
{
    if (out_team_id != NULL) { *out_team_id = 0u; }
    if (out_league_id != NULL) { *out_league_id = 0u; }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t candidates[5] = {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
        memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
            : 0u
    };

    for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); i++) {
        if (candidates[i] == 0u) {
            continue;
        }
        int duplicate = 0;
        for (int j = 0; j < i; j++) {
            if (candidates[j] == candidates[i]) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        if (kbo_foreign_injury_try_player_team_assignment(
                player,
                player_id,
                candidates[i],
                configured_league_id,
                out_team_id,
                out_league_id)) {
            return 1;
        }
    }

    return 0;
}

int kbo_foreign_injury_injured_player_returned_to_org_roster(
    const KboForeignInjuryReplacement* rec,
    uint8_t* injured)
{
    if (rec == NULL || injured == NULL || rec->team_id == 0u
            || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    KboForeignInjuryLiveMemory live_injury;
    memset(&live_injury, 0, sizeof(live_injury));
    kbo_foreign_injury_read_live_memory(injured, &live_injury);
    if (live_injury.active != 0u || live_injury.days_left > 0) {
        return 0;
    }
    if (injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u) {
        return 0;
    }
    if (injured[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
            || injured[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u
            || injured[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u) {
        return 0;
    }

    uint32_t resolved_team_id = 0u;
    uint32_t resolved_league_id = 0u;
    if (!kbo_foreign_injury_resolve_player_team_assignment(
            injured,
            rec->injured_player_id,
            rec->league_id,
            &resolved_team_id,
            &resolved_league_id)) {
        return 0;
    }
    if (resolved_team_id != rec->team_id) {
        return 0;
    }
    if (rec->league_id != 0u && resolved_league_id != 0u && resolved_league_id != rec->league_id) {
        return 0;
    }

    uint8_t* top_team = find_kbo_team_by_numeric_id_any_league(rec->team_id, 1);
    if (top_team == NULL || !memory_range_readable(top_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    return kbo_foreign_injury_team_active_roster_contains_player(top_team, rec->injured_player_id)
        && !kbo_foreign_injury_team_inactive_roster_contains_player(top_team, rec->injured_player_id);
}

uint32_t kbo_foreign_injury_resolve_replacement_for_record(const KboForeignInjuryReplacement* rec)
{
    if (rec == NULL || rec->team_id == 0u || rec->injured_player_id == 0u) {
        return 0u;
    }
    if (rec->replacement_player_id != 0u) {
        return rec->replacement_player_id;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0u;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u
                || player_id == rec->injured_player_id
                || !kbo_player_is_foreign_for_kbo_rights(player)
                || !kbo_foreign_replacement_player_seed_matches_loaded(player, NULL)
                || !kbo_foreign_injury_player_matches_team(player, rec->team_id)
                || !kbo_foreign_injury_candidate_matches_slot(player, rec->slot_type)
                || kbo_foreign_injury_replacement_player_reserved_locked(player_id, rec)) {
            continue;
        }

        if (kbo_foreign_injury_runtime_injury_present_from_memory(player)) {
            continue;
        }
        return player_id;
    }

    return 0u;
}

