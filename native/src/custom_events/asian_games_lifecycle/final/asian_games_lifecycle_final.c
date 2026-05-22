#include "../../runtime/common/custom_events_common.h"
#include "asian_games_lifecycle_final.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../asian_games/history/asian_games_player_history.h"

/* Asian Games final return and exemption lifecycle. */

static uint8_t* kbo_asian_games_find_restricted_team_for_player(uint32_t player_id, uint32_t* out_team_id, uint32_t* out_league_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (player_id == 0u) {
        return NULL;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return NULL;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > KBO_RUNTIME_MAX_TEAM_VECTOR_COUNT
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return NULL;
    }

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0
                || !kbo_team_fixed_array_contains_player(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id)) {
            continue;
        }

        if (out_team_id != NULL) {
            *out_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        if (out_league_id != NULL) {
            *out_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        }
        return team;
    }
    return NULL;
}

static uint32_t kbo_asian_games_result_hash_add(uint32_t hash, uint32_t value)
{
    hash ^= value;
    hash *= 16777619u;
    hash ^= hash >> 13;
    return hash;
}

static uint8_t kbo_asian_games_roll_final_result(
    uint32_t event_yyyymmdd,
    LONG roster_count,
    int no_gold_odds_denominator)
{
    uint32_t hash = 2166136261u;
    hash = kbo_asian_games_result_hash_add(hash, event_yyyymmdd);
    hash = kbo_asian_games_result_hash_add(hash, g_kbo_asian_games_roster_year);
    hash = kbo_asian_games_result_hash_add(hash, (uint32_t)roster_count);
    for (LONG i = 0; i < roster_count && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        const KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        hash = kbo_asian_games_result_hash_add(hash, entry->player_id);
        hash = kbo_asian_games_result_hash_add(hash, entry->original_team_id);
        hash = kbo_asian_games_result_hash_add(hash, (uint32_t)entry->age);
        hash = kbo_asian_games_result_hash_add(hash, (uint32_t)entry->role);
        hash = kbo_asian_games_result_hash_add(hash, (uint32_t)entry->wildcard);
        hash = kbo_asian_games_result_hash_add(hash, (uint32_t)entry->score);
    }
    int odds = kbo_clamp_asian_games_no_gold_odds_denominator(no_gold_odds_denominator);
    return (hash % (uint32_t)odds) == 0u
        ? KBO_ASIAN_GAMES_RESULT_NO_GOLD
        : KBO_ASIAN_GAMES_RESULT_GOLD;
}

static uint8_t kbo_asian_games_resolve_final_result(uint32_t event_yyyymmdd, LONG roster_count, const char* source)
{
    if (g_kbo_asian_games_result == KBO_ASIAN_GAMES_RESULT_GOLD
            || g_kbo_asian_games_result == KBO_ASIAN_GAMES_RESULT_NO_GOLD) {
        return g_kbo_asian_games_result;
    }

    int returned = 0;
    int exempted = 0;
    for (LONG i = 0; i < roster_count && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        const KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u) {
            continue;
        }
        if (entry->returned != 0u) {
            returned++;
        }
        if (entry->exempted != 0u) {
            exempted++;
        }
    }

    int no_gold_odds_denominator = kbo_get_asian_games_no_gold_odds_denominator();
    if (returned > 0) {
        g_kbo_asian_games_result = exempted > 0
            ? KBO_ASIAN_GAMES_RESULT_GOLD
            : KBO_ASIAN_GAMES_RESULT_NO_GOLD;
    } else {
        g_kbo_asian_games_result = kbo_asian_games_roll_final_result(
            event_yyyymmdd,
            roster_count,
            no_gold_odds_denominator);
    }

    kbo_log_runtimef(
        "KBO Asian Games final result resolved source=%s date=%u result=%u no_gold_odds=1/%u returned=%d exempted=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        (uint32_t)g_kbo_asian_games_result,
        (uint32_t)no_gold_odds_denominator,
        returned,
        exempted);
    return g_kbo_asian_games_result;
}

