#include "../../runtime/common/custom_events_common.h"
#include "asian_games_lifecycle_departure.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../core/sql/history_transactions/core_sql_history_transactions.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../asian_games_news/links/links.h"

/* Asian Games departure state mutation and transaction history. */

void kbo_record_asian_games_restricted_reason(
    KboAsianGamesRosterEntry* entry,
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    uint32_t team_id,
    const char* source)
{
    if (entry == NULL || entry->player_id == 0u || event_yyyymmdd == 0u || league_id == 0u || team_id == 0u) {
        return;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;

    char player_link[128] = {0};
    char team_link[128] = {0};
    kbo_copy_asian_games_player_link(entry, player_link, sizeof(player_link));
    kbo_copy_asian_games_team_link(team_id, team_link, sizeof(team_link));

    char league_text[512] = {0};
    char team_text[512] = {0};
    char history_text[512] = {0};
    snprintf(
        league_text,
        sizeof(league_text),
        "%s: Placed %s on the restricted list for Asian Games national team duty.",
        team_link[0] != '\0' ? team_link : "KBO club",
        player_link[0] != '\0' ? player_link : "a player");
    snprintf(
        team_text,
        sizeof(team_text),
        "Placed %s on the restricted list for Asian Games national team duty.",
        player_link[0] != '\0' ? player_link : "a player");
    snprintf(
        history_text,
        sizeof(history_text),
        "Placed on the restricted list for Asian Games national team duty.");

    insert_kbo_roster_transaction_sql(
        league_id,
        team_id,
        year,
        month,
        day,
        0u,
        league_text,
        team_text,
        source);
    insert_kbo_player_history_sql(
        entry->player_id,
        year,
        month,
        day,
        history_text,
        source);
}

int kbo_asian_games_depart_selected_players(uint32_t event_yyyymmdd, const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_load_asian_games_roster_csv(source);
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_log_runtimef(
            "KBO Asian Games departure skipped source=%s date=%u reason=no_roster count=%ld year=%u",
            source != NULL ? source : "",
            event_yyyymmdd,
            roster_count,
            g_kbo_asian_games_roster_year);
        return 0;
    }

    int replacements = kbo_asian_games_replace_unavailable_players(event_yyyymmdd, source);
    if (replacements > 0) {
        roster_count = g_kbo_asian_games_roster_count;
    }

    int departed = 0;
    int missing = 0;
    int no_team = 0;
    int32_t days_left = kbo_asian_games_days_until_return(event_yyyymmdd);
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->departed != 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            missing++;
            kbo_log_runtimef(
                "KBO Asian Games departure missing player_id=%u index=%ld",
                entry->player_id,
                i + 1);
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
        if (team == NULL) {
            no_team++;
            kbo_log_runtimef(
                "KBO Asian Games departure no team player_id=%u team=%u league=%u index=%ld",
                entry->player_id,
                current_team_id,
                current_league_id,
                i + 1);
            continue;
        }

        entry->player_ptr = (uintptr_t)player;
        if (entry->original_team_id == 0u) {
            entry->original_team_id = current_team_id;
        }
        if (entry->original_league_id == 0u) {
            entry->original_league_id = current_league_id;
        }
        entry->old_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        entry->old_secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        entry->old_injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        entry->old_injury_days_left = (int16_t)kbo_military_days_left(player);
        entry->departure_date = event_yyyymmdd;
        entry->return_date = kbo_asian_games_final_date_for_year(event_yyyymmdd / 10000u);

        int removed = kbo_remove_player_id_from_known_team_roster_arrays(team, entry->player_id);
        int added_restricted = kbo_add_player_id_to_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, entry->player_id);
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] = 1u;
        kbo_set_military_days_left(player, days_left);
        kbo_record_asian_games_restricted_reason(
            entry,
            event_yyyymmdd,
            current_league_id,
            current_team_id,
            source);
        entry->departed = 1u;
        departed++;

        kbo_log_runtimef(
            "KBO Asian Games departed #%ld player_id=%u team=%u league=%u role=%u bucket=%s days_left=%d removed=%d added_restricted=%d active_cleared=1 old_restricted=%u old_secondary=%u old_injury=%u old_days=%d",
            i + 1,
            entry->player_id,
            current_team_id,
            current_league_id,
            (uint32_t)entry->role,
            kbo_asian_games_role_bucket_label(entry->role),
            days_left,
            removed,
            added_restricted,
            (uint32_t)entry->old_restricted,
            (uint32_t)entry->old_secondary_restricted,
            (uint32_t)entry->old_injury_active,
            (int)entry->old_injury_days_left);
    }

    kbo_log_runtimef(
        "KBO Asian Games departure processed source=%s date=%u roster=%ld departed=%d missing=%d no_team=%d days_left=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        roster_count,
        departed,
        missing,
        no_team,
        days_left);
    kbo_save_asian_games_roster_csv(source);
    return departed;
}
