#include "../../runtime/common/custom_events_common.h"
#include "select_roster_ortools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/optimizer/kbo_optimizer.h"
#include "../../asian_games/player_eval/asian_games_player_eval.h"
#include "../../asian_games/roster/asian_games_roster_store.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"

static const char* kbo_asian_games_role_bucket_code(uint8_t role)
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
    return "UNK";
}

static int kbo_asian_games_write_ortools_request(
    const char* path,
    const KboAsianGamesCandidate* candidates,
    int candidate_count,
    const uint32_t* required_orgs,
    int required_org_count)
{
    char tmp_path[MAX_PATH] = {0};
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE* file = fopen(tmp_path, "wb");
    if (file == NULL) {
        return 0;
    }
    const KboAsianGamesRosterPolicy* policy = kbo_asian_games_roster_policy();
    int wildcard_age_min = kbo_asian_games_policy_wildcard_age_min();
    fputs(
        "player_id,score,age,role,role_bucket,org_team_id,required_org,wildcard,military_unserved,policy_roster_size,policy_max_wildcards,policy_team_max_players,policy_wildcard_age_min\r\n",
        file);
    for (int i = 0; i < candidate_count; i++) {
        const KboAsianGamesCandidate* candidate = &candidates[i];
        uint32_t player_id = candidate->entry.player_id;
        if (player_id == 0u) {
            continue;
        }
        int required = kbo_asian_games_find_org_index(
            required_orgs,
            required_org_count,
            candidate->org_team_id) >= 0;
        fprintf(
            file,
            "%u,%d,%u,%u,%s,%u,%d,%d,%d,%d,%d,%d,%d\r\n",
            player_id,
            candidate->entry.score,
            (uint32_t)candidate->entry.age,
            (uint32_t)candidate->entry.role,
            kbo_asian_games_role_bucket_code(candidate->entry.role),
            candidate->org_team_id,
            required,
            kbo_asian_games_policy_is_wildcard_age(candidate->entry.age) ? 1 : 0,
            candidate->entry.military_unserved ? 1 : 0,
            policy->roster_size,
            policy->max_wildcards,
            policy->team_max_players,
            wildcard_age_min);
    }
    fclose(file);
    if (!MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp_path);
        return 0;
    }
    return 1;
}

static int kbo_asian_games_find_candidate_by_player_id(
    const KboAsianGamesCandidate* candidates,
    int candidate_count,
    uint32_t player_id)
{
    if (candidates == NULL || player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].entry.player_id == player_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_asian_games_apply_ortools_result(
    const char* result_path,
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    int* selected_count,
    int* pitcher_count,
    int* catcher_count,
    int* infielder_count,
    int* outfielder_count,
    int* wildcard_count)
{
    if (result_path == NULL || candidates == NULL || selected_count == NULL
            || pitcher_count == NULL || catcher_count == NULL
            || infielder_count == NULL || outfielder_count == NULL || wildcard_count == NULL) {
        return 0;
    }

    FILE* file = fopen(result_path, "rb");
    if (file == NULL) {
        return 0;
    }
    char line[256] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    uint32_t selected_ids[KBO_ASIAN_GAMES_ROSTER_SIZE] = {0};
    int id_count = 0;
    int roster_size = kbo_asian_games_policy_roster_size();
    while (id_count < roster_size && fgets(line, sizeof(line), file) != NULL) {
        uint32_t player_id = (uint32_t)strtoul(line, NULL, 10);
        if (player_id == 0u) {
            continue;
        }
        int duplicate = 0;
        for (int i = 0; i < id_count; i++) {
            if (selected_ids[i] == player_id) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) {
            selected_ids[id_count++] = player_id;
        }
    }
    fclose(file);

    int target_count = candidate_count < roster_size
        ? candidate_count
        : roster_size;
    if (id_count < target_count) {
        return 0;
    }

    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    for (int i = 0; i < candidate_count; i++) {
        candidates[i].selected = 0u;
    }
    *selected_count = 0;
    *pitcher_count = 0;
    *catcher_count = 0;
    *infielder_count = 0;
    *outfielder_count = 0;
    *wildcard_count = 0;

    for (int i = 0; i < id_count && *selected_count < roster_size; i++) {
        int index = kbo_asian_games_find_candidate_by_player_id(candidates, candidate_count, selected_ids[i]);
        if (index < 0) {
            return 0;
        }
        KboAsianGamesRosterEntry entry = candidates[index].entry;
        entry.wildcard = kbo_asian_games_policy_is_wildcard_age(entry.age) ? 1u : 0u;
        g_kbo_asian_games_roster[*selected_count] = entry;
        candidates[index].selected = 1u;
        (*selected_count)++;
        if (kbo_asian_games_role_is_pitcher(entry.role)) {
            (*pitcher_count)++;
        } else if (kbo_asian_games_role_is_catcher(entry.role)) {
            (*catcher_count)++;
        } else if (kbo_asian_games_role_is_infielder(entry.role)) {
            (*infielder_count)++;
        } else if (kbo_asian_games_role_is_outfielder(entry.role)) {
            (*outfielder_count)++;
        }
        if (entry.wildcard) {
            (*wildcard_count)++;
        }
    }
    return *selected_count;
}

int kbo_select_asian_games_roster_ortools(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    const uint32_t* required_orgs,
    int required_org_count,
    int* selected_count,
    int* pitcher_count,
    int* catcher_count,
    int* infielder_count,
    int* outfielder_count,
    int* wildcard_count,
    const char* source)
{
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_ASIAN_GAMES_ORTOOLS_FILE)
            || candidates == NULL
            || candidate_count <= 0) {
        return 0;
    }

    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    if (!kbo_get_save_scoped_data_file("work\\asian_games_roster_ortools_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("work\\asian_games_roster_ortools_result.csv", result_path, sizeof(result_path))) {
        return 0;
    }
    if (!kbo_asian_games_write_ortools_request(
            request_path,
            candidates,
            candidate_count,
            required_orgs,
            required_org_count)) {
        return 0;
    }
    if (!kbo_optimizer_run_mode("asian_games_roster", request_path, result_path, (uint32_t)kbo_asian_games_roster_policy()->ortools_timeout_ms)) {
        return 0;
    }
    int applied = kbo_asian_games_apply_ortools_result(
        result_path,
        candidates,
        candidate_count,
        selected_count,
        pitcher_count,
        catcher_count,
        infielder_count,
        outfielder_count,
        wildcard_count);
    if (applied > 0) {
        kbo_log_runtimef(
            "KBO Asian Games OR-Tools roster selected source=%s candidates=%d selected=%d required_orgs=%d pitchers=%d catchers=%d infielders=%d outfielders=%d wildcards=%d",
            source != NULL ? source : "",
            candidate_count,
            applied,
            required_org_count,
            *pitcher_count,
            *catcher_count,
            *infielder_count,
            *outfielder_count,
            *wildcard_count);
    }
    return applied;
}
