#include "captain_seed_display_resolve.h"
#include "../../../runtime_memory/runtime_memory.h"

void kbo_captain_display_loaded_key(uint32_t season, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    char path[KBO_UTF8_PATH_BYTES] = {0};
    char csv_key[192] = "csv:missing";
    if (kbo_captain_selection_csv_path(season, path, sizeof(path))) {
        WIN32_FILE_ATTRIBUTE_DATA attrs;
        if (kbo_get_file_attributes_ex_utf8(path, &attrs)) {
            snprintf(
                csv_key,
                sizeof(csv_key),
                "csv:%08lx%08lx:%08lx%08lx",
                (unsigned long)attrs.ftLastWriteTime.dwHighDateTime,
                (unsigned long)attrs.ftLastWriteTime.dwLowDateTime,
                (unsigned long)attrs.nFileSizeHigh,
                (unsigned long)attrs.nFileSizeLow);
        }
    }

    snprintf(
        out,
        out_size,
        "%s|%s",
        g_kbo_captain_seed_loaded_key,
        csv_key);
}

static uint8_t* kbo_captain_display_find_player_by_id(uint32_t player_id, int* out_vector_available)
{
    if (out_vector_available != NULL) {
        *out_vector_available = 0;
    }
    if (player_id == 0u) {
        return NULL;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }
    if (player_vector == 0u || player_count <= 0 || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return NULL;
    }
    if (out_vector_available != NULL) {
        *out_vector_available = 1;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return player;
        }
    }
    return NULL;
}

static int kbo_captain_display_player_name_is_generic_id(const char* name)
{
    if (name == NULL || strncmp(name, "Player #", 8u) != 0) {
        return 0;
    }
    const char* p = name + 8u;
    if (*p < '0' || *p > '9') {
        return 0;
    }
    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
    }
    return 1;
}

static int kbo_captain_display_player_name_needs_repair(const char* name)
{
    return name == NULL
        || name[0] == '\0'
        || strcmp(name, "Unknown player") == 0
        || kbo_captain_display_player_name_is_generic_id(name);
}

static int kbo_captain_display_copy_seed_player_name(
    const KboCaptainSelectionRow* row,
    char* out,
    size_t out_size)
{
    if (row == NULL || out == NULL || out_size == 0u || row->team_id == 0u || !row->seeded) {
        return 0;
    }

    kbo_ensure_captain_seeds_loaded();
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(row->team_id, 0);
    KboCaptainSeed seed;
    if (!kbo_find_best_captain_seed_for_team(row->season, row->league_id, row->team_id, team, &seed)) {
        return 0;
    }
    if (seed.player_name[0] == '\0') {
        return 0;
    }
    if (seed.player_id != 0u && row->player_id != 0u && seed.player_id != row->player_id) {
        return 0;
    }

    snprintf(out, out_size, "%s", seed.player_name);
    return 1;
}

static int kbo_captain_display_copy_seed_player_name_by_metadata(
    const KboCaptainSelectionRow* row,
    char* out,
    size_t out_size)
{
    if (row == NULL || out == NULL || out_size == 0u || !row->seeded) {
        return 0;
    }

    const char* seed_key = NULL;
    if (strncmp(row->reason, "seed:", 5u) == 0 && row->reason[5] != '\0') {
        seed_key = row->reason + 5u;
    }
    if (seed_key == NULL && row->player_id == 0u) {
        return 0;
    }

    kbo_ensure_captain_seeds_loaded();
    int found = 0;
    char found_name[128] = {0};
    kbo_lock_captain_seeds();
    for (int i = 0; i < g_kbo_captain_seed_count; i++) {
        const KboCaptainSeed* seed = &g_kbo_captain_seeds[i];
        if (!seed->active || seed->player_name[0] == '\0') {
            continue;
        }
        if (seed->season != 0u && seed->season != row->season) {
            continue;
        }
        if (seed->league_id != 0u && row->league_id != 0u && seed->league_id != row->league_id) {
            continue;
        }
        if (seed->team_id != 0u && row->team_id != 0u && seed->team_id != row->team_id) {
            continue;
        }
        if (seed_key != NULL && seed->player_key[0] != '\0' && _stricmp(seed->player_key, seed_key) == 0) {
            snprintf(found_name, sizeof(found_name), "%s", seed->player_name);
            found = 1;
            break;
        }
        if (seed->player_id != 0u && row->player_id != 0u && seed->player_id == row->player_id) {
            snprintf(found_name, sizeof(found_name), "%s", seed->player_name);
            found = 1;
            break;
        }
    }
    kbo_unlock_captain_seeds();

    if (!found) {
        return 0;
    }
    snprintf(out, out_size, "%s", found_name);
    return 1;
}

