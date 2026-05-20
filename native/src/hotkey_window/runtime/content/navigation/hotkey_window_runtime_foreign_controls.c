#include "../hotkey_window_runtime_content.h"
#include "../../hotkey_window_domain_contract.h"

int kbo_hub_foreign_rights_ui_selected(void)
{
    return g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS
        || (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
            && g_kbo_hub_selected_foreign_subview == KBO_HUB_FOREIGN_SUBVIEW_RIGHTS);
}

uint32_t kbo_parse_foreign_candidate_player_id_from_line(const char* line)
{
    if (line == NULL || (line[0] != ' ' && line[0] != '>')) {
        return 0u;
    }

    const char* p = line + 1;
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0' || strncmp(p, "PLAYER", 6) == 0) {
        return 0u;
    }

    const char* number = line + 27;
    while (*number == ' ') {
        number++;
    }
    if (*number < '0' || *number > '9') {
        return 0u;
    }

    unsigned long raw = strtoul(number, NULL, 10);
    if (raw == 0ul || raw > UINT32_MAX) {
        return 0u;
    }
    return (uint32_t)raw;
}

int kbo_select_foreign_candidate_from_edit_click(HWND edit, LPARAM lparam)
{
    if (edit == NULL || !kbo_hub_foreign_rights_ui_selected()) {
        return 0;
    }

    POINT point = {(int)(short)LOWORD(lparam), (int)(short)HIWORD(lparam)};
    LRESULT char_result = SendMessageA(edit, EM_CHARFROMPOS, 0, (LPARAM)&point);
    int char_index = LOWORD(char_result);
    int line_index = (int)SendMessageA(edit, EM_LINEFROMCHAR, (WPARAM)char_index, 0);
    if (line_index < 0) {
        return 0;
    }

    char line[256] = {0};
    *((WORD*)line) = (WORD)(sizeof(line) - 1);
    LRESULT copied = SendMessageA(edit, EM_GETLINE, (WPARAM)line_index, (LPARAM)line);
    if (copied <= 0) {
        return 0;
    }
    if (copied >= (LRESULT)sizeof(line)) {
        copied = (LRESULT)sizeof(line) - 1;
    }
    line[copied] = '\0';

    uint32_t player_id = kbo_parse_foreign_candidate_player_id_from_line(line);
    if (player_id == 0u) {
        return 0;
    }

    g_kbo_hub_selected_foreign_player_id = player_id;
    kbo_log_runtimef("foreign rights ui: selected player=%u from row=%d", player_id, line_index);
    kbo_refresh_hotkey_window();
    if (g_kbo_hotkey_window != NULL) {
        InvalidateRect(g_kbo_hotkey_window, NULL, TRUE);
    }
    return 1;
}

LRESULT CALLBACK kbo_hotkey_edit_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_LBUTTONDOWN && kbo_select_foreign_candidate_from_edit_click(hwnd, lparam)) {
        return 0;
    }

    if (g_kbo_hotkey_edit_original_proc != NULL) {
        return CallWindowProcA(g_kbo_hotkey_edit_original_proc, hwnd, message, wparam, lparam);
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

void kbo_refresh_foreign_rights_controls(void)
{
    if (g_kbo_foreign_list == NULL) {
        return;
    }

    SendMessageA(g_kbo_foreign_list, LB_RESETCONTENT, 0, 0);

    if (!kbo_hub_foreign_rights_ui_selected() || g_kbo_hub_selected_team_id == 0u) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }

    int selected_index = -1;
    uint32_t today = 0u;
    uint32_t window_end = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);
    kbo_current_foreign_waiver_window_dates(NULL, &window_end);
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
    int window_open = kbo_is_foreign_waiver_negotiation_window_open();
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        char latest_action[16] = {0};
        int has_decision = kbo_foreign_waiver_latest_decision_action(
            window_end,
            g_kbo_hub_selected_team_id,
            player_id,
            latest_action,
            sizeof(latest_action));
        int has_active_right = today != 0u
            && kbo_has_active_foreign_waiver_right(g_kbo_hub_selected_team_id, player_id, today)
            && (!has_decision || _stricmp(latest_action, "SKIP") != 0);
        const char* decision_status = "Pending";
        if (has_active_right) {
            decision_status = "Retained";
        } else if (has_decision && _stricmp(latest_action, "SKIP") == 0) {
            decision_status = "Skipped";
        } else if (has_decision && _stricmp(latest_action, "RETAIN") == 0) {
            decision_status = "RetainQueued";
        }
        if (player_id == 0u
                || !kbo_player_is_foreign_for_kbo_rights(player)
                || (window_open
                    ? decision_team_id != g_kbo_hub_selected_team_id
                    : !has_active_right)) {
            continue;
        }

        char player_name[64] = {0};
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        uint8_t restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        uint8_t dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        uint8_t inj_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];

        char label[192] = {0};
        snprintf(
            label,
            sizeof(label),
            "%-28.28s   ID %-8u   Nation %-4u   %-12.12s %s%s%s",
            player_name,
            player_id,
            nation_id,
            decision_status,
            restricted ? "Restricted " : "",
            dfa ? "DFA " : "",
            inj_active ? "Injured" : "Active");

        LRESULT index = SendMessageA(g_kbo_foreign_list, LB_ADDSTRING, 0, (LPARAM)label);
        if (index >= 0) {
            SendMessageA(g_kbo_foreign_list, LB_SETITEMDATA, (WPARAM)index, (LPARAM)player_id);
            if (player_id == g_kbo_hub_selected_foreign_player_id) {
                selected_index = (int)index;
            }
        }
    }

    if (selected_index >= 0) {
        SendMessageA(g_kbo_foreign_list, LB_SETCURSEL, (WPARAM)selected_index, 0);
    } else if (SendMessageA(g_kbo_foreign_list, LB_GETCOUNT, 0, 0) > 0) {
        SendMessageA(g_kbo_foreign_list, LB_SETCURSEL, 0, 0);
        g_kbo_hub_selected_foreign_player_id = (uint32_t)SendMessageA(g_kbo_foreign_list, LB_GETITEMDATA, 0, 0);
    }
}

