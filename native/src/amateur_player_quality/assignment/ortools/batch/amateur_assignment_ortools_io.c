#include "..\amateur_assignment_ortools.h"
#include "../../../../build_verify/build_verify.h"

uint32_t kbo_amateur_batch_resolve_source_team_id(uint8_t* player, uintptr_t source_team_ptr)
{
    uint32_t current_team_id = 0u;
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        current_team_id = kbo_amateur_player_assignment_team_id(player);
    }
    if (source_team_ptr != 0
            && memory_range_readable((void*)source_team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        uint32_t source_team_id = *(uint32_t*)((uint8_t*)source_team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        if (current_team_id == 0u) {
            current_team_id = source_team_id;
        }
    }
    return current_team_id;
}

int kbo_amateur_batch_source_index(uint32_t* team_ids, int count, uint32_t team_id)
{
    if (team_id == 0u) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (team_ids[i] == team_id) {
            return i;
        }
    }
    return -1;
}

uintptr_t kbo_amateur_candidate_team_ptr_by_id(
    KboAmateurAssignmentCandidate* candidates,
    int count,
    uint32_t team_id)
{
    if (candidates == NULL || count <= 0 || team_id == 0u) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == team_id) {
            return (uintptr_t)candidates[i].team;
        }
    }
    return 0;
}

