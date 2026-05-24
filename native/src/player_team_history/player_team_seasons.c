#include "internal/player_team_seasons_internal.h"

int kbo_player_team_seasons_count_by_key(uint32_t team_id, const char* player_key, int* out_season_count)
{
    if (out_season_count != NULL) {
        *out_season_count = 0;
    }
    if (team_id == 0u || player_key == NULL || player_key[0] == '\0') {
        return 0;
    }
    char team_code[16] = {0};
    kbo_player_team_seasons_copy_team_seed_code(team_id, team_code, sizeof(team_code));
    if (team_code[0] == '\0') {
        return 0;
    }

    kbo_player_team_seasons_ensure_seed_loaded();
    for (int i = 0; i < g_kbo_player_team_season_seed_count; i++) {
        if (_stricmp(g_kbo_player_team_season_seed[i].player_key, player_key) == 0
                && _stricmp(g_kbo_player_team_season_seed[i].team_code, team_code) == 0) {
            if (out_season_count != NULL) {
                *out_season_count = g_kbo_player_team_season_seed[i].season_count;
            }
            return 1;
        }
    }
    return 0;
}

int kbo_player_team_seasons_count_for_player(uint32_t team_id, uint8_t* player, int* out_season_count)
{
    if (out_season_count != NULL) {
        *out_season_count = 0;
    }
    if (team_id == 0u || player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    char team_code[16] = {0};
    kbo_player_team_seasons_copy_team_seed_code(team_id, team_code, sizeof(team_code));
    if (team_code[0] == '\0') {
        return 0;
    }

    char player_keys[4][64];
    int key_count = kbo_player_team_seasons_copy_player_export_keys(player, player_keys, 4);
    if (key_count <= 0) {
        return 0;
    }

    kbo_player_team_seasons_ensure_seed_loaded();
    int best_count = 0;
    for (int i = 0; i < g_kbo_player_team_season_seed_count; i++) {
        if (_stricmp(g_kbo_player_team_season_seed[i].team_code, team_code) != 0) {
            continue;
        }
        for (int k = 0; k < key_count; k++) {
            if (_stricmp(g_kbo_player_team_season_seed[i].player_key, player_keys[k]) == 0
                    && g_kbo_player_team_season_seed[i].season_count > best_count) {
                best_count = g_kbo_player_team_season_seed[i].season_count;
            }
        }
    }

    if (best_count <= 0) {
        return 0;
    }
    if (out_season_count != NULL) {
        *out_season_count = best_count;
    }
    return 1;
}

int kbo_player_team_total_seasons_for_player(uint8_t* player, int* out_season_count)
{
    if (out_season_count != NULL) {
        *out_season_count = 0;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    char player_keys[4][64];
    int key_count = kbo_player_team_seasons_copy_player_export_keys(player, player_keys, 4);
    if (key_count <= 0) {
        return 0;
    }

    kbo_player_team_seasons_ensure_seed_loaded();
    int best_count = 0;
    for (int k = 0; k < key_count; k++) {
        int total_count = 0;
        for (int i = 0; i < g_kbo_player_team_season_seed_count; i++) {
            if (_stricmp(g_kbo_player_team_season_seed[i].player_key, player_keys[k]) == 0) {
                total_count += g_kbo_player_team_season_seed[i].season_count;
            }
        }
        if (total_count > best_count) {
            best_count = total_count;
        }
    }

    if (best_count <= 0) {
        return 0;
    }
    if (out_season_count != NULL) {
        *out_season_count = best_count;
    }
    return 1;
}
