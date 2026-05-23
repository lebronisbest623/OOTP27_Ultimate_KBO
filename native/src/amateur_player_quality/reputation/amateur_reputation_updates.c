#include "../internal/amateur_assignment_internal.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../runtime_memory/runtime_memory.h"
#include "sql/amateur_reputation_sql_store.h"

int kbo_find_amateur_team_reputation_by_memory_team(uint32_t league_id, uint8_t* team, uint8_t* out_reputation)
{
    if (out_reputation != NULL) {
        *out_reputation = kbo_amateur_default_team_reputation();
    }
    if (league_id == 0u || team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    for (int i = 0; i < g_kbo_amateur_resolved_team_reputation_count; i++) {
        if (g_kbo_amateur_resolved_team_reputations[i].team == team
                && g_kbo_amateur_resolved_team_reputations[i].league_id == league_id) {
            if (out_reputation != NULL) {
                *out_reputation = g_kbo_amateur_resolved_team_reputations[i].reputation;
            }
            return 1;
        }
    }
    uint32_t numeric_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (kbo_find_amateur_team_reputation_for_league(league_id, numeric_team_id, out_reputation)) {
        if (g_kbo_amateur_resolved_team_reputation_count < KBO_AMATEUR_ASSIGNMENT_TEAM_MAX) {
            KboAmateurResolvedTeamReputation* resolved = &g_kbo_amateur_resolved_team_reputations[g_kbo_amateur_resolved_team_reputation_count++];
            resolved->team = team;
            resolved->league_id = league_id;
            resolved->reputation = out_reputation != NULL ? *out_reputation : kbo_amateur_default_team_reputation();
        }
        return 1;
    }

    kbo_ensure_amateur_reputation_seeds_loaded();
    kbo_lock_amateur_reputation_seeds();
    for (int i = 0; i < g_kbo_amateur_reputation_seed_count; i++) {
        KboAmateurReputationSeed* seed = &g_kbo_amateur_reputation_seeds[i];
        if (seed->league_id != league_id) {
            continue;
        }
        if ((seed->team_abbr[0] != '\0' && team_has_ootp_string_text(team, seed->team_abbr))
                || (seed->team_name[0] != '\0' && team_has_ootp_string_text(team, seed->team_name))) {
            if (out_reputation != NULL) {
                *out_reputation = seed->reputation;
            }
            if (g_kbo_amateur_resolved_team_reputation_count < KBO_AMATEUR_ASSIGNMENT_TEAM_MAX) {
                KboAmateurResolvedTeamReputation* resolved = &g_kbo_amateur_resolved_team_reputations[g_kbo_amateur_resolved_team_reputation_count++];
                resolved->team = team;
                resolved->league_id = league_id;
                resolved->reputation = seed->reputation;
            }
            kbo_unlock_amateur_reputation_seeds();
            return 1;
        }
    }
    kbo_unlock_amateur_reputation_seeds();
    return 0;
}

uint32_t kbo_resolve_amateur_assignment_league_id_for_team_ptr(uint8_t* team)
{
    if (team == NULL) {
        return 0u;
    }
    uint8_t reputation = 0u;
    if (kbo_find_amateur_team_reputation_by_memory_team(KBO_HIGH_SCHOOL_LEAGUE_ID, team, &reputation)) {
        return KBO_HIGH_SCHOOL_LEAGUE_ID;
    }
    if (kbo_find_amateur_team_reputation_by_memory_team(KBO_COLLEGE_LEAGUE_ID, team, &reputation)) {
        return KBO_COLLEGE_LEAGUE_ID;
    }
    return 0u;
}

uint32_t kbo_resolve_amateur_assignment_league_id_for_team_and_player(uint8_t* team, uint8_t* player)
{
    uint32_t player_league_id = kbo_amateur_player_assignment_league_id(player);
    if (player_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID || player_league_id == KBO_COLLEGE_LEAGUE_ID) {
        uint8_t reputation = 0u;
        if (kbo_find_amateur_team_reputation_by_memory_team(player_league_id, team, &reputation)) {
            return player_league_id;
        }

        uint32_t team_league_id = kbo_resolve_amateur_assignment_league_id_for_team_ptr(team);
        if (team_league_id != 0u && team_league_id != player_league_id) {
            return 0u;
        }
        return 0u;
    }

    return kbo_resolve_amateur_assignment_league_id_for_team_ptr(team);
}

int kbo_compare_amateur_reputation_update_rows(const void* a, const void* b)
{
    const KboAmateurReputationUpdateRow* left = (const KboAmateurReputationUpdateRow*)a;
    const KboAmateurReputationUpdateRow* right = (const KboAmateurReputationUpdateRow*)b;
    if (left->score != right->score) {
        return right->score - left->score;
    }
    if (left->wins != right->wins) {
        return (int)right->wins - (int)left->wins;
    }
    return (int)left->team_id - (int)right->team_id;
}

int kbo_append_amateur_reputation_history(
    uint32_t league_id,
    const KboAmateurReputationUpdateRow* rows,
    int row_count,
    const char* source,
    uint32_t year)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_amateur_reputation_history_path(path, sizeof(path))) {
        kbo_log_runtimef("amateur reputation history skipped source=%s league=%u year=%u reason=no_save_scoped_path",
            source != NULL ? source : "", league_id, year);
        return 0;
    }

    if (!kbo_amateur_reputation_sql_history_append(league_id, rows, row_count, source, year)) {
        kbo_log_runtimef(
            "amateur reputation history write failed source=%s league=%u year=%u path=%s store=sqlite",
            source != NULL ? source : "",
            league_id,
            year,
            path);
        return 0;
    }
    kbo_log_runtimef("amateur reputation history appended source=%s league=%u year=%u rows=%d path=%s store=sqlite",
        source != NULL ? source : "", league_id, year, row_count, path);
    return 1;
}

