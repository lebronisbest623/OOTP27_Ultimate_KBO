#include "ui_secondary_draft_view_internal.h"

#include <string.h>

void kbo_secondary_draft_ui_append_draft_view(
    KboWindowTextBuffer* buffer,
    uint32_t season,
    uint32_t drafting_team_id,
    KboSecondaryDraftResultRow* result_rows,
    int result_count)
{
    kbo_secondary_draft_ui_append_results_table(buffer, result_rows, result_count);

    KboSecondaryDraftCandidateRow pool[KBO_SECONDARY_DRAFT_UI_MAX_CANDIDATES];
    int pool_count = kbo_secondary_draft_collect_draft_pool_rows(
        season,
        drafting_team_id,
        pool,
        KBO_SECONDARY_DRAFT_UI_MAX_CANDIDATES);
    KboSecondaryDraftWindow window;
    memset(&window, 0, sizeof(window));
    int has_window = kbo_secondary_draft_load_window(season, &window);
    char draft_date_text[16] = "-";
    if (has_window) {
        kbo_secondary_draft_ui_format_date(window.draft_yyyymmdd, draft_date_text, sizeof(draft_date_text));
    }
    int draft_open = kbo_secondary_draft_draft_window_open(season);
    const char* draft_status = has_window
        ? (draft_open
            ? "\xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8 \xea\xb0\x80\xeb\x8a\xa5"
            : "\xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8 \xec\xa0\x84")
        : "\xec\x9d\xbc\xec\xa0\x95 \xec\x97\x86\xec\x9d\x8c";
    int team_action_available = drafting_team_id != 0u
        && kbo_hub_ui_team_action_available(drafting_team_id, "hub_secondary_draft_pick_render");
    const char* action_status = drafting_team_id == 0u
        ? "\xea\xb5\xac\xeb\x8b\xa8 \xec\x84\xa0\xed\x83\x9d \xed\x95\x84\xec\x9a\x94"
        : (team_action_available
            ? draft_status
            : "\xea\xb5\xac\xeb\x8b\xa8 \xea\xb6\x8c\xed\x95\x9c \xec\x97\x86\xec\x9d\x8c");
    int action_available = team_action_available && draft_open;
    kbo_window_text_appendf(
        buffer,
        "<div class='rosterTopBar'><div class='rosterTopText'>"
        "\xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8\xec\x9d\xbc %s | "
        "\xec\xa7\x80\xeb\xaa\x85 \xec\x99\x84\xeb\xa3\x8c %d | \xec\xa7\x80\xeb\xaa\x85 \xea\xb0\x80\xeb\x8a\xa5 %d | %s"
        "</div></div>",
        draft_date_text,
        result_count,
        pool_count,
        action_available ? "\xec\xa7\x80\xeb\xaa\x85 \xea\xb0\x80\xeb\x8a\xa5" : action_status);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable secondaryDraftPoolTable'><thead><tr>"
        "<th class='roAction'>\xec\xa7\x80\xeb\xaa\x85</th>"
        "<th class='roName' data-sort-type='text'>\xec\x84\xa0\xec\x88\x98</th>"
        "<th class='roClub' data-sort-type='text'>\xec\x9b\x90\xec\x86\x8c\xec\x86\x8d</th>"
        "<th class='roAge' data-sort-type='number'>\xeb\x82\x98\xec\x9d\xb4</th>"
        "<th class='roStatus' data-sort-type='text'>\xec\x83\x81\xed\x83\x9c</th>"
        "</tr></thead><tbody>");
    if (drafting_team_id == 0u) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 5, "\xec\xa7\x80\xeb\xaa\x85\xed\x95\xa0 \xea\xb5\xac\xeb\x8b\xa8\xec\x9d\x84 \xeb\xa8\xbc\xec\xa0\x80 \xec\x84\xa0\xed\x83\x9d\xed\x95\x98\xec\x84\xb8\xec\x9a\x94.");
    } else if (pool_count <= 0) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 5, "\xec\xa7\x80\xeb\xaa\x85 \xea\xb0\x80\xeb\x8a\xa5\xed\x95\x9c \xec\x84\xa0\xec\x88\x98\xea\xb0\x80 \xec\x97\x86\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.");
    }
    for (int i = 0; i < pool_count; i++) {
        KboSecondaryDraftCandidateRow* row = &pool[i];
        kbo_window_text_appendf(buffer, "<tr><td class='roAction'><span class='rightsActions'>");
        if (action_available && row->eligible) {
            kbo_window_text_appendf(
                buffer,
                "<a class='rightsAction rightsAdd' title='\xec\x9d\xb4 \xec\x84\xa0\xec\x88\x98 \xec\xa7\x80\xeb\xaa\x85' href='kbo://secondary-draft/pick/%u/%u/%u'>Pick</a>",
                season,
                drafting_team_id,
                row->player_id);
        } else {
            kbo_window_text_appendf(buffer, "<span class='rightsAction rightsAdd disabled'>-</span>");
        }
        kbo_window_text_appendf(buffer, "</span></td>");
        kbo_webview_append_player_name_cell(buffer, row->player_name[0] != '\0' ? row->player_name : "-", row->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, row->team_name[0] != '\0' ? row->team_name : "-");
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roAge'>%u</td><td class='roStatus'>",
            row->age);
        kbo_html_append_escaped(buffer, row->eligible ? "\xec\xa7\x80\xeb\xaa\x85 \xea\xb0\x80\xeb\x8a\xa5" : "\xed\x99\x95\xec\x9d\xb8 \xed\x95\x84\xec\x9a\x94");
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section>");
}
