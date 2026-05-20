#include "../ui_asian_games_view_internal.h"

static void kbo_webview_asian_games_roster_add_year(uint16_t* years, int* year_count, uint32_t year)
{
    if (years == NULL || year_count == NULL || year < 1982u || year > 2300u) {
        return;
    }
    for (int i = 0; i < *year_count; i++) {
        if ((uint32_t)years[i] == year) {
            return;
        }
    }
    if (*year_count < OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        years[*year_count] = (uint16_t)year;
        (*year_count)++;
    }
}

static void kbo_webview_asian_games_roster_sort_years(uint16_t* years, int year_count)
{
    if (years == NULL || year_count <= 1) {
        return;
    }
    for (int left = 0; left < year_count; left++) {
        for (int right = left + 1; right < year_count; right++) {
            if (years[right] > years[left]) {
                uint16_t tmp = years[left];
                years[left] = years[right];
                years[right] = tmp;
            }
        }
    }
}

static LONG kbo_webview_asian_games_current_roster_count(void)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0) {
        kbo_load_asian_games_roster_csv("hotkey_ui");
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    return roster_count;
}

static int kbo_webview_asian_games_history_count_for_year(
    const KboAsianGamesRosterHistoryEntry* history,
    int history_count,
    uint32_t year)
{
    if (history == NULL || history_count <= 0 || year == 0u) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < history_count; i++) {
        if (history[i].year == year && history[i].entry.player_id != 0u) {
            count++;
        }
    }
    return count;
}

static int kbo_webview_asian_games_current_wildcard_count(LONG roster_count)
{
    int wildcards = 0;
    for (LONG i = 0; i < roster_count; i++) {
        if (g_kbo_asian_games_roster[i].wildcard != 0u) {
            wildcards++;
        }
    }
    return wildcards;
}

static int kbo_webview_asian_games_history_wildcard_count(
    const KboAsianGamesRosterHistoryEntry* history,
    int history_count,
    uint32_t year)
{
    if (history == NULL || history_count <= 0 || year == 0u) {
        return 0;
    }
    int wildcards = 0;
    for (int i = 0; i < history_count; i++) {
        if (history[i].year == year && history[i].entry.player_id != 0u && history[i].entry.wildcard != 0u) {
            wildcards++;
        }
    }
    return wildcards;
}

static uint8_t kbo_webview_asian_games_history_result_for_year(
    const KboAsianGamesRosterHistoryEntry* history,
    int history_count,
    uint32_t year)
{
    if (history == NULL || history_count <= 0 || year == 0u) {
        return KBO_ASIAN_GAMES_RESULT_UNKNOWN;
    }
    for (int i = 0; i < history_count; i++) {
        if (history[i].year == year
                && (history[i].tournament_result == KBO_ASIAN_GAMES_RESULT_GOLD
                    || history[i].tournament_result == KBO_ASIAN_GAMES_RESULT_NO_GOLD)) {
            return history[i].tournament_result;
        }
    }
    return KBO_ASIAN_GAMES_RESULT_UNKNOWN;
}

static const char* kbo_webview_asian_games_roster_status_label(const KboAsianGamesRosterEntry* entry)
{
    if (entry == NULL) {
        return "-";
    }
    if (entry->exempted != 0u) {
        return "면제";
    }
    if (entry->returned != 0u) {
        return "복귀";
    }
    if (entry->departed != 0u) {
        return "출국";
    }
    return "선발";
}

