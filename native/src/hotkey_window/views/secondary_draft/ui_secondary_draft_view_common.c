#include "ui_secondary_draft_view_internal.h"

#include <stdio.h>
#include <string.h>

void kbo_secondary_draft_ui_format_date(uint32_t yyyymmdd, char* out, size_t out_size)
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
        "%04u-%02u-%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

void kbo_secondary_draft_ui_format_cash(int64_t amount, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    char raw[32] = {0};
    snprintf(raw, sizeof(raw), "%lld", (long long)amount);

    const char* digits = raw;
    int negative = 0;
    if (digits[0] == '-') {
        negative = 1;
        digits++;
    }
    size_t digit_count = strlen(digits);
    if (digit_count == 0u) {
        snprintf(out, out_size, "0");
        return;
    }

    char formatted[48] = {0};
    size_t cursor = 0u;
    if (negative && cursor + 1u < sizeof(formatted)) {
        formatted[cursor++] = '-';
    }
    for (size_t i = 0u; i < digit_count && cursor + 1u < sizeof(formatted); i++) {
        if (i > 0u && ((digit_count - i) % 3u) == 0u && cursor + 1u < sizeof(formatted)) {
            formatted[cursor++] = ',';
        }
        formatted[cursor++] = digits[i];
    }
    formatted[cursor] = '\0';
    snprintf(out, out_size, "%s", formatted);
}

uint32_t kbo_secondary_draft_ui_default_season(void)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_tick_latest_components(&year, &month, &day) || year == 0u) {
        return 0u;
    }
    (void)month;
    (void)day;
    return (year % 2u) == 0u ? year + 1u : year;
}

void kbo_secondary_draft_ui_append_empty_row(
    KboWindowTextBuffer* buffer,
    int colspan,
    const char* text)
{
    kbo_window_text_appendf(buffer, "<tr><td class='roEmptyMessage' colspan='%d'>", colspan);
    kbo_html_append_escaped(buffer, text != NULL && text[0] != '\0' ? text : "-");
    kbo_window_text_appendf(buffer, "</td></tr>");
}

void kbo_secondary_draft_ui_append_top_bar(
    KboWindowTextBuffer* buffer,
    const KboSecondaryDraftRunSummary* summary,
    const uint32_t* seasons,
    int season_count,
    uint32_t selected_season,
    int row_count)
{
    char date_text[16] = "-";
    char cash_text[48] = "0";
    char summary_text[256] = {0};
    if (summary != NULL && summary->season != 0u) {
        kbo_secondary_draft_ui_format_date(summary->event_yyyymmdd, date_text, sizeof(date_text));
        kbo_secondary_draft_ui_format_cash(summary->cash_total, cash_text, sizeof(cash_text));
        snprintf(
            summary_text,
            sizeof(summary_text),
            "\x32\xec\xb0\xa8 \xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8 - %u\xec\x8b\x9c\xec\xa6\x8c - \xed\x96\x89\xec\x82\xac %s - \xec\xa7\x80\xeb\xaa\x85 %d\xeb\xaa\x85 - \xed\x9b\x84\xeb\xb3\xb4 %d\xeb\xaa\x85 - \xeb\xb3\xb4\xed\x98\xb8 %d\xeb\xaa\x85 - \xeb\xb3\xb4\xec\x83\x81\xea\xb8\x88 %s",
            summary->season,
            date_text,
            row_count,
            summary->candidate_count,
            summary->protected_count,
            cash_text);
    } else if (selected_season != 0u) {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "\x32\xec\xb0\xa8 \xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8 - %u\xec\x8b\x9c\xec\xa6\x8c - \xec\x95\x84\xec\xa7\x81 \xea\xb2\xb0\xea\xb3\xbc \xec\x97\x86\xec\x9d\x8c",
            selected_season);
    } else {
        snprintf(summary_text, sizeof(summary_text), "\x32\xec\xb0\xa8 \xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8 - \xec\x95\x84\xec\xa7\x81 \xea\xb2\xb0\xea\xb3\xbc \xec\x97\x86\xec\x9d\x8c");
    }

    kbo_window_text_appendf(buffer, "<div class='rosterTopBar'><div class='rosterTopText'>");
    kbo_html_append_escaped(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "</div><div class='rosterTopControls'><span class='rosterTopLabel'>\xec\x8b\x9c\xec\xa6\x8c:</span>"
        "<select id='secondaryDraftSeasonSelect' class='rosterYearSelect' "
        "onchange=\"location.href='kbo://secondary-draft/year/'+this.value\">");
    if (season_count > 0) {
        int selected_found = 0;
        for (int i = 0; i < season_count; i++) {
            uint32_t season = seasons[i];
            if (season == selected_season) {
                selected_found = 1;
            }
            kbo_window_text_appendf(
                buffer,
                "<option value='%u'%s>%u</option>",
                season,
                season == selected_season ? " selected" : "",
                season);
        }
        if (!selected_found && selected_season != 0u) {
            kbo_window_text_appendf(
                buffer,
                "<option value='%u' selected>%u</option>",
                selected_season,
                selected_season);
        }
    } else {
        if (selected_season != 0u) {
            kbo_window_text_appendf(
                buffer,
                "<option value='%u' selected>%u</option>",
                selected_season,
                selected_season);
        } else {
            kbo_window_text_appendf(buffer, "<option value='0' selected>-</option>");
        }
    }
    kbo_window_text_appendf(buffer, "</select></div></div>");
}