int kbo_amateur_ortools_write_batch_request(
    const char* path,
    uintptr_t* players,
    uintptr_t* source_teams,
    int32_t player_count,
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int count,
    int incoming_batch)
{
    char tmp_path[MAX_PATH] = {0};
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE* file = fopen(tmp_path, "wb");
    if (file == NULL) {
        return 0;
    }

    int32_t target_max_players = kbo_amateur_assignment_target_max_players(league_id);
    uint32_t source_team_ids[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX];
    int32_t source_player_counts[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX];
    int32_t source_hitter_counts[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX];
    int32_t source_position_counts[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX][KBO_AMATEUR_POSITION_BUCKET_COUNT];
    int source_team_count = 0;
    memset(source_team_ids, 0, sizeof(source_team_ids));
    memset(source_player_counts, 0, sizeof(source_player_counts));
    memset(source_hitter_counts, 0, sizeof(source_hitter_counts));
    memset(source_position_counts, 0, sizeof(source_position_counts));

    for (int32_t p = 0; p < player_count; p++) {
        uint8_t* player = (uint8_t*)players[p];
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        if (player_id == 0u || !kbo_amateur_player_age_eligible(league_id, age)) {
            continue;
        }
        uint32_t source_team_id = kbo_amateur_batch_resolve_source_team_id(
            player,
            source_teams != NULL ? source_teams[p] : 0);
        if (source_team_id == 0u) {
            continue;
        }
        int source_index = kbo_amateur_batch_source_index(source_team_ids, source_team_count, source_team_id);
        if (source_index < 0) {
            if (source_team_count >= KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX) {
                continue;
            }
            source_index = source_team_count++;
            source_team_ids[source_index] = source_team_id;
        }
        source_player_counts[source_index]++;
        if (kbo_amateur_player_is_hitter(player)) {
            source_hitter_counts[source_index]++;
        }
        int position_bucket = kbo_amateur_player_position_bucket(player);
        if (position_bucket >= 0 && position_bucket < KBO_AMATEUR_POSITION_BUCKET_COUNT) {
            source_position_counts[source_index][position_bucket]++;
        }
    }

    fprintf(
        file,
        "player_id,league_id,age,quality_score,player_tier,target_reputation,current_team_id,current_reputation,is_hitter,role_bucket,position_group,position_role,source_batch_count,source_hitter_batch_count,source_pitcher_batch_count,source_catcher_batch_count,source_infielder_batch_count,source_outfielder_batch_count,source_first_base_batch_count,source_second_base_batch_count,source_third_base_batch_count,source_shortstop_batch_count,source_left_field_batch_count,source_center_field_batch_count,source_right_field_batch_count,source_designated_hitter_batch_count,target_max_players,team_id,reputation,team_tier,player_count,hitter_count,pitcher_count,catcher_count,infielder_count,outfielder_count,first_base_count,second_base_count,third_base_count,shortstop_count,left_field_count,center_field_count,right_field_count,designated_hitter_count,rejected,batch_mode,draft_penalty_stages\r\n");

    int written_players = 0;
    for (int32_t p = 0; p < player_count; p++) {
        uint8_t* player = (uint8_t*)players[p];
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        int32_t quality_score = kbo_amateur_quality_score(player);
        if (player_id == 0u || quality_score <= 0 || !kbo_amateur_player_age_eligible(league_id, age)) {
            continue;
        }

        uint32_t current_team_id = kbo_amateur_batch_resolve_source_team_id(
            player,
            source_teams != NULL ? source_teams[p] : 0);

        uint8_t current_reputation = 0u;
        for (int i = 0; i < count; i++) {
            if (candidates[i].team_id == current_team_id) {
                current_reputation = candidates[i].reputation;
                break;
            }
        }
        if (current_team_id == 0u || current_reputation == 0u) {
            continue;
        }

        int32_t target = kbo_amateur_assignment_target_reputation(league_id, quality_score);
        int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);

        int source_index = kbo_amateur_batch_source_index(source_team_ids, source_team_count, current_team_id);
        int32_t source_batch_count = source_index >= 0 ? source_player_counts[source_index] : 0;
        int32_t source_hitter_batch_count = source_index >= 0 ? source_hitter_counts[source_index] : 0;
        int32_t source_pitcher_batch_count = source_index >= 0 ? source_position_counts[source_index][0] : 0;
        int32_t source_catcher_batch_count = source_index >= 0 ? source_position_counts[source_index][1] : 0;
        int32_t source_first_base_batch_count = source_index >= 0 ? source_position_counts[source_index][2] : 0;
        int32_t source_second_base_batch_count = source_index >= 0 ? source_position_counts[source_index][3] : 0;
        int32_t source_third_base_batch_count = source_index >= 0 ? source_position_counts[source_index][4] : 0;
        int32_t source_shortstop_batch_count = source_index >= 0 ? source_position_counts[source_index][5] : 0;
        int32_t source_left_field_batch_count = source_index >= 0 ? source_position_counts[source_index][6] : 0;
        int32_t source_center_field_batch_count = source_index >= 0 ? source_position_counts[source_index][7] : 0;
        int32_t source_right_field_batch_count = source_index >= 0 ? source_position_counts[source_index][8] : 0;
        int32_t source_designated_hitter_batch_count = source_index >= 0 ? source_position_counts[source_index][9] : 0;
        int32_t source_infielder_batch_count = source_first_base_batch_count
            + source_second_base_batch_count
            + source_third_base_batch_count
            + source_shortstop_batch_count
            + source_designated_hitter_batch_count;
        int32_t source_outfielder_batch_count = source_left_field_batch_count
            + source_center_field_batch_count
            + source_right_field_batch_count;
        int is_hitter = kbo_amateur_player_is_hitter(player);
        uint8_t position_group = memory_range_readable(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET, sizeof(uint8_t))
            ? player[OOTP27_PLAYER_POSITION_GROUP_OFFSET]
            : 0u;
        uint8_t position_role = memory_range_readable(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET, sizeof(uint8_t))
            ? player[OOTP27_PLAYER_POSITION_ROLE_OFFSET]
            : 0u;
        const char* role_bucket = kbo_amateur_position_bucket_label(kbo_amateur_player_position_bucket(player));
        int wrote_player = 0;
        for (int i = 0; i < count; i++) {
            int team_tier = kbo_amateur_assignment_team_tier(league_id, candidates[i].reputation);
            int rejected = kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id);
            if (rejected) {
                continue;
            }
            int draft_penalty = kbo_cbt_draft_penalty_stages_for_team(candidates[i].team_id);
            if (!wrote_player) {
                written_players++;
                wrote_player = 1;
            }
            fprintf(
                file,
                "%u,%u,%d,%d,%d,%d,%u,%u,%d,%s,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d\r\n",
                player_id,
                league_id,
                (int)age,
                quality_score,
                player_tier,
                target,
                current_team_id,
                (uint32_t)current_reputation,
                is_hitter,
                role_bucket,
                (uint32_t)position_group,
                (uint32_t)position_role,
                source_batch_count,
                source_hitter_batch_count,
                source_pitcher_batch_count,
                source_catcher_batch_count,
                source_infielder_batch_count,
                source_outfielder_batch_count,
                source_first_base_batch_count,
                source_second_base_batch_count,
                source_third_base_batch_count,
                source_shortstop_batch_count,
                source_left_field_batch_count,
                source_center_field_batch_count,
                source_right_field_batch_count,
                source_designated_hitter_batch_count,
                target_max_players,
                candidates[i].team_id,
                (uint32_t)candidates[i].reputation,
                team_tier,
                candidates[i].player_count,
                candidates[i].hitter_count,
                candidates[i].pitcher_count,
                candidates[i].catcher_count,
                candidates[i].infielder_count,
                candidates[i].outfielder_count,
                candidates[i].first_base_count,
                candidates[i].second_base_count,
                candidates[i].third_base_count,
                candidates[i].shortstop_count,
                candidates[i].left_field_count,
                candidates[i].center_field_count,
                candidates[i].right_field_count,
                candidates[i].designated_hitter_count,
                rejected,
                incoming_batch ? "incoming" : "roster",
                draft_penalty);
        }
    }

    fclose(file);
    if (!MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp_path);
        return 0;
    }
    return written_players > 0;
}