static void kbo_webview_append_asian_games_roster_row(
    KboWindowTextBuffer* buffer,
    const KboAsianGamesRosterEntry* entry)
{
    if (buffer == NULL || entry == NULL || entry->player_id == 0u) {
        return;
    }

    uintptr_t player_ptr = entry->player_ptr;
    if (!kbo_player_pointer_plausible(player_ptr)) {
        player_ptr = (uintptr_t)kbo_find_player_by_id(entry->player_id, NULL, NULL);
    }

    char player_name[96] = {0};
    if (kbo_player_pointer_plausible(player_ptr)) {
        kbo_hub_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
    } else {
        snprintf(player_name, sizeof(player_name), "#%u", entry->player_id);
    }

    char uniform_number[8] = {0};
    kbo_webview_copy_player_uniform_number(entry->player_id, uniform_number, sizeof(uniform_number));

    char team_abbrev[16] = {0};
    kbo_hub_copy_team_abbrev_by_id(entry->original_team_id, team_abbrev, sizeof(team_abbrev), NULL);
    if (team_abbrev[0] == '\0') {
        snprintf(team_abbrev, sizeof(team_abbrev), "-");
    }

    kbo_window_text_appendf(
        buffer,
        "<tr><td class='roPo'>%s</td><td class='roNum'>",
        kbo_webview_player_position_label(
            kbo_player_pointer_plausible(player_ptr) ? (uint8_t*)player_ptr : NULL,
            entry->role));
    kbo_html_append_escaped(buffer, uniform_number);
    kbo_window_text_appendf(buffer, "</td>");
    kbo_webview_append_player_name_cell(buffer, player_name, entry->player_id);
    kbo_window_text_appendf(
        buffer,
        "<td class='roLeague'>대한민국 대표팀</td><td class='roAge'>%u</td>",
        (uint32_t)entry->age);
    kbo_webview_append_roster_nation_cell(buffer, OOTP27_KBO_KOREA_NATION_ID, kbo_hub_nation_flag_asset_path);
    kbo_window_text_appendf(buffer, "<td class='roTeam'>");
    kbo_html_append_escaped(buffer, team_abbrev);
    kbo_window_text_appendf(
        buffer,
        "</td><td class='roClub'>%s</td><td class='roStatus'>%s</td></tr>",
        entry->wildcard ? "예" : "아니오",
        kbo_webview_asian_games_roster_status_label(entry));
}

