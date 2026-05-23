#include "../hotkey_window_runtime_content.h"
#include "../../hotkey_window_domain_contract.h"

RECT kbo_hub_nav_item_rect(int index, int width, int height)
{
    (void)width;
    RECT rect;
    rect.left   = 12 + (kbo_hub_skin_article_gap_x() / 2);
    rect.right  = rect.left + 156;
    rect.top    = 98 + (index * (34 + kbo_hub_skin_article_gap_y()));
    rect.bottom = rect.top + 34;
    if (rect.bottom > height - 12) {
        rect.bottom = height - 12;
    }
    return rect;
}

const char* kbo_hub_current_view_title(void)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY: return kbo_hub_text("\xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80",    "군경팀");
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c", "외국인 보류권");
    case KBO_HUB_VIEW_ASIAN_QUOTA: return kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98", "외국인 선수");
    case KBO_HUB_VIEW_ASIAN_GAMES: return kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84", "아시안게임");
    case KBO_HUB_VIEW_UPCOMING_FA:
    case KBO_HUB_VIEW_FA_CASES: return kbo_hub_text("FA", "FA");
    case KBO_HUB_VIEW_MOD_INFO: return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "모드 정보");
    case KBO_HUB_VIEW_SETTINGS: return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95",      "설정");
    case KBO_HUB_VIEW_REPUTATION: return kbo_hub_text("\xed\x8f\x89\xed\x8c\x90", "평판");
    case KBO_HUB_VIEW_CBT:        return kbo_hub_text("\xea\xb2\xbd\xec\x9f\x81\xea\xb7\xa0\xed\x98\x95\xec\x84\xb8", "경쟁균형세");
    case KBO_HUB_VIEW_FUTURES_LEAGUE: return kbo_hub_text("\xea\xb5\xac\xeb\xa7\xa4", "구매");
    case KBO_HUB_VIEW_SECONDARY_DRAFT: return kbo_hub_text("\x32\xec\xb0\xa8 \xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8", "Secondary Draft");
    default:                    return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "모드 정보");
    }
}

const char* kbo_hub_current_view_subtitle(void)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY:
        return kbo_hub_military_subnav_label(g_kbo_hub_selected_military_subview);
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xed\x8c\x80\xeb\xb3\x84 \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98 \xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c\xec\x9d\x84 \xed\x99\x95\xec\x9d\xb8\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4", "구단별 외국인 선수 보류권");
    case KBO_HUB_VIEW_ASIAN_QUOTA:
        return kbo_hub_foreign_subnav_label(g_kbo_hub_selected_foreign_subview);
    case KBO_HUB_VIEW_ASIAN_GAMES:
        switch (g_kbo_hub_selected_agames_subview) {
        case KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS:
            return kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84 \xeb\x8c\x80\xed\x9a\x8c", "아시안게임 대회");
        case KBO_HUB_AGAMES_SUBVIEW_SCHEDULE:
            return kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84 \xec\x9d\xbc\xec\xa0\x95", "아시안게임 일정");
        case KBO_HUB_AGAMES_SUBVIEW_ROSTER:
            return kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84 \xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "아시안게임 로스터");
        default:
            return kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84", "아시안게임");
        }
    case KBO_HUB_VIEW_UPCOMING_FA:
    case KBO_HUB_VIEW_FA_CASES:
        return kbo_hub_fa_subnav_label(g_kbo_hub_selected_fa_subview);
    case KBO_HUB_VIEW_MOD_INFO:
        return kbo_hub_mod_subnav_label(g_kbo_hub_selected_mod_subview);
    case KBO_HUB_VIEW_SETTINGS: return kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8\xec\x99\x80 \xea\xb2\x8c\xec\x9e\x84 \xed\x94\x8c\xeb\xa0\x88\xec\x9d\xb4 \xec\x84\xa4\xec\xa0\x95", "리그와 게임 플레이 설정");
    case KBO_HUB_VIEW_REPUTATION: return kbo_hub_text("\xec\xb5\x9c\xea\xb7\xbc 5\xeb\x85\x84 \xed\x8f\x89\xed\x8c\x90 \xeb\xb3\x80\xeb\x8f\x99", "최근 5년 평판 변동");
    case KBO_HUB_VIEW_CBT:        return kbo_hub_text("\xed\x8c\x80\xeb\xb3\x84 \xea\xb2\xbd\xec\x9f\x81\xea\xb7\xa0\xed\x98\x95\xec\x84\xb8 \xed\x98\x84\xed\x99\xa9", "구단별 경쟁균형세 현황");
    case KBO_HUB_VIEW_FUTURES_LEAGUE:
        return kbo_hub_futures_subnav_label(g_kbo_hub_selected_futures_subview);
    case KBO_HUB_VIEW_SECONDARY_DRAFT:
        return kbo_hub_secondary_draft_subnav_label(g_kbo_hub_selected_secondary_draft_subview);
    default:                    return kbo_hub_text("\xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xeb\x9f\xb0\xed\x83\x80\xec\x9e\x84 \xed\x8c\xa8\xec\xb9\x98 \xec\x83\x81\xed\x83\x9c\xeb\xa5\xbc \xed\x91\x9c\xec\x8b\x9c\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4",       "런처와 런타임 패치 상태");
    }
}