int kbo_update_amateur_reputation_for_league(uint32_t league_id, const char* source, uint32_t year)
{
    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return 0;
    }
    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > KBO_RUNTIME_MAX_TEAM_VECTOR_COUNT
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    KboAmateurReputationUpdateRow rows[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
    int row_count = 0;
    for (int32_t i = 0; i < team_count && row_count < KBO_AMATEUR_ASSIGNMENT_TEAM_MAX; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint8_t old_rep = kbo_amateur_default_team_reputation();
        if (team_id == 0u || !kbo_find_amateur_team_reputation_by_memory_team(league_id, team, &old_rep)) {
            continue;
        }

        uint16_t wins = *(uint16_t*)(team + KBO_AMATEUR_TEAM_REGULAR_WINS_OFFSET);
        uint16_t losses = *(uint16_t*)(team + KBO_AMATEUR_TEAM_REGULAR_LOSSES_OFFSET);
        uint16_t ties = *(uint16_t*)(team + KBO_AMATEUR_TEAM_REGULAR_TIES_OFFSET);
        uint32_t games = (uint32_t)wins + (uint32_t)losses + (uint32_t)ties;
        if (games < (uint32_t)kbo_amateur_player_quality_policy()->reputation_update_min_games
                || wins > 250u || losses > 250u || ties > 250u) {
            continue;
        }

        int32_t pct_score = (int32_t)(((uint32_t)wins * 2000u + (uint32_t)ties * 1000u) / games);
        rows[row_count++] = (KboAmateurReputationUpdateRow){
            league_id,
            team_id,
            old_rep,
            old_rep,
            wins,
            losses,
            ties,
            pct_score
        };
    }

    if (row_count < 10) {
        kbo_log_runtimef("amateur reputation update skipped source=%s league=%u year=%u reason=too_few_rows rows=%d",
            source != NULL ? source : "", league_id, year, row_count);
        return 0;
    }

    qsort(rows, (size_t)row_count, sizeof(rows[0]), kbo_compare_amateur_reputation_update_rows);
    int32_t raw_delta_sum = 0;
    int32_t balance_adjustment = 0;
    int32_t final_delta_sum = 0;
    kbo_apply_amateur_reputation_balanced_deltas(
        league_id,
        rows,
        row_count,
        &raw_delta_sum,
        &balance_adjustment,
        &final_delta_sum);

    if (kbo_amateur_reputation_history_has_year(league_id, year)) {
        kbo_log_runtimef("amateur reputation update skipped source=%s league=%u year=%u reason=history_already_exists",
            source != NULL ? source : "", league_id, year);
        return row_count;
    }

    if (!kbo_append_amateur_reputation_history(league_id, rows, row_count, source, year)) {
        return 0;
    }

    kbo_lock_amateur_reputation_seeds();
    for (int i = 0; i < row_count; i++) {
        kbo_amateur_reputation_add_seed(league_id, rows[i].team_id, "", "", "", rows[i].new_reputation);
    }
    kbo_unlock_amateur_reputation_seeds();
    kbo_invalidate_amateur_assignment_candidate_cache();

    int top = row_count < 3 ? row_count : 3;
    for (int i = 0; i < top; i++) {
        kbo_log_runtimef("amateur reputation update top source=%s league=%u year=%u rank=%d team=%u record=%u-%u-%u rep=%u->%u score=%d",
            source != NULL ? source : "", league_id, year, i + 1, rows[i].team_id,
            (uint32_t)rows[i].wins, (uint32_t)rows[i].losses, (uint32_t)rows[i].ties,
            (uint32_t)rows[i].old_reputation, (uint32_t)rows[i].new_reputation, rows[i].score);
    }
    kbo_log_runtimef("amateur reputation update summary source=%s league=%u year=%u rows=%d raw_delta_sum=%d balance_adjust=%d final_delta_sum=%d playoff=todo regular_offsets=w:0x%x,l:0x%x,t:0x%x",
        source != NULL ? source : "", league_id, year, row_count,
        raw_delta_sum,
        balance_adjustment,
        final_delta_sum,
        KBO_AMATEUR_TEAM_REGULAR_WINS_OFFSET,
        KBO_AMATEUR_TEAM_REGULAR_LOSSES_OFFSET,
        KBO_AMATEUR_TEAM_REGULAR_TIES_OFFSET);
    return row_count;
}

