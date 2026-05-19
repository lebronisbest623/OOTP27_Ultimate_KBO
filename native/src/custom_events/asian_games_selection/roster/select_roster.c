#include "../../runtime/common/custom_events_common.h"
#include "select_roster.h"
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../asian_games/roster/asian_games_roster_store.h"
#include "select_roster_ortools.h"

int kbo_select_asian_games_roster(uint32_t event_yyyymmdd, const char* source)
{
    uint32_t year = event_yyyymmdd / 10000u;
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        kbo_log_runtimef(
            "KBO Asian Games roster selection skipped source=%s year=%u reason=player_vector_unavailable",
            source != NULL ? source : "",
            year);
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = kbo_resolve_kbo_league_id();
    }
    uint32_t allowed_leagues[KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES] = {0};
    int allowed_league_count = 0;
    if (kbo_league_id != 0u && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = kbo_league_id;
    }
    uint32_t included_minor_league_id = 0u;
    if (kbo_asian_games_policy_minor_league_included(kbo_league_id, &included_minor_league_id)
            && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = included_minor_league_id;
    }

    KboAsianGamesCandidate* candidates = (KboAsianGamesCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(KboAsianGamesCandidate) * KBO_ASIAN_GAMES_MAX_CANDIDATES);
    if (candidates == NULL) {
        kbo_log_runtimef(
            "KBO Asian Games roster selection skipped source=%s year=%u reason=alloc_failed",
            source != NULL ? source : "",
            year);
        return 0;
    }

    int candidate_count = 0;
    int scanned = 0;
    int rejected_nation = 0;
    int rejected_status = 0;
    int rejected_team = 0;
    int rejected_league = 0;
    int rejected_service_team = 0;
    int rejected_parentless_affiliate = 0;
    uint32_t main_league_id = allowed_league_count > 0 ? allowed_leagues[0] : 0u;
    for (int32_t i = 0; i < player_count && candidate_count < KBO_ASIAN_GAMES_MAX_CANDIDATES; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
            rejected_nation++;
            continue;
        }
        if (player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] != 0u
                || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u
                || player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] != 0u
                || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u) {
            rejected_status++;
            continue;
        }
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (!kbo_asian_games_league_allowed(current_league_id, allowed_leagues, allowed_league_count)) {
            rejected_league++;
            continue;
        }
        uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
        uint32_t team_nation_id = current_team != NULL
            ? *(uint32_t*)(current_team + OOTP27_KBO_TEAM_NATION_ID_OFFSET)
            : 0u;
        if (current_team_id == 0u || current_league_id == 0u
                || current_team == NULL
                || team_nation_id != OOTP27_KBO_KOREA_NATION_ID) {
            rejected_team++;
            continue;
        }
        if (kbo_asian_games_team_is_service_or_ulsan(current_team)) {
            rejected_service_team++;
            continue;
        }
        if (!kbo_asian_games_team_has_parent_if_affiliate(current_team, main_league_id)) {
            rejected_parentless_affiliate++;
            continue;
        }

        KboAsianGamesCandidate* candidate = &candidates[candidate_count++];
        candidate->entry.player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        candidate->entry.original_team_id = current_team_id;
        candidate->entry.original_league_id = current_league_id;
        candidate->entry.departure_date = 0u;
        candidate->entry.return_date = 0u;
        candidate->entry.age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        candidate->entry.role = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
        candidate->entry.wildcard = 0u;
        candidate->entry.military_unserved = 1u;
        candidate->entry.old_restricted = 0u;
        candidate->entry.old_secondary_restricted = 0u;
        candidate->entry.old_injury_active = 0u;
        candidate->entry.old_injury_days_left = 0;
        candidate->entry.departed = 0u;
        candidate->entry.returned = 0u;
        candidate->entry.exempted = 0u;
        candidate->entry.score = kbo_asian_games_player_score(player);
        candidate->entry.player_ptr = player_ptr;
        candidate->org_team_id = kbo_asian_games_org_team_id_for_team(current_team_id);
    }

    qsort(candidates, (size_t)candidate_count, sizeof(KboAsianGamesCandidate), kbo_asian_games_candidate_compare_desc);
    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    g_kbo_asian_games_roster_count = 0;
    g_kbo_asian_games_roster_year = year;
    g_kbo_asian_games_result = KBO_ASIAN_GAMES_RESULT_UNKNOWN;

    uint32_t required_orgs[KBO_ASIAN_GAMES_MAX_ORGS] = {0};
    int required_org_count = kbo_asian_games_collect_required_orgs(
        allowed_league_count > 0 ? allowed_leagues[0] : 0u,
        required_orgs,
        KBO_ASIAN_GAMES_MAX_ORGS);

    int selected_count = 0;
    int pitcher_count = 0;
    int catcher_count = 0;
    int infielder_count = 0;
    int outfielder_count = 0;
    int wildcard_count = 0;

    if (kbo_select_asian_games_roster_ortools(
            candidates,
            candidate_count,
            required_orgs,
            required_org_count,
            &selected_count,
            &pitcher_count,
            &catcher_count,
            &infielder_count,
            &outfielder_count,
            &wildcard_count,
            source) <= 0) {
        kbo_log_runtimef(
            "KBO Asian Games roster selection skipped source=%s year=%u reason=optimizer_unavailable scanned=%d candidates=%d required_orgs=%d allowed_leagues=%u/%u vector_offset=0x%x rejected_nation=%d rejected_status=%d rejected_league=%d rejected_team=%d rejected_service_team=%d rejected_parentless_affiliate=%d",
            source != NULL ? source : "",
            year,
            scanned,
            candidate_count,
            required_org_count,
            allowed_league_count > 0 ? allowed_leagues[0] : 0u,
            allowed_league_count > 1 ? allowed_leagues[1] : 0u,
            vector_offset,
            rejected_nation,
            rejected_status,
            rejected_league,
            rejected_team,
            rejected_service_team,
            rejected_parentless_affiliate);
        HeapFree(GetProcessHeap(), 0, candidates);
        return 0;
    }

    int required_org_missing = 0;
    for (int org_index = 0; org_index < required_org_count; org_index++) {
        if (kbo_asian_games_roster_org_count(required_orgs[org_index], selected_count) <= 0) {
            required_org_missing++;
        }
    }

    g_kbo_asian_games_roster_count = selected_count;
    kbo_log_runtimef(
        "KBO Asian Games roster selected source=%s year=%u method=optimizer scanned=%d candidates=%d selected=%d pitchers=%d catchers=%d infielders=%d outfielders=%d wildcards=%d required_orgs=%d required_missing=%d allowed_leagues=%u/%u vector_offset=0x%x rejected_nation=%d rejected_status=%d rejected_league=%d rejected_team=%d rejected_service_team=%d rejected_parentless_affiliate=%d",
        source != NULL ? source : "",
        year,
        scanned,
        candidate_count,
        selected_count,
        pitcher_count,
        catcher_count,
        infielder_count,
        outfielder_count,
        wildcard_count,
        required_org_count,
        required_org_missing,
        allowed_league_count > 0 ? allowed_leagues[0] : 0u,
        allowed_league_count > 1 ? allowed_leagues[1] : 0u,
        vector_offset,
        rejected_nation,
        rejected_status,
        rejected_league,
        rejected_team,
        rejected_service_team,
        rejected_parentless_affiliate);
    for (int i = 0; i < selected_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        kbo_log_runtimef(
            "KBO Asian Games selected #%02d player_id=%u team=%u org=%u league=%u age=%u role=%u bucket=%s wildcard=%u score=%d",
            i + 1,
            entry->player_id,
            entry->original_team_id,
            kbo_asian_games_org_team_id_for_team(entry->original_team_id),
            entry->original_league_id,
            (uint32_t)entry->age,
            (uint32_t)entry->role,
            kbo_asian_games_role_bucket_label(entry->role),
            (uint32_t)entry->wildcard,
            entry->score);
    }

    HeapFree(GetProcessHeap(), 0, candidates);
    kbo_save_asian_games_roster_csv(source);
    return selected_count;
}
