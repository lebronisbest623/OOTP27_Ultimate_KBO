#include "../hotkey_window_runtime_content.h"

#include <stdio.h>
#include <string.h>
int kbo_hub_argb_team_color_to_hex(uint32_t argb, char* out, size_t out_size)
{
    if (out == NULL || out_size < 8) {
        return 0;
    }

    uint32_t alpha = (argb >> 24u) & 0xffu;
    uint32_t rgb = argb & 0x00ffffffu;
    if (alpha == 0u && rgb == 0u) {
        return 0;
    }

    snprintf(out, out_size, "#%06X", rgb);
    return 1;
}

int kbo_hub_copy_team_bar_colors(uint32_t team_id, char* primary, size_t primary_size, char* secondary, size_t secondary_size)
{
    if (primary != NULL && primary_size > 0) {
        snprintf(primary, primary_size, "#f04a22");
    }
    if (secondary != NULL && secondary_size > 0) {
        snprintf(secondary, secondary_size, "#2c2c2c");
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_PRIMARY_COLOR_OFFSET, 12u)) {
        return 0;
    }

    uint32_t primary_argb = *(uint32_t*)(team + OOTP27_KBO_TEAM_PRIMARY_COLOR_OFFSET);
    uint32_t secondary_argb = *(uint32_t*)(team + OOTP27_KBO_TEAM_SECONDARY_COLOR_OFFSET);
    int primary_ok = kbo_hub_argb_team_color_to_hex(primary_argb, primary, primary_size);
    int secondary_ok = kbo_hub_argb_team_color_to_hex(secondary_argb, secondary, secondary_size);
    return primary_ok && secondary_ok;
}

