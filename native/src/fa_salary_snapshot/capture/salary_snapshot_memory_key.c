#include "salary_snapshot_memory_key.h"

#include <stdio.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../runtime_memory/runtime_memory.h"

static int kbo_fa_salary_snapshot_memory_key_text_valid(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    int len = 0;
    int has_alpha = 0;
    while (text[len] != '\0' && len < 63) {
        char ch = text[len];
        if (!((ch >= 'a' && ch <= 'z')
                || (ch >= 'A' && ch <= 'Z')
                || (ch >= '0' && ch <= '9')
                || ch == '-' || ch == '_' || ch == '.')) {
            return 0;
        }
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
            has_alpha = 1;
        }
        len++;
    }
    return len >= 4 && len < 63 && has_alpha;
}

static int kbo_fa_salary_snapshot_copy_memory_player_export_key_at(
    uint8_t* player,
    uint32_t offset,
    char* out,
    size_t out_size)
{
    if (player == NULL || out == NULL || out_size == 0u
            || !memory_range_readable(player + offset, sizeof(uintptr_t))) {
        return 0;
    }
    uintptr_t ptr = *(uintptr_t*)(player + offset);
    if (ptr == 0u || !memory_range_readable((void*)ptr, 64u)) {
        return 0;
    }
    const char* text = (const char*)ptr;
    if (!kbo_fa_salary_snapshot_memory_key_text_valid(text)) {
        return 0;
    }
    snprintf(out, out_size, "%s", text);
    return out[0] != '\0';
}

void kbo_fa_salary_snapshot_copy_memory_player_key(uint8_t* player, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (player == NULL) {
        return;
    }

    static const uint32_t export_key_offsets[] = OOTP27_PLAYER_EXPORT_KEY_STRING_OFFSETS;
    for (int i = 0; i < (int)(sizeof(export_key_offsets) / sizeof(export_key_offsets[0])); i++) {
        if (kbo_fa_salary_snapshot_copy_memory_player_export_key_at(
                player,
                export_key_offsets[i],
                out,
                out_size)) {
            return;
        }
    }
}
