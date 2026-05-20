#include "ui_futures_league_view_helpers.h"

#include <stdio.h>

#include "../../../core/league_roles/kbo_league_roles.h"
#include "../../runtime/hotkey_window_runtime_shared.h"

void kbo_futures_ui_format_yyyymmdd(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u/%02u/%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

void kbo_futures_ui_format_cash(int32_t value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (value >= 1000000 || value <= -1000000) {
        snprintf(out, out_size, "$%.1fM", (double)value / 1000000.0);
        return;
    }
    if (value >= 1000 || value <= -1000) {
        snprintf(out, out_size, "$%dk", value / 1000);
        return;
    }
    snprintf(out, out_size, "$%d", value);
}

void kbo_futures_ui_copy_player_name(uintptr_t player_ptr, uint32_t player_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (kbo_player_pointer_plausible(player_ptr)
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        kbo_hub_copy_player_display_name((uint8_t*)player_ptr, out, out_size);
    }
    if (out[0] == '\0') {
        snprintf(out, out_size, "#%u", player_id);
    }
}

const char* kbo_futures_ui_position_label(uintptr_t player_ptr)
{
    if (kbo_player_pointer_plausible(player_ptr)
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return kbo_webview_player_position_label((uint8_t*)player_ptr, 0u);
    }
    return "-";
}

void kbo_futures_ui_copy_team_name(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    kbo_hub_copy_team_display_name_by_id(team_id, out, out_size, NULL);
    if (out[0] == '\0') {
        snprintf(out, out_size, "#%u", team_id);
    }
}

static uint32_t kbo_futures_ui_read_team_league_id(uint8_t* team)
{
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

uint32_t kbo_futures_ui_resolve_buyer_team_id(uint32_t selected_team_id)
{
    if (selected_team_id == 0u) {
        return 0u;
    }

    uint8_t* selected_team = find_kbo_team_by_numeric_id_any_league(selected_team_id, 1);
    if (selected_team == NULL || !memory_range_readable(selected_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = kbo_league_role_main_league_id();
    }

    if (kbo_futures_ui_read_team_league_id(selected_team) == kbo_league_id) {
        return selected_team_id;
    }

    if (!memory_range_readable(selected_team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }

    uint32_t parent_team_id = *(uint32_t*)(selected_team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    if (parent_team_id == 0u || parent_team_id == selected_team_id) {
        return 0u;
    }

    uint8_t* parent_team = find_kbo_team_by_numeric_id_any_league(parent_team_id, 1);
    if (parent_team == NULL || !memory_range_readable(parent_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    if (kbo_futures_ui_read_team_league_id(parent_team) != kbo_league_id) {
        return 0u;
    }

    return parent_team_id;
}