void kbo_apply_foreign_rights_button(int retain)
{
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        kbo_log_runtimef("foreign rights ui: %s blocked by window closed", retain ? "RETAIN" : "SKIP");
        return;
    }

    uint32_t player_id = g_kbo_hub_selected_foreign_player_id;
    if (g_kbo_foreign_list != NULL) {
        LRESULT sel = SendMessageA(g_kbo_foreign_list, LB_GETCURSEL, 0, 0);
        if (sel != LB_ERR) {
            LRESULT data = SendMessageA(g_kbo_foreign_list, LB_GETITEMDATA, (WPARAM)sel, 0);
            if (data != LB_ERR && data != 0) {
                player_id = (uint32_t)data;
                g_kbo_hub_selected_foreign_player_id = player_id;
            }
        }
    }

    if (player_id != 0u && kbo_append_foreign_waiver_user_decision(g_kbo_hub_selected_team_id, player_id, retain)) {
        kbo_log_runtimef("foreign rights ui: queued %s team=%u player=%u", retain ? "RETAIN" : "SKIP", g_kbo_hub_selected_team_id, player_id);
        process_foreign_waiver_commands();
        kbo_refresh_foreign_rights_controls();
        kbo_refresh_hotkey_window();
        if (g_kbo_hotkey_window != NULL) {
            InvalidateRect(g_kbo_hotkey_window, NULL, TRUE);
        }
    }
}
void kbo_webview_append_league_dropdown(KboWindowTextBuffer* buffer, uint32_t current_year)
{
    if (buffer == NULL) {
        return;
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return;
    }

    uint32_t leagues[256] = {0};
    int league_count = 0;
    for (int32_t i = 0; i < team_count && league_count < 256; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) { continue; }
        if (kbo_hub_team_hidden_from_dropdown(team)) { continue; }
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0) { continue; }
        int exists = 0;
        for (int j = 0; j < league_count; j++) {
            if (leagues[j] == league_id) { exists = 1; break; }
        }
        if (!exists) { leagues[league_count++] = league_id; }
    }
    if (league_count <= 0) {
        return;
    }

    kbo_window_text_appendf(buffer, "<div class='dropdown leagueMenu'>");
    for (int i = 0; i < league_count; i++) {
        char label[128] = {0};
        char logo_path[MAX_PATH] = {0};
        kbo_hub_copy_league_display_name_fast(leagues[i], label, sizeof(label));
        kbo_hub_get_league_logo_path(leagues[i], current_year, logo_path, sizeof(logo_path));
        kbo_window_text_appendf(buffer, "<a class='ddItem%s' href='kbo://setleague/%u'>",
            leagues[i] == g_kbo_hub_selected_league_id ? " selected" : "", leagues[i]);
        kbo_webview_append_dropdown_logo(buffer, logo_path);
        kbo_window_text_appendf(buffer, "<span class='ddText'>");
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer, "</span></a>");
    }
    kbo_window_text_appendf(buffer, "</div>");
}

void kbo_webview_append_team_dropdown(KboWindowTextBuffer* buffer, uint32_t current_year)
{
    if (buffer == NULL) {
        return;
    }
    if (g_kbo_hub_selected_league_id == 0) {
        kbo_hub_ensure_valid_selection();
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return;
    }

    uint32_t teams[512] = {0};
    int filtered_count = 0;
    for (int32_t i = 0; i < team_count && filtered_count < 512; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) { continue; }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) != g_kbo_hub_selected_league_id) { continue; }
        if (kbo_hub_team_hidden_from_dropdown(team)) { continue; }
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id != 0) { teams[filtered_count++] = team_id; }
    }
    if (filtered_count <= 0) {
        return;
    }

    kbo_window_text_appendf(buffer, "<div class='dropdown teamMenu'>");
    for (int i = 0; i < filtered_count; i++) {
        char label[128] = {0};
        char logo_path[MAX_PATH] = {0};
        kbo_hub_copy_team_display_name_by_id(teams[i], label, sizeof(label), NULL);
        kbo_hub_get_team_logo_path(teams[i], current_year, logo_path, sizeof(logo_path));
        kbo_window_text_appendf(buffer, "<a class='ddItem%s' href='kbo://setteam/%u'>",
            teams[i] == g_kbo_hub_selected_team_id ? " selected" : "", teams[i]);
        kbo_webview_append_dropdown_logo(buffer, logo_path);
        kbo_window_text_appendf(buffer, "<span class='ddText'>");
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer, "</span></a>");
    }
    kbo_window_text_appendf(buffer, "</div>");
}

