

#include "../../runtime/common/custom_events_common.h"
#include "asian_games_player_eval.h"

#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_string.h"
#include "../../asian_games_schedule_seed/query/query_helpers.h"

int kbo_asian_games_candidate_compare_desc(const void* left, const void* right)
{
    const KboAsianGamesCandidate* a = (const KboAsianGamesCandidate*)left;
    const KboAsianGamesCandidate* b = (const KboAsianGamesCandidate*)right;
    if (a->entry.score != b->entry.score) {
        return a->entry.score > b->entry.score ? -1 : 1;
    }
    if (a->entry.age != b->entry.age) {
        return a->entry.age < b->entry.age ? -1 : 1;
    }
    if (a->entry.player_id == b->entry.player_id) {
        return 0;
    }
    return a->entry.player_id < b->entry.player_id ? -1 : 1;
}

int32_t kbo_asian_games_player_score(uint8_t* player)
{
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);

    const KboAsianGamesRosterPolicy* policy = kbo_asian_games_roster_policy();
    int32_t score = talent * policy->score_talent_weight
        + overall * policy->score_overall_weight
        + ratings * policy->score_ratings_weight
        + career * policy->score_career_weight;
    if (age <= (uint16_t)policy->score_young_age_max) {
        score += policy->score_young_bonus;
    } else if (age <= (uint16_t)policy->score_prime_age_max) {
        score += policy->score_prime_bonus;
    } else {
        uint16_t decline_after = (uint16_t)policy->score_age_decline_after;
        if (age > decline_after) {
            score -= (int32_t)(age - decline_after) * policy->score_age_decline_penalty_per_year;
        }
    }
    if (player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] == 0u) {
        score += policy->score_non_exempt_bonus;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != 0u) {
        score += policy->score_current_team_bonus;
    }
    return score;
}

int kbo_asian_games_role_is_pitcher(uint8_t role)
{
    return role == 1u;
}

int kbo_asian_games_role_is_catcher(uint8_t role)
{
    return role == 2u;
}

int kbo_asian_games_role_is_infielder(uint8_t role)
{
    return role >= 3u && role <= 6u;
}

int kbo_asian_games_role_is_outfielder(uint8_t role)
{
    return role >= 7u && role <= 9u;
}

const char* kbo_asian_games_role_bucket_label(uint8_t role)
{
    if (kbo_asian_games_role_is_pitcher(role)) {
        return "P";
    }
    if (kbo_asian_games_role_is_catcher(role)) {
        return "C";
    }
    if (kbo_asian_games_role_is_infielder(role)) {
        return "IF";
    }
    if (kbo_asian_games_role_is_outfielder(role)) {
        return "OF";
    }
    return "BAT";
}

int kbo_asian_games_roles_same_bucket(uint8_t left, uint8_t right)
{
    if (kbo_asian_games_role_is_pitcher(left)) {
        return kbo_asian_games_role_is_pitcher(right);
    }
    if (kbo_asian_games_role_is_catcher(left)) {
        return kbo_asian_games_role_is_catcher(right);
    }
    if (kbo_asian_games_role_is_infielder(left)) {
        return kbo_asian_games_role_is_infielder(right);
    }
    if (kbo_asian_games_role_is_outfielder(left)) {
        return kbo_asian_games_role_is_outfielder(right);
    }
    return 0;
}

int kbo_asian_games_league_allowed(uint32_t league_id, const uint32_t* allowed_leagues, int allowed_count)
{
    if (league_id == 0u || allowed_leagues == NULL || allowed_count <= 0) {
        return 0;
    }
    for (int i = 0; i < allowed_count; i++) {
        if (allowed_leagues[i] == league_id) {
            return 1;
        }
    }
    return 0;
}

int kbo_ascii_contains_ignore_case(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    size_t needle_len = strlen(needle);
    for (const char* p = text; *p != '\0'; p++) {
        size_t i = 0;
        while (i < needle_len && p[i] != '\0') {
            char a = p[i];
            char b = needle[i];
            if (a >= 'A' && a <= 'Z') { a = (char)(a + 32); }
            if (b >= 'A' && b <= 'Z') { b = (char)(b + 32); }
            if (a != b) {
                break;
            }
            i++;
        }
        if (i == needle_len) {
            return 1;
        }
    }
    return 0;
}