static void kbo_webview_append_asian_games_roster_view(
    KboWindowTextBuffer* buffer,
    uint32_t* selected_roster_year)
{
    if (buffer == NULL) {
        return;
    }

    kbo_clear_asian_games_roster_if_save_changed("hotkey_ui");
    LONG current_roster_count = kbo_webview_asian_games_current_roster_count();

    KboAsianGamesRosterHistoryEntry* history = (KboAsianGamesRosterHistoryEntry*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS * sizeof(KboAsianGamesRosterHistoryEntry));
    int history_count = 0;
    if (history != NULL) {
        history_count = kbo_load_asian_games_roster_history(
            history,
            OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS,
            "hotkey_ui");
    }

    uint16_t years[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS] = {0};
    int year_count = 0;
    if (current_roster_count > 0 && g_kbo_asian_games_roster_year != 0u) {
        kbo_webview_asian_games_roster_add_year(years, &year_count, g_kbo_asian_games_roster_year);
    }
    for (int i = 0; i < history_count; i++) {
        if (history[i].entry.player_id != 0u) {
            kbo_webview_asian_games_roster_add_year(years, &year_count, history[i].year);
        }
    }
    kbo_webview_asian_games_roster_sort_years(years, year_count);

    uint32_t selected_year = selected_roster_year != NULL ? *selected_roster_year : 0u;
    int selected_year_found = 0;
    for (int i = 0; i < year_count; i++) {
        if ((uint32_t)years[i] == selected_year) {
            selected_year_found = 1;
            break;
        }
    }
    if (year_count > 0 && !selected_year_found) {
        selected_year = (uint32_t)years[0];
        if (selected_roster_year != NULL) {
            *selected_roster_year = selected_year;
        }
    } else if (year_count == 0 && selected_year == 0u) {
        uint32_t current_year = 0u;
        if (kbo_current_year_relaxed(&current_year) && current_year != 0u) {
            selected_year = current_year;
            if (selected_roster_year != NULL) {
                *selected_roster_year = selected_year;
            }
        }
    }

    int use_current_roster = current_roster_count > 0 && g_kbo_asian_games_roster_year == selected_year;
    int selected_count = use_current_roster
        ? (int)current_roster_count
        : kbo_webview_asian_games_history_count_for_year(history, history_count, selected_year);
    int wildcard_count = use_current_roster
        ? kbo_webview_asian_games_current_wildcard_count(current_roster_count)
        : kbo_webview_asian_games_history_wildcard_count(history, history_count, selected_year);
    uint8_t result = use_current_roster
        ? g_kbo_asian_games_result
        : kbo_webview_asian_games_history_result_for_year(history, history_count, selected_year);
    const char* result_label = kbo_webview_asian_games_history_result_label(result);

    char summary_text[192] = {0};
    if (selected_year != 0u && result_label[0] != '\0') {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "보기: 역대 로스터 - %u - %d명 - 와일드카드: %d - 최종: %s",
            selected_year,
            selected_count,
            wildcard_count,
            result_label);
    } else if (selected_year != 0u) {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "보기: 역대 로스터 - %u - %d명 - 와일드카드: %d",
            selected_year,
            selected_count,
            wildcard_count);
    } else {
        snprintf(summary_text, sizeof(summary_text), "보기: 역대 로스터 - %d명", selected_count);
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_window_text_appendf(buffer, "<div class='rosterTopBar'><div class='rosterTopText'>");
    kbo_html_append_escaped(buffer, summary_text);

    kbo_window_text_appendf(
        buffer,
        "</div><div class='rosterTopControls'><span class='rosterTopLabel'>로스터:</span>"
        "<select id='asianGamesRosterYearSelect' class='rosterYearSelect' "
        "onchange=\"location.href='kbo://agames/roster/year/'+this.value\">");
    if (year_count > 0) {
        for (int i = 0; i < year_count; i++) {
            uint32_t year = (uint32_t)years[i];
            kbo_window_text_appendf(
                buffer,
                "<option value='%u'%s>%u</option>",
                year,
                year == selected_year ? " selected" : "",
                year);
        }
    } else {
        if (selected_year != 0u) {
            kbo_window_text_appendf(
                buffer,
                "<option value='%u' selected>%u</option>",
                selected_year,
                selected_year);
        } else {
            kbo_window_text_appendf(buffer, "<option value='0' selected>-</option>");
        }
    }
    kbo_window_text_appendf(buffer, "</select></div></div>");

    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable agRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>포지션</th><th class='roNum' data-sort-type='number'></th>"
        "<th class='roName' data-sort-type='text'>선수</th><th class='roLeague' data-sort-type='text'>대표팀</th>"
        "<th class='roAge' data-sort-type='number'>나이</th><th class='roNat' data-sort-type='text'>국적*</th>"
        "<th class='roTeam' data-sort-type='text'>구단</th><th class='roClub' data-sort-type='text'>와일드카드</th>"
        "<th class='roStatus' data-sort-type='text'>상태</th></tr></thead><tbody>");

    int rendered = 0;
    if (use_current_roster) {
        for (LONG i = 0; i < current_roster_count; i++) {
            kbo_webview_append_asian_games_roster_row(buffer, &g_kbo_asian_games_roster[i]);
            rendered++;
        }
    } else if (history != NULL) {
        for (int i = 0; i < history_count && rendered < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
            if (history[i].year != selected_year || history[i].entry.player_id == 0u) {
                continue;
            }
            kbo_webview_append_asian_games_roster_row(buffer, &history[i].entry);
            rendered++;
        }
    }

    if (rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='9' class='roEmptyMessage'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");

    if (history != NULL) {
        HeapFree(GetProcessHeap(), 0, history);
    }
}

void kbo_webview_append_asian_games_view(
    KboWindowTextBuffer* buffer,
    int selected_agames_subview,
    uint32_t* selected_roster_year)
{
    if (selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS) {
        kbo_webview_append_asian_games_tournaments_view(buffer);
        return;
    }
    if (selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_SCHEDULE) {
        kbo_webview_append_asian_games_schedule_view(buffer);
        return;
    }

    kbo_webview_append_asian_games_roster_view(buffer, selected_roster_year);
}
