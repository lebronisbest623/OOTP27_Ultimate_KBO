#include "..\amateur_assignment_ortools.h"

int kbo_amateur_ortools_write_request(
    const char* path,
    uint8_t* player,
    uint32_t league_id,
    uint32_t current_team_id,
    uint8_t current_reputation,
    int32_t quality_score,
    int player_tier,
    int32_t target_reputation,
    KboAmateurAssignmentCandidate* candidates,
    int count)
{
    char tmp_path[MAX_PATH] = {0};
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE* file = fopen(tmp_path, "wb");
    if (file == NULL) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    int32_t target_max_players = kbo_amateur_assignment_target_max_players(league_id);
    fprintf(
        file,
        "player_id,league_id,age,quality_score,player_tier,target_reputation,current_team_id,current_reputation,target_max_players,team_id,reputation,team_tier,player_count,hitter_count,rejected,draft_penalty_stages\r\n");

    for (int i = 0; i < count; i++) {
        int team_tier = kbo_amateur_assignment_team_tier(league_id, candidates[i].reputation);
        int rejected = kbo_amateur_assignment_target_rejected(league_id, candidates[i].team_id);
        int draft_penalty = kbo_cbt_draft_penalty_stages_for_team(candidates[i].team_id);
        fprintf(
            file,
            "%u,%u,%d,%d,%d,%d,%u,%u,%d,%u,%u,%d,%d,%d,%d,%d\r\n",
            player_id,
            league_id,
            (int)age,
            quality_score,
            player_tier,
            target_reputation,
            current_team_id,
            (uint32_t)current_reputation,
            target_max_players,
            candidates[i].team_id,
            (uint32_t)candidates[i].reputation,
            team_tier,
            candidates[i].player_count,
            candidates[i].hitter_count,
            rejected,
            draft_penalty);
    }

    fclose(file);
    if (!MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp_path);
        return 0;
    }
    return 1;
}
