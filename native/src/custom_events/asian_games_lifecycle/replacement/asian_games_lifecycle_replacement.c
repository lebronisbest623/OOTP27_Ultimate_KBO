#include "../../runtime/common/custom_events_common.h"
#include "asian_games_lifecycle_replacement.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../asian_games/history/asian_games_player_history.h"
#include "../../asian_games/player_eval/asian_games_player_eligibility.h"
#include "../roster/asian_games_lifecycle_roster.h"
#include "../../asian_games_news/emit/emit.h"

/* Asian Games unavailable-player replacement. */

int kbo_asian_games_find_replacement_for_entry(
    KboAsianGamesRosterEntry* old_entry,
    LONG old_index,
    KboAsianGamesRosterEntry* out_entry,
    uint32_t* out_org_id,
    const char* source)
{
    if (old_entry == NULL || out_entry == NULL) {
        return 0;
    }
    memset(out_entry, 0, sizeof(*out_entry));
    if (out_org_id != NULL) {
        *out_org_id = 0u;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        kbo_log_runtimef("KBO Asian Games replacement skipped source=%s old_player=%u reason=player_vector_unavailable", source != NULL ? source : "", old_entry->player_id);
        return 0;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    uint32_t allowed_leagues[KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES] = {0};
    int allowed_league_count = 0;
    if (kbo_league_id != 0u) {
        allowed_leagues[allowed_league_count++] = kbo_league_id;
    }
    uint32_t included_minor_league_id = 0u;
    if (kbo_asian_games_policy_minor_league_included(kbo_league_id, &included_minor_league_id)
            && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = included_minor_league_id;
    }

    int wildcard_count_without_old = kbo_asian_games_roster_wildcard_count_except(old_index);
    uint32_t old_org_id = kbo_asian_games_org_team_id_for_team(old_entry->original_team_id);
    int best_score = -2147483647;
    int best_same_bucket = 0;
    KboAsianGamesRosterEntry best_entry;
    memset(&best_entry, 0, sizeof(best_entry));
    uint32_t best_org_id = 0u;
    int scanned = 0;
    int rejected_service_team = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || player_id == old_entry->player_id || kbo_asian_games_roster_contains_player(player_id)) {
            continue;
        }
        if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (kbo_asian_games_player_unavailable_for_departure(player)) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (!kbo_asian_games_league_allowed(current_league_id, allowed_leagues, allowed_league_count)) {
            continue;
        }
        uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
        if (current_team == NULL
                || *(uint32_t*)(current_team + OOTP27_KBO_TEAM_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (kbo_asian_games_team_is_service_or_ulsan(current_team)) {
            rejected_service_team++;
            continue;
        }

        uint8_t role = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
        int same_bucket = kbo_asian_games_roles_same_bucket(role, old_entry->role);
        uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        const KboAsianGamesRosterPolicy* policy = kbo_asian_games_roster_policy();
        int wildcard = kbo_asian_games_policy_is_wildcard_age(age);
        if (wildcard && wildcard_count_without_old >= policy->max_wildcards) {
            continue;
        }
        uint32_t org_id = kbo_asian_games_org_team_id_for_team(current_team_id);
        if (!kbo_asian_games_replacement_allowed_for_org(org_id, old_org_id, old_index)) {
            continue;
        }

        int score = kbo_asian_games_player_score(player);
        if (best_entry.player_id == 0u
                || (same_bucket && !best_same_bucket)
                || (same_bucket == best_same_bucket && score > best_score)) {
            memset(&best_entry, 0, sizeof(best_entry));
            best_entry.player_id = player_id;
            best_entry.original_team_id = current_team_id;
            best_entry.original_league_id = current_league_id;
            best_entry.age = age;
            best_entry.role = role;
            best_entry.wildcard = wildcard ? 1u : 0u;
            best_entry.military_unserved = kbo_asian_games_player_military_unserved(player);
            best_entry.score = score;
            best_entry.player_ptr = player_ptr;
            best_score = score;
            best_same_bucket = same_bucket;
            best_org_id = org_id;
        }
    }

    if (best_entry.player_id == 0u) {
        kbo_log_runtimef(
            "KBO Asian Games replacement unavailable source=%s old_player=%u old_role=%u scanned=%d rejected_service_team=%d vector_offset=0x%x",
            source != NULL ? source : "",
            old_entry->player_id,
            (uint32_t)old_entry->role,
            scanned,
            rejected_service_team,
            vector_offset);
        return 0;
    }

    *out_entry = best_entry;
    if (out_org_id != NULL) {
        *out_org_id = best_org_id;
    }
    return 1;
}

int kbo_asian_games_replace_unavailable_players(uint32_t event_yyyymmdd, const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        return 0;
    }

    int replacements = 0;
    KboAsianGamesRosterEntry old_entries[KBO_ASIAN_GAMES_ROSTER_SIZE];
    KboAsianGamesRosterEntry new_entries[KBO_ASIAN_GAMES_ROSTER_SIZE];
    memset(old_entries, 0, sizeof(old_entries));
    memset(new_entries, 0, sizeof(new_entries));
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->departed != 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
        if (player != NULL && kbo_player_pointer_plausible((uintptr_t)player)) {
            entry->player_ptr = (uintptr_t)player;
        }
        if (!kbo_asian_games_player_unavailable_for_departure(player)) {
            continue;
        }

        KboAsianGamesRosterEntry old_entry = *entry;
        KboAsianGamesRosterEntry new_entry;
        uint32_t new_org_id = 0u;
        if (!kbo_asian_games_find_replacement_for_entry(entry, i, &new_entry, &new_org_id, source)) {
            kbo_log_runtimef(
                "KBO Asian Games replacement failed source=%s date=%u old_player=%u slot=%ld",
                source != NULL ? source : "",
                event_yyyymmdd,
                entry->player_id,
                i + 1);
            continue;
        }

        g_kbo_asian_games_roster[i] = new_entry;
        if (replacements < KBO_ASIAN_GAMES_ROSTER_SIZE) {
            old_entries[replacements] = old_entry;
            new_entries[replacements] = new_entry;
        }
        replacements++;
        kbo_log_runtimef(
            "KBO Asian Games injury replacement source=%s date=%u slot=%ld old_player=%u old_team=%u old_role=%u new_player=%u new_team=%u new_org=%u new_role=%u same_bucket=%d wildcard=%u score=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            i + 1,
            old_entry.player_id,
            old_entry.original_team_id,
            (uint32_t)old_entry.role,
            new_entry.player_id,
            new_entry.original_team_id,
            new_org_id,
            (uint32_t)new_entry.role,
            kbo_asian_games_roles_same_bucket(new_entry.role, old_entry.role),
            (uint32_t)new_entry.wildcard,
            new_entry.score);
    }

    if (replacements > 0) {
        int history_recorded = 0;
        for (int i = 0; i < replacements && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
            history_recorded += kbo_record_asian_games_replacement_history(
                &old_entries[i],
                &new_entries[i],
                event_yyyymmdd,
                source);
        }
        kbo_log_runtimef(
            "KBO Asian Games replacement player history source=%s date=%u replacements=%d recorded=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            replacements,
            history_recorded);
        kbo_save_asian_games_roster_csv(source);
        kbo_emit_asian_games_replacement_news_batch(
            event_yyyymmdd,
            old_entries,
            new_entries,
            replacements,
            source);
    }
    return replacements;
}