int kbo_amateur_league_batch_has_team(uint32_t team_id)
{
    for (int32_t i = 0; i < g_kbo_amateur_league_batch_team_count; i++) {
        if (g_kbo_amateur_league_batch_team_ids[i] == team_id) {
            return 1;
        }
    }
    return 0;
}

int32_t kbo_amateur_league_batch_find_player_index(uint32_t player_id)
{
    for (int32_t i = 0; i < g_kbo_amateur_league_batch_player_count; i++) {
        uint8_t* player = (uint8_t*)g_kbo_amateur_league_batch_players[i];
        if (player != NULL
                && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                && *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return i;
        }
    }
    return -1;
}

int kbo_amateur_league_batch_has_player(uint32_t player_id)
{
    return kbo_amateur_league_batch_find_player_index(player_id) >= 0;
}

int kbo_amateur_local_player_list_has_id(uintptr_t* players, int32_t count, uint32_t player_id)
{
    for (int32_t i = 0; i < count; i++) {
        uint8_t* player = (uint8_t*)players[i];
        if (player != NULL
                && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                && *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return 1;
        }
    }
    return 0;
}

int kbo_amateur_generation_team_add_caller(uint32_t caller_rva)
{
    return kbo_current_build_caller_rva_matches(caller_rva, OOTP27_AMATEUR_GENERATION_TEAM_ADD_CALLER_00A30BA0_RVA)
        || kbo_current_build_caller_rva_matches(caller_rva, OOTP27_AMATEUR_GENERATION_TEAM_ADD_CALLER_00A3105B_RVA)
        || kbo_current_build_caller_rva_matches(caller_rva, OOTP27_AMATEUR_GENERATION_TEAM_ADD_CALLER_00A312A7_RVA)
        || kbo_current_build_caller_rva_matches(caller_rva, OOTP27_AMATEUR_GENERATION_TEAM_ADD_CALLER_00A3150F_RVA)
        || kbo_current_build_caller_rva_matches(caller_rva, OOTP27_AMATEUR_GENERATION_TEAM_ADD_CALLER_00A3175B_RVA)
        || kbo_current_build_caller_rva_matches(caller_rva, OOTP27_AMATEUR_GENERATION_TEAM_ADD_CALLER_00A319A8_RVA);
}

void kbo_amateur_league_batch_clear(uint32_t league_id)
{
    memset(g_kbo_amateur_league_batch_players, 0, sizeof(g_kbo_amateur_league_batch_players));
    memset(g_kbo_amateur_league_batch_source_teams, 0, sizeof(g_kbo_amateur_league_batch_source_teams));
    memset(g_kbo_amateur_league_batch_team_ids, 0, sizeof(g_kbo_amateur_league_batch_team_ids));
    memset(g_kbo_amateur_deferred_team_adds, 0, sizeof(g_kbo_amateur_deferred_team_adds));
    g_kbo_amateur_league_batch_league_id = league_id;
    g_kbo_amateur_league_batch_player_count = 0;
    g_kbo_amateur_league_batch_team_count = 0;
    g_kbo_amateur_deferred_team_add_count = 0;
    g_kbo_amateur_league_batch_last_tick = 0u;
}