int kbo_hub_league_keeps_allstar_teams(uint32_t league_id)
{
    char league_name[128] = {0};
    kbo_hub_copy_league_display_name_fast(league_id, league_name, sizeof(league_name));
    return kbo_ascii_contains_ignore_case(league_name, "Tournament")
        || kbo_ascii_contains_ignore_case(league_name, "Classic")
        || kbo_ascii_contains_ignore_case(league_name, "Cup")
        || kbo_ascii_contains_ignore_case(league_name, "Qualifier")
        || kbo_ascii_contains_ignore_case(league_name, "World Baseball")
        || kbo_ascii_contains_ignore_case(league_name, "Asian Games");
}

int kbo_hub_team_name_is_allstar(const char* name)
{
    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    return kbo_ascii_contains_ignore_case(name, "All-Star")
        || kbo_ascii_contains_ignore_case(name, "All Star")
        || kbo_ascii_contains_ignore_case(name, "Allstars")
        || kbo_ascii_contains_ignore_case(name, "All-Stars")
        || kbo_ascii_contains_ignore_case(name, "All Stars")
        || kbo_ascii_contains_ignore_case(name, "Futures Stars")
        || kbo_ascii_contains_ignore_case(name, "Future Stars")
        || strstr(name, "\xec\x98\xac\xec\x8a\xa4\xed\x83\x80") != NULL
        || strstr(name, "\xec\x98\xac\x20\xec\x8a\xa4\xed\x83\x80") != NULL
        || strstr(name, "\xed\x93\xa8\xec\xb2\x98\xec\x8a\xa4") != NULL
        || strstr(name, "\xed\x93\xa8\xec\xb2\x98") != NULL;
}

int kbo_hub_team_hidden_from_dropdown(uint8_t* team)
{
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (kbo_hub_league_keeps_allstar_teams(league_id)) {
        return 0;
    }

    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    char display_name[128] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, display_name, sizeof(display_name), NULL);
    if (kbo_hub_team_name_is_allstar(display_name)) {
        return 1;
    }

    static const uint32_t string_offsets[] = OOTP27_KBO_TEAM_SEARCH_STRING_OFFSETS;
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        char text[96] = {0};
        if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))
                && kbo_hub_team_name_is_allstar(text)) {
            return 1;
        }
    }

    return 0;
}

void kbo_hub_ensure_valid_selection(void)
{
    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        g_kbo_hub_selected_league_id = 0;
        g_kbo_hub_selected_team_id   = 0;
        return;
    }

    int has_selected_team   = 0;
    int has_selected_league = 0;
    int has_kbo_league      = 0;
    uint32_t kbo_league_id  = kbo_resolve_kbo_league_id();
    uint32_t first_league_id = 0;
    uint32_t first_team_id   = 0;
    uint32_t first_team_in_selected_league = 0;
    uint32_t first_team_in_kbo_league      = 0;

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (kbo_hub_team_hidden_from_dropdown(team)) {
            continue;
        }
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        uint32_t team_id   = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (league_id == 0 || team_id == 0) {
            continue;
        }
        if (first_league_id == 0) { first_league_id = league_id; }
        if (first_team_id   == 0) { first_team_id   = team_id;   }
        if (kbo_league_id != 0u && league_id == kbo_league_id) {
            has_kbo_league = 1;
            if (first_team_in_kbo_league == 0) { first_team_in_kbo_league = team_id; }
        }
        if (league_id == g_kbo_hub_selected_league_id) {
            has_selected_league = 1;
            if (first_team_in_selected_league == 0) { first_team_in_selected_league = team_id; }
            if (team_id == g_kbo_hub_selected_team_id) { has_selected_team = 1; }
        }
    }

    if (!has_selected_league) {
        g_kbo_hub_selected_league_id = has_kbo_league ? kbo_league_id : first_league_id;
        g_kbo_hub_selected_team_id   = 0;
        first_team_in_selected_league = has_kbo_league ? first_team_in_kbo_league : 0;
        for (int32_t i = 0; i < team_count; i++) {
            if (first_team_in_selected_league != 0) {
                break;
            }
            uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
                continue;
            }
            uint8_t* team = (uint8_t*)team_ptr;
            if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
                continue;
            }
            if (kbo_hub_team_hidden_from_dropdown(team)) {
                continue;
            }
            if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) == g_kbo_hub_selected_league_id) {
                first_team_in_selected_league = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
                break;
            }
        }
    }

    if (!has_selected_team) {
        g_kbo_hub_selected_team_id = first_team_in_selected_league != 0
            ? first_team_in_selected_league : first_team_id;
    }

    if (g_kbo_hub_selected_settings_subview < 0
            || g_kbo_hub_selected_settings_subview >= KBO_HUB_SETTINGS_SUBVIEW_COUNT) {
        g_kbo_hub_selected_settings_subview = KBO_HUB_SETTINGS_SUBVIEW_LEAGUE;
    }
}