void kbo_update_amateur_reputation_from_team_records_for_date(uint32_t today, const char* source)
{
    if (today == 0u || (today % 10000u) != 101u) {
        return;
    }
    uint32_t year = today / 10000u;

    if (g_kbo_amateur_reputation_last_update_high_school_year != year) {
        int updated = kbo_update_amateur_reputation_for_league(KBO_HIGH_SCHOOL_LEAGUE_ID, source, year);
        if (updated > 0) {
            g_kbo_amateur_reputation_last_update_high_school_year = year;
        }
    }
    if (g_kbo_amateur_reputation_last_update_college_year != year) {
        int updated = kbo_update_amateur_reputation_for_league(KBO_COLLEGE_LEAGUE_ID, source, year);
        if (updated > 0) {
            g_kbo_amateur_reputation_last_update_college_year = year;
        }
    }
}

void kbo_update_amateur_reputation_from_team_records(const char* source)
{
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today)) {
        return;
    }
    kbo_update_amateur_reputation_from_team_records_for_date(today, source);
}

uint32_t kbo_amateur_assignment_processed_hash_key(uint32_t player_id, uint32_t team_id)
{
    uint32_t hash = 2166136261u;
    hash = (hash ^ player_id) * 16777619u;
    hash = (hash ^ team_id) * 16777619u;
    hash ^= hash >> 16;
    return hash;
}

void kbo_amateur_assignment_clear_processed_cache(void)
{
    memset(g_kbo_amateur_assignment_processed, 0, sizeof(g_kbo_amateur_assignment_processed));
    memset(g_kbo_amateur_assignment_processed_hash, 0, sizeof(g_kbo_amateur_assignment_processed_hash));
    InterlockedExchange(&g_kbo_amateur_assignment_processed_count, 0);
    InterlockedExchange(&g_kbo_amateur_assignment_processed_hash_count, 0);
}