void kbo_secondary_draft_ui_append_action_bar(
    KboWindowTextBuffer* buffer,
    uint32_t season,
    uint32_t team_id,
    int submitted,
    int saved_count,
    int action_available,
    const char* status_text)
{
    kbo_window_text_appendf(buffer, "<div class='rosterTopBar'><div class='rosterTopText'>");
    kbo_window_text_appendf(
        buffer,
        "\xeb\xb3\xb4\xed\x98\xb8 \xeb\xaa\x85\xeb\x8b\xa8 %d/%d - %s",
        saved_count,
        KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT,
        status_text != NULL && status_text[0] != '\0'
            ? status_text
            : (submitted ? "\xec\xa0\x9c\xec\xb6\x9c\xeb\x90\xa8" : "\xed\x8e\xb8\xec\xa7\x91 \xea\xb0\x80\xeb\x8a\xa5"));
    kbo_window_text_appendf(buffer, "</div><div class='rosterTopControls'>");
    if (team_id != 0u && !submitted && action_available) {
        kbo_window_text_appendf(
            buffer,
            "<a class='rightsTextAction' href='kbo://secondary-draft/autofill/%u/%u'>\xec\x9e\x90\xeb\x8f\x99 \xec\xb1\x84\xec\x9a\xb0\xea\xb8\xb0</a>"
            "<a class='rightsTextAction' href='kbo://secondary-draft/submit/%u/%u'>\xec\xa0\x9c\xec\xb6\x9c</a>",
            season,
            team_id,
            season,
            team_id);
    } else {
        kbo_window_text_appendf(
            buffer,
            "<span class='rightsTextAction disabled'>\xec\x9e\x90\xeb\x8f\x99 \xec\xb1\x84\xec\x9a\xb0\xea\xb8\xb0</span>"
            "<span class='rightsTextAction disabled'>\xec\xa0\x9c\xec\xb6\x9c</span>");
    }
    kbo_window_text_appendf(buffer, "</div></div>");
}

void kbo_secondary_draft_ui_append_results_table(
    KboWindowTextBuffer* buffer,
    KboSecondaryDraftResultRow* rows,
    int row_count)
{
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable secondaryDraftTable'><thead><tr>"
        "<th class='roPick' data-sort-type='number'>\xec\x88\x9c\xeb\xb2\x88</th>"
        "<th class='roRound' data-sort-type='number'>\xeb\x9d\xbc\xec\x9a\xb4\xeb\x93\x9c</th>"
        "<th class='roName' data-sort-type='text'>\xec\x84\xa0\xec\x88\x98</th>"
        "<th class='roClub' data-sort-type='text'>\xec\x9b\x90\xec\x86\x8c\xec\x86\x8d</th>"
        "<th class='roTeam' data-sort-type='text'>\xec\xa7\x80\xeb\xaa\x85 \xea\xb5\xac\xeb\x8b\xa8</th>"
        "<th class='roCash' data-sort-type='number'>\xeb\xb3\xb4\xec\x83\x81\xea\xb8\x88</th>"
        "<th class='roStatus' data-sort-type='text'>\xec\x9d\xb4\xeb\x8f\x99</th>"
        "</tr></thead><tbody>");
    if (row_count <= 0) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 7, "\xec\x95\x84\xec\xa7\x81 \xec\xa7\x80\xeb\xaa\x85\xeb\x90\x9c \xec\x84\xa0\xec\x88\x98\xea\xb0\x80 \xec\x97\x86\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.");
    }
    for (int i = 0; i < row_count; i++) {
        KboSecondaryDraftResultRow* row = &rows[i];
        char cash_text[48] = {0};
        kbo_secondary_draft_ui_format_cash(row->cash_amount, cash_text, sizeof(cash_text));
        kbo_window_text_appendf(
            buffer,
            "<tr><td class='roPick' data-sort-value='%u'>%u</td><td class='roRound' data-sort-value='%u'>%u</td>",
            row->pick_no,
            row->pick_no,
            row->round,
            row->round);
        kbo_webview_append_player_name_cell(buffer, row->player_name[0] != '\0' ? row->player_name : "-", row->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, row->from_team_name[0] != '\0' ? row->from_team_name : "-");
        kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
        kbo_html_append_escaped(buffer, row->to_team_name[0] != '\0' ? row->to_team_name : "-");
        kbo_window_text_appendf(buffer, "</td><td class='roCash' data-sort-value='%u'>", row->cash_amount);
        kbo_html_append_escaped(buffer, cash_text);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roStatus'>%s</td></tr>",
            row->moved ? "\xec\x99\x84\xeb\xa3\x8c" : "\xed\x99\x95\xec\x9d\xb8");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section>");
}
