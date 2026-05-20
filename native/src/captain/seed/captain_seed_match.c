#include "../internal/captain_selection_internal.h"
#include "../../core/dates/constants/kbo_date_constants.h"

static int kbo_captain_seed_key_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9')
        || ch == '_' || ch == '-';
}

static int kbo_captain_seed_key_text_matches(const char* text, const char* key)
{
    char copied[KBO_CAPTAIN_SEED_KEY_BYTES] = {0};
    if (text == NULL || key == NULL || key[0] == '\0') {
        return 0;
    }
    if (!copy_limited_ascii_string(text, copied, sizeof(copied))) {
        return 0;
    }
    return _stricmp(copied, key) == 0;
}

static int kbo_captain_player_string_slot_contains_seed_key(uint8_t* player, uint32_t offset, const char* key)
{
    if (player == NULL || key == NULL || offset >= OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }

    char text[KBO_CAPTAIN_SEED_KEY_BYTES] = {0};
    if (offset + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET + sizeof(uintptr_t) <= OOTP27_PLAYER_SCAN_BYTES
            && copy_ootp_string_object_text(player, offset, text, sizeof(text))
            && _stricmp(text, key) == 0) {
        return 1;
    }

    if (offset + sizeof(uintptr_t) <= OOTP27_PLAYER_SCAN_BYTES
            && memory_range_readable(player + offset, sizeof(uintptr_t))) {
        uintptr_t ptr = *(uintptr_t*)(player + offset);
        if (ptr != 0u && kbo_captain_seed_key_text_matches((const char*)ptr, key)) {
            return 1;
        }
    }

    if (kbo_captain_seed_key_text_matches((const char*)(player + offset), key)) {
        return 1;
    }
    return 0;
}

static int kbo_captain_player_contains_seed_key(uint8_t* player, const char* key)
{
    if (player == NULL || key == NULL || key[0] == '\0' || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    size_t key_len = strlen(key);
    if (key_len < 3u || key_len >= KBO_CAPTAIN_SEED_KEY_BYTES || key_len >= OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }

    static const uint32_t export_key_offsets[] = { 0x1140u, 0x1188u, 0x11a0u };
    for (int i = 0; i < (int)(sizeof(export_key_offsets) / sizeof(export_key_offsets[0])); i++) {
        if (kbo_captain_player_string_slot_contains_seed_key(player, export_key_offsets[i], key)) {
            return 1;
        }
    }

    for (size_t i = 0; i + key_len < OOTP27_PLAYER_SCAN_BYTES; i++) {
        if (memcmp(player + i, key, key_len) != 0) {
            continue;
        }
        char before = i > 0u ? (char)player[i - 1u] : '\0';
        char after = (char)player[i + key_len];
        if (!kbo_captain_seed_key_char(before) && !kbo_captain_seed_key_char(after)) {
            return 1;
        }
    }
    return 0;
}

static int kbo_captain_seed_matches_team(const KboCaptainSeed* seed, uint32_t team_id, uint8_t* team)
{
    if (seed == NULL) {
        return 0;
    }
    if (seed->team_id != 0u) {
        return seed->team_id == team_id;
    }
    return seed->team_code[0] != '\0'
        && team != NULL
        && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)
        && team_has_ootp_string_text(team, seed->team_code);
}

int kbo_captain_seed_matches_player(const KboCaptainSeed* seed, uint8_t* player)
{
    if (seed == NULL || player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (seed->player_id != 0u && seed->player_id != player_id) {
        return 0;
    }
    if (seed->player_key[0] != '\0' && !kbo_captain_player_contains_seed_key(player, seed->player_key)) {
        return 0;
    }
    return seed->player_id != 0u || seed->player_key[0] != '\0';
}

static int kbo_captain_seed_match_rank(
    const KboCaptainSeed* seed,
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id)
{
    int rank = (int)seed->source_rank * 1000000;
    if (seed->season == season) {
        rank += 100000;
    }
    if (seed->league_id == league_id) {
        rank += 10000;
    }
    if (seed->team_id == team_id && team_id != 0u) {
        rank += 1000;
    }
    rank += seed->priority;
    return rank;
}

int kbo_find_best_captain_seed_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint8_t* team,
    KboCaptainSeed* out_seed)
{
    if (out_seed != NULL) {
        memset(out_seed, 0, sizeof(*out_seed));
    }
    if (team_id == 0u) {
        return 0;
    }

    int best_rank = -2147483647;
    KboCaptainSeed best;
    memset(&best, 0, sizeof(best));

    kbo_lock_captain_seeds();
    for (int i = 0; i < g_kbo_captain_seed_count; i++) {
        const KboCaptainSeed* seed = &g_kbo_captain_seeds[i];
        if (!seed->active) {
            continue;
        }
        if (seed->season != 0u && seed->season != season) {
            continue;
        }
        if (seed->league_id != 0u && seed->league_id != league_id) {
            continue;
        }
        if (!kbo_captain_seed_matches_team(seed, team_id, team)) {
            continue;
        }

        int rank = kbo_captain_seed_match_rank(seed, season, league_id, team_id);
        if (rank > best_rank) {
            best = *seed;
            best_rank = rank;
        }
    }
    kbo_unlock_captain_seeds();

    if (best_rank == -2147483647) {
        return 0;
    }
    if (out_seed != NULL) {
        *out_seed = best;
    }
    return 1;
}

int kbo_captain_seed_available_for_season(uint32_t season, uint32_t league_id)
{
    if (season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    kbo_ensure_captain_seeds_loaded();
    int available = 0;
    kbo_lock_captain_seeds();
    for (int i = 0; i < g_kbo_captain_seed_count; i++) {
        const KboCaptainSeed* seed = &g_kbo_captain_seeds[i];
        if (!seed->active) {
            continue;
        }
        if (seed->season != 0u && seed->season != season) {
            continue;
        }
        if (seed->league_id != 0u && seed->league_id != league_id) {
            continue;
        }
        available = 1;
        break;
    }
    kbo_unlock_captain_seeds();
    return available;
}

int kbo_find_captain_seed_for_player(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint8_t* team,
    uint8_t* player,
    KboCaptainSeed* out_seed)
{
    if (out_seed != NULL) {
        memset(out_seed, 0, sizeof(*out_seed));
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    kbo_ensure_captain_seeds_loaded();
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int best_rank = -2147483647;
    KboCaptainSeed best;
    memset(&best, 0, sizeof(best));

    kbo_lock_captain_seeds();
    for (int i = 0; i < g_kbo_captain_seed_count; i++) {
        const KboCaptainSeed* seed = &g_kbo_captain_seeds[i];
        if (!seed->active) {
            continue;
        }
        if (seed->season != 0u && seed->season != season) {
            continue;
        }
        if (seed->league_id != 0u && seed->league_id != league_id) {
            continue;
        }
        if (!kbo_captain_seed_matches_team(seed, team_id, team)) {
            continue;
        }
        if (seed->player_id != 0u && seed->player_id != player_id) {
            continue;
        }
        if (seed->player_key[0] != '\0' && !kbo_captain_player_contains_seed_key(player, seed->player_key)) {
            continue;
        }

        int rank = kbo_captain_seed_match_rank(seed, season, league_id, team_id);
        if (rank > best_rank) {
            best = *seed;
            best_rank = rank;
        }
    }
    kbo_unlock_captain_seeds();

    if (best_rank == -2147483647) {
        return 0;
    }
    if (out_seed != NULL) {
        *out_seed = best;
    }
    return 1;
}
