#include "../internal/ui_military_view_internal.h"

static LONG kbo_military_results_candidate_count(void)
{
    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    return count;
}

static void kbo_military_results_add_year(uint16_t* years, int* year_count, uint32_t year)
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

static void kbo_military_results_sort_years(uint16_t* years, int year_count)
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

static int kbo_military_results_live_selected_exists(uint32_t year, uint32_t player_id)
{
    if (year == 0u || player_id == 0u) {
        return 0;
    }
    LONG count = kbo_military_results_candidate_count();
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == player_id
                && candidate->selected != 0u
                && (uint32_t)candidate->entry_year == year) {
            return 1;
        }
    }
    return 0;
}

static uint32_t kbo_military_results_announcement_date_for_year(
    const KboMilitarySelectionResultEntry* history,
    int history_count,
    uint32_t year)
{
    if (history == NULL || history_count <= 0 || year == 0u) {
        return 0u;
    }
    for (int i = 0; i < history_count; i++) {
        if (history[i].year == year && history[i].announcement_date != 0u) {
            return history[i].announcement_date;
        }
    }
    return 0u;
}

static int kbo_military_results_count_for_year(
    const KboMilitarySelectionResultEntry* history,
    int history_count,
    uint32_t year)
{
    if (year == 0u) {
        return 0;
    }
    int selected = 0;
    LONG count = kbo_military_results_candidate_count();
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id != 0u
                && candidate->selected != 0u
                && (uint32_t)candidate->entry_year == year) {
            selected++;
        }
    }
    if (history != NULL && history_count > 0) {
        for (int i = 0; i < history_count; i++) {
            if (history[i].year == year
                    && history[i].player_id != 0u
                    && !kbo_military_results_live_selected_exists(year, history[i].player_id)) {
                selected++;
            }
        }
    }
    return selected;
}

static uint32_t kbo_military_results_live_service_team_id(uint8_t* player, uint32_t sang_id, uint32_t kpb_id)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0u;
    }
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    if (sang_id != 0u && (current_team_id == sang_id || loan_team_id == sang_id)) {
        return sang_id;
    }
    if (kpb_id != 0u && (current_team_id == kpb_id || loan_team_id == kpb_id)) {
        return kpb_id;
    }
    return 0u;
}

static void kbo_webview_append_military_result_row(
    KboWindowTextBuffer* buffer,
    uint32_t player_id,
    uintptr_t player_ptr,
    uint32_t original_team_id,
    uint32_t service_team_id,
    uint32_t announcement_date,
    uint32_t return_date,
    uint8_t fallback_position_group,
    uint8_t fallback_position_role,
    const char* service_fallback)
{
    if (buffer == NULL || player_id == 0u) {
        return;
    }

    if (!kbo_player_pointer_plausible(player_ptr)) {
        player_ptr = (uintptr_t)kbo_military_find_player_by_id(player_id);
    }

    char player_name[96] = {0};
    const char* position_label = kbo_webview_position_label_from_values(
        fallback_position_group,
        fallback_position_role);
    if (kbo_player_pointer_plausible(player_ptr)) {
        uint8_t* player = (uint8_t*)player_ptr;
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        position_label = kbo_webview_player_position_label(player, fallback_position_group);
        if (return_date == 0u) {
            return_date = kbo_military_effective_return_yyyymmdd(player);
        }
    } else {
        snprintf(player_name, sizeof(player_name), "#%u", player_id);
    }

    char original_team_name[64] = {0};
    kbo_hub_copy_team_display_name_by_id(original_team_id, original_team_name, sizeof(original_team_name), NULL);
    if (original_team_name[0] == '\0') {
        snprintf(original_team_name, sizeof(original_team_name), "-");
    }

    char service_team_name[64] = {0};
    kbo_hub_copy_team_display_name_by_id(
        service_team_id,
        service_team_name,
        sizeof(service_team_name),
        service_fallback != NULL ? service_fallback : "상무");
    if (service_team_name[0] == '\0') {
        snprintf(service_team_name, sizeof(service_team_name), "%s", service_fallback != NULL ? service_fallback : "상무");
    }

    char announcement_text[16] = "-";
    char return_text[16] = "-";
    kbo_military_format_yyyymmdd(announcement_date, announcement_text, sizeof(announcement_text));
    kbo_military_format_yyyymmdd(return_date, return_text, sizeof(return_text));

    kbo_window_text_appendf(buffer, "<tr><td class='roPo'>%s</td>", position_label);
    kbo_webview_append_player_name_cell(
        buffer,
        player_name[0] != '\0' ? player_name : "알 수 없는 선수",
        player_id);
    kbo_window_text_appendf(buffer, "<td class='roClub'>");
    kbo_html_append_escaped(buffer, original_team_name);
    kbo_window_text_appendf(buffer, "</td><td class='roLeague'>");
    kbo_html_append_escaped(buffer, service_team_name);
    kbo_window_text_appendf(buffer, "</td><td class='roDate'>%s</td><td class='roReturn'>%s</td><td class='roResult'>합격</td></tr>",
        announcement_text,
        return_text);
}