static int kbo_captain_display_fill_from_row(
    const KboCaptainSelectionRow* row,
    KboCaptainDisplayCache* out)
{
    if (row == NULL || out == NULL || row->team_id == 0u) {
        return 0;
    }

    int vector_available = 0;
    uint8_t* player = kbo_captain_display_find_player_by_id(row->player_id, &vector_available);
    if (vector_available) {
        if (player == NULL
                || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
                || !kbo_player_current_assignment_matches_team_or_affiliate(player, row->team_id)) {
            return 0;
        }
    }

    out->player_id = row->player_id;
    snprintf(out->player_name, sizeof(out->player_name), "%s", row->player_name);
    snprintf(out->source, sizeof(out->source), "selection_csv");
    if (player != NULL) {
        char resolved_name[128] = {0};
        kbo_copy_player_display_name(player, resolved_name, sizeof(resolved_name));
        if (resolved_name[0] != '\0' && strcmp(resolved_name, "Unknown player") != 0) {
            snprintf(out->player_name, sizeof(out->player_name), "%s", resolved_name);
        }
    }
    if (kbo_captain_display_player_name_needs_repair(out->player_name)) {
        if (!kbo_captain_display_copy_seed_player_name(row, out->player_name, sizeof(out->player_name))) {
            kbo_captain_display_copy_seed_player_name_by_metadata(row, out->player_name, sizeof(out->player_name));
        }
    }
    out->found = out->player_name[0] != '\0' || out->player_id != 0u;
    return out->found;
}

static int kbo_captain_display_fill_from_selection_csv(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    KboCaptainDisplayCache* out)
{
    if (out == NULL || team_id == 0u) {
        return 0;
    }

    KboCaptainSelectionRow rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(rows, 0, sizeof(rows));
    int row_count = kbo_captain_load_selection_csv(season, rows, KBO_CAPTAIN_MAX_TEAMS);
    for (int i = 0; i < row_count; i++) {
        if (rows[i].team_id != team_id) {
            continue;
        }
        if (rows[i].league_id != 0u && league_id != 0u && rows[i].league_id != league_id) {
            continue;
        }
        return kbo_captain_display_fill_from_row(&rows[i], out);
    }
    return 0;
}

int kbo_captain_compute_display_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    const char* loaded_key,
    KboCaptainDisplayCache* out)
{
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->season = season;
    out->league_id = league_id;
    out->team_id = team_id;
    snprintf(out->loaded_key, sizeof(out->loaded_key), "%s", loaded_key != NULL ? loaded_key : "");

    if (kbo_captain_display_fill_from_selection_csv(season, league_id, team_id, out)) {
        return 1;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    KboCaptainSeed seed;
    if (!kbo_find_best_captain_seed_for_team(season, league_id, team_id, team, &seed)) {
        return 0;
    }

    snprintf(out->player_name, sizeof(out->player_name), "%s", seed.player_name);
    snprintf(out->source, sizeof(out->source), "%s", seed.source);
    out->player_id = seed.player_id;

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    int vector_available = 0;
    int resolved_seed_player = 0;
    if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            && player_vector != 0u
            && player_count > 0
            && player_count <= KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT
            && memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        vector_available = 1;
        for (int32_t i = 0; i < player_count; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) {
                continue;
            }
            uint8_t* player = (uint8_t*)player_ptr;
            if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
                continue;
            }
            if (!kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
                continue;
            }
            if (!kbo_captain_seed_matches_player(&seed, player)) {
                continue;
            }

            out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            resolved_seed_player = 1;
            char resolved_name[128] = {0};
            kbo_copy_player_display_name(player, resolved_name, sizeof(resolved_name));
            if (resolved_name[0] != '\0' && strcmp(resolved_name, "Unknown player") != 0) {
                snprintf(out->player_name, sizeof(out->player_name), "%s", resolved_name);
            }
            break;
        }
    }

    if (vector_available && !resolved_seed_player) {
        out->player_name[0] = '\0';
        out->source[0] = '\0';
        out->player_id = 0u;
        out->found = 0;
        return 0;
    }

    out->found = out->player_name[0] != '\0' || out->player_id != 0u;
    return out->found;
}