POINT kbo_hub_dropdown_anchor_point(HWND hwnd, const RECT* rect)
{
    POINT pt = {0, 0};
    if (hwnd != NULL
            && rect != NULL
            && rect->right > rect->left
            && rect->bottom > rect->top) {
        pt.x = rect->left;
        pt.y = rect->bottom;
        ClientToScreen(hwnd, &pt);
        return pt;
    }

    if (GetCursorPos(&pt)) {
        return pt;
    }

    RECT window_rect = {0, 0, 0, 0};
    if (hwnd != NULL && GetWindowRect(hwnd, &window_rect)) {
        pt.x = window_rect.left + 24;
        pt.y = window_rect.top + 64;
    }
    return pt;
}

void kbo_hub_show_league_dropdown(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return;
    }

    uint32_t leagues[KBO_HUB_DROPDOWN_MAX_LEAGUES] = {0};
    int league_count = 0;
    for (int32_t i = 0; i < team_count && league_count < KBO_HUB_DROPDOWN_MAX_LEAGUES; i++) {
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

    HMENU menu = CreatePopupMenu();
    if (menu == NULL) {
        return;
    }

    for (int i = 0; i < league_count; i++) {
        char label[128] = {0};
        WCHAR wide_label[160] = {0};
        kbo_hub_copy_league_display_name_fast(leagues[i], label, sizeof(label));
        kbo_utf8_to_wide(label, wide_label, (int)(sizeof(wide_label) / sizeof(wide_label[0])));
        UINT flags = MF_STRING;
        if (leagues[i] == g_kbo_hub_selected_league_id) { flags |= MF_CHECKED; }
        AppendMenuW(menu, flags, KBO_HUB_LEAGUE_MENU_COMMAND_BASE + (UINT)i, wide_label);
    }

    POINT pt = kbo_hub_dropdown_anchor_point(hwnd, &g_kbo_hub_league_dropdown_rect);
    SetForegroundWindow(hwnd);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    if (command >= KBO_HUB_LEAGUE_MENU_COMMAND_BASE
            && command < KBO_HUB_LEAGUE_MENU_COMMAND_BASE + (UINT)league_count) {
        g_kbo_hub_selected_league_id = leagues[command - KBO_HUB_LEAGUE_MENU_COMMAND_BASE];
        g_kbo_hub_selected_team_id   = 0;
        kbo_hub_ensure_valid_selection();
        kbo_refresh_hotkey_window();
        InvalidateRect(hwnd, NULL, TRUE);
    }

    DestroyMenu(menu);
    PostMessageA(hwnd, WM_NULL, 0, 0);
}

void kbo_hub_show_team_dropdown(HWND hwnd)
{
    if (hwnd == NULL) {
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

    uint32_t teams[KBO_HUB_DROPDOWN_MAX_TEAMS] = {0};
    int filtered_count = 0;
    for (int32_t i = 0; i < team_count && filtered_count < KBO_HUB_DROPDOWN_MAX_TEAMS; i++) {
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

    HMENU menu = CreatePopupMenu();
    if (menu == NULL) {
        return;
    }

    for (int i = 0; i < filtered_count; i++) {
        char label[128] = {0};
        WCHAR wide_label[160] = {0};
        kbo_hub_copy_team_display_name_by_id(teams[i], label, sizeof(label), NULL);
        kbo_utf8_to_wide(label, wide_label, (int)(sizeof(wide_label) / sizeof(wide_label[0])));
        UINT flags = MF_STRING;
        if (teams[i] == g_kbo_hub_selected_team_id) { flags |= MF_CHECKED; }
        AppendMenuW(menu, flags, KBO_HUB_TEAM_MENU_COMMAND_BASE + (UINT)i, wide_label);
    }

    POINT pt = kbo_hub_dropdown_anchor_point(hwnd, &g_kbo_hub_team_dropdown_rect);
    SetForegroundWindow(hwnd);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    if (command >= KBO_HUB_TEAM_MENU_COMMAND_BASE
            && command < KBO_HUB_TEAM_MENU_COMMAND_BASE + (UINT)filtered_count) {
        g_kbo_hub_selected_team_id = teams[command - KBO_HUB_TEAM_MENU_COMMAND_BASE];
        kbo_refresh_hotkey_window();
        InvalidateRect(hwnd, NULL, TRUE);
    }

    DestroyMenu(menu);
    PostMessageA(hwnd, WM_NULL, 0, 0);
}

