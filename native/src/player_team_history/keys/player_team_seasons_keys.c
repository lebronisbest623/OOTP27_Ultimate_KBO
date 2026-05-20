#include "../internal/player_team_seasons_internal.h"

static int kbo_player_team_seasons_seed_key_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9')
        || ch == '_' || ch == '-';
}

static int kbo_player_team_seasons_export_key_usable(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    size_t len = strlen(text);
    if (len < 3u || len >= 64u) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        if (!kbo_player_team_seasons_seed_key_char(text[i])) {
            return 0;
        }
    }
    return 1;
}

static int kbo_player_team_seasons_add_export_key(
    char keys[][64],
    int* count,
    int max_count,
    const char* text)
{
    if (keys == NULL || count == NULL || *count >= max_count || !kbo_player_team_seasons_export_key_usable(text)) {
        return 0;
    }
    for (int i = 0; i < *count; i++) {
        if (_stricmp(keys[i], text) == 0) {
            return 0;
        }
    }
    snprintf(keys[*count], 64, "%s", text);
    (*count)++;
    return 1;
}

int kbo_player_team_seasons_copy_player_export_keys(uint8_t* player, char keys[][64], int max_count)
{
    if (keys == NULL || max_count <= 0) {
        return 0;
    }
    for (int i = 0; i < max_count; i++) {
        keys[i][0] = '\0';
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    int count = 0;
    static const uint32_t export_key_offsets[] = OOTP27_PLAYER_EXPORT_KEY_STRING_OFFSETS;
    for (int i = 0; i < (int)(sizeof(export_key_offsets) / sizeof(export_key_offsets[0])); i++) {
        uint32_t offset = export_key_offsets[i];
        char text[64] = {0};
        if (offset + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET + sizeof(uintptr_t) <= OOTP27_PLAYER_SCAN_BYTES
                && copy_ootp_string_object_text(player, offset, text, sizeof(text))) {
            kbo_player_team_seasons_add_export_key(keys, &count, max_count, text);
        }

        text[0] = '\0';
        if (offset + sizeof(uintptr_t) <= OOTP27_PLAYER_SCAN_BYTES
                && memory_range_readable(player + offset, sizeof(uintptr_t))) {
            uintptr_t ptr = *(uintptr_t*)(player + offset);
            if (ptr != 0u && copy_limited_ascii_string((const char*)ptr, text, sizeof(text))) {
                kbo_player_team_seasons_add_export_key(keys, &count, max_count, text);
            }
        }

        text[0] = '\0';
        if (offset < OOTP27_PLAYER_SCAN_BYTES
                && copy_limited_ascii_string((const char*)(player + offset), text, sizeof(text))) {
            kbo_player_team_seasons_add_export_key(keys, &count, max_count, text);
        }
    }
    return count;
}
