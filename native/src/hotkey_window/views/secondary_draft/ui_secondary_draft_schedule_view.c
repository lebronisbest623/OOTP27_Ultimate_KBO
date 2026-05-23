#include "ui_secondary_draft_view_internal.h"

#include <string.h>

static uint32_t kbo_secondary_draft_schedule_today(void)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_tick_latest_components(&year, &month, &day)
            || year == 0u
            || month == 0u
            || day == 0u) {
        return 0u;
    }
    return year * 10000u + month * 100u + day;
}

static const char* kbo_secondary_draft_schedule_status(
    int has_summary,
    int has_window,
    uint32_t today,
    const KboSecondaryDraftWindow* window)
{
    if (has_summary) {
        return "\xec\x99\x84\xeb\xa3\x8c";
    }
    if (!has_window || window == NULL) {
        return "\xec\x9d\xbc\xec\xa0\x95 \xec\x97\x86\xec\x9d\x8c";
    }
    if (today == 0u) {
        return "\xeb\x8c\x80\xea\xb8\xb0";
    }
    if (today != 0u && today < window->protection_open_yyyymmdd) {
        return "\xeb\x8c\x80\xea\xb8\xb0";
    }
    if (today != 0u && today <= window->protection_deadline_yyyymmdd) {
        return "\xec\xa0\x9c\xec\xb6\x9c \xec\xa4\x91";
    }
    if (today != 0u && today < window->draft_yyyymmdd) {
        return "\xeb\xaa\x85\xeb\x8b\xa8 \xec\x9e\xa0\xea\xb9\x80";
    }
    return "\xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8 \xea\xb0\x80\xeb\x8a\xa5";
}

void kbo_secondary_draft_ui_append_schedule_view(
    KboWindowTextBuffer* buffer,
    const uint32_t* seasons,
    int season_count,
    uint32_t selected_season)
{
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable secondaryDraftScheduleTable'><thead><tr>"
        "<th class='roDate' data-sort-type='number'>\xec\x8b\x9c\xec\xa6\x8c</th>"
        "<th class='roDate' data-sort-type='date'>\xec\xa0\x9c\xec\xb6\x9c \xec\x8b\x9c\xec\x9e\x91</th>"
        "<th class='roDate' data-sort-type='date'>\xec\xa0\x9c\xec\xb6\x9c \xeb\xa7\x88\xea\xb0\x90</th>"
        "<th class='roDate' data-sort-type='date'>\xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8\xec\x9d\xbc</th>"
        "<th class='roStatus' data-sort-type='text'>\xec\x83\x81\xed\x83\x9c</th>"
        "<th class='roPick' data-sort-type='number'>\xec\xa7\x80\xeb\xaa\x85</th>"
        "<th class='roPick' data-sort-type='number'>\xed\x9b\x84\xeb\xb3\xb4</th>"
        "<th class='roPick' data-sort-type='number'>\xeb\xb3\xb4\xed\x98\xb8</th>"
        "<th class='roCash' data-sort-type='number'>\xeb\xb3\xb4\xec\x83\x81\xea\xb8\x88</th>"
        "</tr></thead><tbody>");

    if (season_count <= 0) {
        kbo_window_text_appendf(
            buffer,
            "<tr><td class='roDate' data-sort-value='%u'>%u</td><td class='roDate'>-</td><td class='roDate'>-</td><td class='roDate'>\xed\x99\x80\xec\x88\x98\xed\x95\xb4 \xec\x98\xa4\xed\x94\x84\xec\x8b\x9c\xec\xa6\x8c</td><td class='roStatus'>\xeb\x8c\x80\xea\xb8\xb0</td><td class='roPick'>0</td><td class='roPick'>-</td><td class='roPick'>-</td><td class='roCash'>-</td></tr>",
            selected_season,
            selected_season);
    }
    uint32_t today = kbo_secondary_draft_schedule_today();
    for (int i = 0; i < season_count; i++) {
        KboSecondaryDraftRunSummary summary;
        memset(&summary, 0, sizeof(summary));
        KboSecondaryDraftWindow window;
        memset(&window, 0, sizeof(window));
        char open_text[16] = "-";
        char deadline_text[16] = "-";
        char draft_text[16] = "-";
        char cash_text[48] = "0";
        int has_summary = kbo_secondary_draft_load_run_summary(seasons[i], &summary);
        int has_window = kbo_secondary_draft_load_window(seasons[i], &window);
        if (has_window) {
            kbo_secondary_draft_ui_format_date(
                window.protection_open_yyyymmdd,
                open_text,
                sizeof(open_text));
            kbo_secondary_draft_ui_format_date(
                window.protection_deadline_yyyymmdd,
                deadline_text,
                sizeof(deadline_text));
            kbo_secondary_draft_ui_format_date(window.draft_yyyymmdd, draft_text, sizeof(draft_text));
        }
        if (has_summary) {
            if (!has_window) {
                kbo_secondary_draft_ui_format_date(summary.event_yyyymmdd, draft_text, sizeof(draft_text));
            }
            kbo_secondary_draft_ui_format_cash(summary.cash_total, cash_text, sizeof(cash_text));
        }
        kbo_window_text_appendf(
            buffer,
            "<tr><td class='roDate' data-sort-value='%u'>%u</td><td class='roDate'>",
            seasons[i],
            seasons[i]);
        kbo_html_append_escaped(buffer, open_text);
        kbo_window_text_appendf(buffer, "</td><td class='roDate'>");
        kbo_html_append_escaped(buffer, deadline_text);
        kbo_window_text_appendf(buffer, "</td><td class='roDate'>");
        kbo_html_append_escaped(buffer, draft_text);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roStatus'>%s</td><td class='roPick'>%d</td><td class='roPick'>%d</td><td class='roPick'>%d</td><td class='roCash'>",
            kbo_secondary_draft_schedule_status(has_summary, has_window, today, &window),
            has_summary ? summary.pick_count : 0,
            has_summary ? summary.candidate_count : 0,
            has_summary ? summary.protected_count : 0);
        kbo_html_append_escaped(buffer, cash_text);
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section>");
}