int kbo_asian_games_team_is_service_or_ulsan(uint8_t* team)
{
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    static const uint32_t string_offsets[] = OOTP27_KBO_TEAM_SEARCH_STRING_OFFSETS;
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        char name[96] = {0};
        if (!copy_ootp_string_object_text(team, string_offsets[i], name, sizeof(name))) {
            continue;
        }
        const KboAsianGamesRosterPolicy* policy = kbo_asian_games_roster_policy();
        if (kbo_ascii_contains_ignore_case(name, policy->service_team_keyword_1)
                || kbo_ascii_contains_ignore_case(name, policy->service_team_keyword_2)
                || kbo_ascii_contains_ignore_case(name, policy->service_team_keyword_3)
                || strstr(name, "\xec\x83\x81\xeb\xac\xb4") != NULL
                || strstr(name, "\xea\xb2\xbd\xec\xb0\xb0") != NULL
                || strstr(name, "\xec\x9a\xb8\xec\x82\xb0") != NULL) {
            return 1;
        }
    }
    return 0;
}

uint32_t kbo_asian_games_org_team_id_for_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u) {
            return parent_team_id;
        }
    }
    return team_id;
}

int kbo_asian_games_team_has_parent_if_affiliate(uint8_t* team, uint32_t main_league_id)
{
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (main_league_id == 0u || team_league_id == 0u || team_league_id == main_league_id) {
        return 1;
    }
    if (!memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }
    uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    if (parent_team_id == 0u) {
        return 0;
    }
    uint8_t* parent_team = find_kbo_team_by_numeric_id_any_league(parent_team_id, 0);
    return parent_team != NULL
        && memory_range_readable(parent_team, OOTP27_KBO_TEAM_READABLE_BYTES)
        && parent_team[OOTP27_KBO_TEAM_DELETED_OFFSET] == 0u
        && *(uint32_t*)(parent_team + OOTP27_KBO_TEAM_NATION_ID_OFFSET) == OOTP27_KBO_KOREA_NATION_ID;
}

int kbo_asian_games_find_org_index(const uint32_t* org_ids, int org_count, uint32_t org_id)
{
    if (org_ids == NULL || org_id == 0u || org_count <= 0) {
        return -1;
    }
    for (int i = 0; i < org_count; i++) {
        if (org_ids[i] == org_id) {
            return i;
        }
    }
    return -1;
}

int kbo_asian_games_roster_org_count(uint32_t org_id, int selected_count)
{
    if (org_id == 0u || selected_count <= 0) {
        return 0;
    }

    int count = 0;
    if (selected_count > kbo_asian_games_policy_roster_size()) {
        selected_count = kbo_asian_games_policy_roster_size();
    }
    for (int i = 0; i < selected_count; i++) {
        uint32_t roster_org_id = kbo_asian_games_org_team_id_for_team(g_kbo_asian_games_roster[i].original_team_id);
        if (roster_org_id == org_id) {
            count++;
        }
    }
    return count;
}

int kbo_asian_games_collect_required_orgs(uint32_t main_league_id, uint32_t* org_ids, int org_capacity)
{
    uintptr_t global = get_ootp_global_database();
    if (main_league_id == 0u || org_ids == NULL || org_capacity <= 0
            || global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return 0;
    }

    uintptr_t vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    int org_count = 0;
    for (int32_t i = 0; i < team_count && org_count < org_capacity; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) != main_league_id
                || *(uint32_t*)(team + OOTP27_KBO_TEAM_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID
                || team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0u) {
            continue;
        }
        if (kbo_asian_games_team_is_service_or_ulsan(team)) {
            continue;
        }
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint32_t org_id = kbo_asian_games_org_team_id_for_team(team_id);
        if (org_id != 0u && kbo_asian_games_find_org_index(org_ids, org_count, org_id) < 0) {
            org_ids[org_count++] = org_id;
        }
    }
    return org_count;
}

uint32_t kbo_asian_games_final_date_for_year(uint32_t year)
{
    KboAsianGamesScheduleSeed schedule;
    if (kbo_get_asian_games_schedule_for_year(year, &schedule)) {
        if (schedule.final_date != 0u) {
            return schedule.final_date;
        }
        if (schedule.tournament_end != 0u) {
            return schedule.tournament_end;
        }
    }
    return 0u;
}

int32_t kbo_asian_games_days_until_return(uint32_t event_yyyymmdd)
{
    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    uint32_t final_date = kbo_asian_games_final_date_for_year(year);
    uint32_t start_serial = kbo_date_serial(year, month, day);
    uint32_t final_serial = kbo_date_serial(final_date / 10000u, (final_date / 100u) % 100u, final_date % 100u);
    if (start_serial == 0u || final_serial == 0u || final_serial < start_serial) {
        return (int32_t)kbo_asian_games_roster_policy()->fallback_return_days;
    }
    return (int32_t)(final_serial - start_serial + 1u);
}