void kbo_webview_append_military_results_view(KboWindowTextBuffer* buffer, uint32_t* selected_results_year)
{
    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    char sang_name[64] = {0};
    char kpb_name[64]  = {0};
    kbo_hub_copy_team_display_name_from_ptr(sang, sang_name, sizeof(sang_name), "상무");
    kbo_hub_copy_team_display_name_from_ptr(kpb,  kpb_name,  sizeof(kpb_name),  "경찰 야구단");

    KboMilitarySelectionResultEntry* history = (KboMilitarySelectionResultEntry*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS * sizeof(KboMilitarySelectionResultEntry));
    int history_count = 0;
    if (history != NULL) {
        history_count = kbo_load_military_selection_result_history(
            history,
            OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS,
            "hotkey_window");
    }

    uint16_t years[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS] = {0};
    int year_count = 0;
    LONG count = kbo_military_results_candidate_count();
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id != 0u && candidate->selected != 0u) {
            kbo_military_results_add_year(years, &year_count, candidate->entry_year);
        }
    }
    for (int i = 0; i < history_count; i++) {
        if (history[i].player_id != 0u) {
            kbo_military_results_add_year(years, &year_count, history[i].year);
        }
    }
    kbo_military_results_sort_years(years, year_count);

    uint32_t selected_year = selected_results_year != NULL ? *selected_results_year : 0u;
    int selected_year_found = 0;
    for (int y = 0; y < year_count; y++) {
        if ((uint32_t)years[y] == selected_year) {
            selected_year_found = 1;
            break;
        }
    }
    if (year_count > 0 && !selected_year_found) {
        selected_year = (uint32_t)years[0];
        if (selected_results_year != NULL) { *selected_results_year = selected_year; }
    } else if (year_count == 0 && selected_year == 0u) {
        uint32_t current_year = 0u;
        if (kbo_current_year_relaxed(&current_year) && current_year != 0u) {
            selected_year = current_year;
            if (selected_results_year != NULL) { *selected_results_year = selected_year; }
        }
    }

    int selected_in_year = kbo_military_results_count_for_year(history, history_count, selected_year);
    uint32_t announcement_date = kbo_military_results_announcement_date_for_year(
        history,
        history_count,
        selected_year);

    char announcement_text[16] = "-";
    kbo_military_format_yyyymmdd(announcement_date, announcement_text, sizeof(announcement_text));

    char summary_text[192] = {0};
    if (selected_year != 0u && announcement_date != 0u) {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "보기: 역대 발표 결과 - %u - 발표일: %s - 합격: %d명",
            selected_year,
            announcement_text,
            selected_in_year);
    } else if (selected_year != 0u) {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "보기: 역대 발표 결과 - %u - 합격: %d명",
            selected_year,
            selected_in_year);
    } else {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "보기: 역대 발표 결과 - 합격: %d명",
            selected_in_year);
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_window_text_appendf(buffer, "<div class='rosterTopBar'><div class='rosterTopText'>");
    kbo_html_append_escaped(buffer, summary_text);

    kbo_window_text_appendf(
        buffer,
        "</div><div class='rosterTopControls'><span class='rosterTopLabel'>발표:</span>"
        "<select id='militaryResultsYearSelect' class='rosterYearSelect' "
        "onchange=\"location.href='kbo://military/results/year/'+this.value\">");
    if (year_count > 0) {
        for (int y = 0; y < year_count; y++) {
            uint32_t year = (uint32_t)years[y];
            int year_selected = year == selected_year;
            kbo_window_text_appendf(
                buffer,
                "<option value='%u'%s>%u</option>",
                year,
                year_selected ? " selected" : "",
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
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable resultRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>포지션</th><th class='roName' data-sort-type='text'>선수</th>"
        "<th class='roClub' data-sort-type='text'>원 소속</th><th class='roLeague' data-sort-type='text'>복무 구단</th>"
        "<th class='roDate' data-sort-type='date'>발표일</th><th class='roReturn' data-sort-type='date'>복귀일</th>"
        "<th class='roResult' data-sort-type='text'>결과</th>"
        "</tr></thead><tbody>");

    int rendered = 0;
    for (LONG i = 0; i < count && rendered < 500; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u
                || candidate->selected == 0u
                || (uint32_t)candidate->entry_year != selected_year) {
            continue;
        }

        uintptr_t player_ptr = candidate->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
        }

        uint32_t service_team_id = 0u;
        const char* service_fallback = sang_name[0] != '\0' ? sang_name : "상무";
        uint32_t return_date = 0u;
        uint8_t position_group = 0u;
        uint8_t position_role = 0u;
        if (kbo_player_pointer_plausible(player_ptr)) {
            uint8_t* player = (uint8_t*)player_ptr;
            service_team_id = kbo_military_results_live_service_team_id(player, sang_id, kpb_id);
            if (kpb_id != 0u && service_team_id == kpb_id && kpb_name[0] != '\0') {
                service_fallback = kpb_name;
            }
            return_date = kbo_military_effective_return_yyyymmdd(player);
            position_group = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
            position_role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
        }
        if (service_team_id == 0u) {
            service_team_id = sang_id;
        }

        kbo_webview_append_military_result_row(
            buffer,
            candidate->player_id,
            player_ptr,
            candidate->original_team_id,
            service_team_id,
            announcement_date,
            return_date,
            position_group,
            position_role,
            service_fallback);
        rendered++;
    }

    for (int i = 0; i < history_count && rendered < 500; i++) {
        KboMilitarySelectionResultEntry* entry = &history[i];
        if (entry->year != selected_year
                || entry->player_id == 0u
                || kbo_military_results_live_selected_exists(selected_year, entry->player_id)) {
            continue;
        }

        const char* service_fallback = sang_name[0] != '\0' ? sang_name : "상무";
        if (kpb_id != 0u && entry->service_team_id == kpb_id && kpb_name[0] != '\0') {
            service_fallback = kpb_name;
        }
        kbo_webview_append_military_result_row(
            buffer,
            entry->player_id,
            0u,
            entry->original_team_id,
            entry->service_team_id != 0u ? entry->service_team_id : sang_id,
            entry->announcement_date,
            entry->return_date,
            entry->position_group,
            entry->position_role,
            service_fallback);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='7' class='roEmptyMessage'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    if (history != NULL) {
        HeapFree(GetProcessHeap(), 0, history);
    }
}