int kbo_asian_games_finalize_selected_players(uint32_t event_yyyymmdd, const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_load_asian_games_roster_csv(source);
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_log_runtimef(
            "KBO Asian Games final skipped source=%s date=%u reason=no_roster count=%ld year=%u",
            source != NULL ? source : "",
            event_yyyymmdd,
            roster_count,
            g_kbo_asian_games_roster_year);
        return 0;
    }

    uint8_t final_result = kbo_asian_games_resolve_final_result(event_yyyymmdd, roster_count, source);
    int gold_won = final_result == KBO_ASIAN_GAMES_RESULT_GOLD;
    int returned = 0;
    int exempted = 0;
    int missing = 0;
    int no_team = 0;
    int history_recorded = 0;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->returned != 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            missing++;
            kbo_log_runtimef(
                "KBO Asian Games final missing player_id=%u index=%ld",
                entry->player_id,
                i + 1);
            continue;
        }

        uint32_t team_id = entry->original_team_id != 0u
            ? entry->original_team_id
            : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t league_id = entry->original_league_id != 0u
            ? entry->original_league_id
            : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
        if (team == NULL) {
            uint32_t restricted_team_id = 0u;
            uint32_t restricted_league_id = 0u;
            team = kbo_asian_games_find_restricted_team_for_player(
                entry->player_id,
                &restricted_team_id,
                &restricted_league_id);
            if (team != NULL) {
                team_id = restricted_team_id;
                if (restricted_league_id != 0u) {
                    league_id = restricted_league_id;
                }
                if (entry->original_team_id == 0u) {
                    entry->original_team_id = team_id;
                }
                if (entry->original_league_id == 0u) {
                    entry->original_league_id = league_id;
                }
            }
        }
        if (team == NULL) {
            no_team++;
            kbo_log_runtimef(
                "KBO Asian Games final no team player_id=%u team=%u league=%u index=%ld",
                entry->player_id,
                team_id,
                league_id,
                i + 1);
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint8_t* current_team = current_team_id != 0u && current_team_id != team_id
            ? find_kbo_team_by_numeric_id_any_league(current_team_id, 0)
            : NULL;
        uint8_t* active_team = active_team_id != 0u && active_team_id != team_id && active_team_id != current_team_id
            ? find_kbo_team_by_numeric_id_any_league(active_team_id, 0)
            : NULL;
        int removed_current = current_team != NULL
            ? kbo_remove_player_id_from_known_team_roster_arrays(current_team, entry->player_id)
            : 0;
        int removed_active = active_team != NULL
            ? kbo_remove_player_id_from_known_team_roster_arrays(active_team, entry->player_id)
            : 0;
        int removed_restricted = kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, entry->player_id);
        int added_assignment = kbo_add_player_id_to_team_assignment_arrays(team, entry->player_id);
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = team_id;
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = team_id;

        if (gold_won && entry->military_unserved != 0u) {
            complete_kbo_military_service_status(player);
            entry->exempted = 1u;
            exempted++;
        } else {
            entry->exempted = 0u;
        }

        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = entry->old_restricted;
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = entry->old_secondary_restricted;
        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] = entry->old_injury_active;
        kbo_set_military_days_left(player, entry->old_injury_days_left);

        entry->returned = 1u;
        returned++;
        history_recorded += kbo_record_asian_games_final_history(
            entry,
            event_yyyymmdd,
            final_result,
            source);
        kbo_log_runtimef(
            "KBO Asian Games finalized #%ld player_id=%u team=%u league=%u result=%u gold=%d exempted=%u removed_restricted=%d removed_current=%d removed_active=%d added_assignment=%d restored_restricted=%u restored_secondary=%u restored_injury=%u restored_days=%d",
            i + 1,
            entry->player_id,
            team_id,
            league_id,
            (uint32_t)final_result,
            gold_won,
            (uint32_t)player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET],
            removed_restricted,
            removed_current,
            removed_active,
            added_assignment,
            (uint32_t)entry->old_restricted,
            (uint32_t)entry->old_secondary_restricted,
            (uint32_t)entry->old_injury_active,
            (int)entry->old_injury_days_left);
    }

    kbo_log_runtimef(
        "KBO Asian Games final processed source=%s date=%u roster=%ld result=%u gold=%d returned=%d exempted=%d missing=%d no_team=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        roster_count,
        (uint32_t)final_result,
        gold_won,
        returned,
        exempted,
        missing,
        no_team);
    kbo_log_runtimef(
        "KBO Asian Games final player history source=%s date=%u returned=%d recorded=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        returned,
        history_recorded);
    kbo_save_asian_games_roster_csv(source);
    kbo_append_asian_games_tournament_history(
        event_yyyymmdd / 10000u,
        event_yyyymmdd,
        final_result,
        source);
    return returned;
}

int kbo_asian_games_roster_already_finalized(const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_load_asian_games_roster_csv(source);
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        return 0;
    }

    int finalized = 0;
    int repaired_teamless = 0;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id != 0u && entry->returned != 0u) {
            finalized++;
            if (entry->original_team_id != 0u) {
                uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
                uint8_t* team = find_kbo_team_by_numeric_id_any_league(entry->original_team_id, 0);
                if (player != NULL && team != NULL && kbo_player_pointer_plausible((uintptr_t)player)
                        && *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == 0u) {
                    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = entry->original_team_id;
                    if (entry->original_league_id != 0u) {
                        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = entry->original_league_id;
                    }
                    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = entry->original_team_id;
                    kbo_add_player_id_to_team_assignment_arrays(team, entry->player_id);
                    repaired_teamless++;
                }
            }
        }
    }
    if (repaired_teamless > 0) {
        kbo_log_runtimef(
            "KBO Asian Games finalized roster repaired teamless players source=%s repaired=%d roster=%ld",
            source != NULL ? source : "",
            repaired_teamless,
            roster_count);
    }
    return finalized > 0 && finalized == roster_count;
}
