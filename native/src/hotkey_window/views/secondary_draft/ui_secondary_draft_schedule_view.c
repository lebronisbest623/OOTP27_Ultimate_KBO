#include "ui_secondary_draft_view_internal.h"

#include <stdio.h>
#include <string.h>

typedef enum KboSecondaryDraftScheduleState {
    KBO_SECONDARY_DRAFT_SCHEDULE_MISSING = 0, KBO_SECONDARY_DRAFT_SCHEDULE_WAITING,
    KBO_SECONDARY_DRAFT_SCHEDULE_PROTECTION_OPEN, KBO_SECONDARY_DRAFT_SCHEDULE_LOCKED,
    KBO_SECONDARY_DRAFT_SCHEDULE_DRAFT_READY, KBO_SECONDARY_DRAFT_SCHEDULE_DONE
} KboSecondaryDraftScheduleState;

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

static KboSecondaryDraftScheduleState kbo_secondary_draft_schedule_state(
    int has_summary, int has_window, uint32_t today, const KboSecondaryDraftWindow* window)
{
    if (has_summary) {
        return KBO_SECONDARY_DRAFT_SCHEDULE_DONE;
    }
    if (!has_window || window == NULL) {
        return KBO_SECONDARY_DRAFT_SCHEDULE_MISSING;
    }
    if (today == 0u || today < window->protection_open_yyyymmdd) {
        return KBO_SECONDARY_DRAFT_SCHEDULE_WAITING;
    }
    if (today <= window->protection_deadline_yyyymmdd) {
        return KBO_SECONDARY_DRAFT_SCHEDULE_PROTECTION_OPEN;
    }
    if (today < window->draft_yyyymmdd) {
        return KBO_SECONDARY_DRAFT_SCHEDULE_LOCKED;
    }
    return KBO_SECONDARY_DRAFT_SCHEDULE_DRAFT_READY;
}

static const char* kbo_secondary_draft_schedule_status_text(KboSecondaryDraftScheduleState state)
{
    switch (state) {
    case KBO_SECONDARY_DRAFT_SCHEDULE_DONE:
        return "\xec\x99\x84\xeb\xa3\x8c";
    case KBO_SECONDARY_DRAFT_SCHEDULE_PROTECTION_OPEN:
        return "\xec\xa0\x9c\xec\xb6\x9c \xec\xa4\x91";
    case KBO_SECONDARY_DRAFT_SCHEDULE_LOCKED:
        return "\xeb\xaa\x85\xeb\x8b\xa8 \xec\x9e\xa0\xea\xb9\x80";
    case KBO_SECONDARY_DRAFT_SCHEDULE_DRAFT_READY:
        return "\xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8 \xea\xb0\x80\xeb\x8a\xa5";
    case KBO_SECONDARY_DRAFT_SCHEDULE_WAITING:
        return "\xeb\x8c\x80\xea\xb8\xb0";
    default:
        return "\xec\x9d\xbc\xec\xa0\x95 \xec\x97\x86\xec\x9d\x8c";
    }
}

static const char* kbo_secondary_draft_schedule_state_class(KboSecondaryDraftScheduleState state)
{
    switch (state) {
    case KBO_SECONDARY_DRAFT_SCHEDULE_DONE:
        return "done";
    case KBO_SECONDARY_DRAFT_SCHEDULE_PROTECTION_OPEN:
        return "open";
    case KBO_SECONDARY_DRAFT_SCHEDULE_LOCKED:
        return "locked";
    case KBO_SECONDARY_DRAFT_SCHEDULE_DRAFT_READY:
        return "ready";
    case KBO_SECONDARY_DRAFT_SCHEDULE_WAITING:
        return "waiting";
    default:
        return "missing";
    }
}

static const char* kbo_secondary_draft_schedule_step_class(KboSecondaryDraftScheduleState state, int step_index)
{
    if (state == KBO_SECONDARY_DRAFT_SCHEDULE_DONE) {
        return "done";
    }
    if (state == KBO_SECONDARY_DRAFT_SCHEDULE_PROTECTION_OPEN) {
        return step_index == 0 ? "done" : (step_index == 1 ? "current" : "pending");
    }
    if (state == KBO_SECONDARY_DRAFT_SCHEDULE_LOCKED) {
        return step_index < 2 ? "done" : "current";
    }
    if (state == KBO_SECONDARY_DRAFT_SCHEDULE_DRAFT_READY) {
        return step_index < 2 ? "done" : "current";
    }
    return "pending";
}

static void kbo_secondary_draft_schedule_append_metric(
    KboWindowTextBuffer* buffer, const char* label, const char* value, const char* tone)
{
    kbo_window_text_appendf(
        buffer,
        "<div class='secondaryDraftScheduleMetric %s'><div class='secondaryDraftScheduleMetricLabel'>",
        tone != NULL ? tone : "");
    kbo_html_append_escaped(buffer, label != NULL ? label : "-");
    kbo_window_text_appendf(buffer, "</div><div class='secondaryDraftScheduleMetricValue'>");
    kbo_html_append_escaped(buffer, value != NULL && value[0] != '\0' ? value : "-");
    kbo_window_text_appendf(buffer, "</div></div>");
}

static void kbo_secondary_draft_schedule_append_step(
    KboWindowTextBuffer* buffer, const char* label, const char* date_text, const char* step_class)
{
    kbo_window_text_appendf(
        buffer,
        "<div class='secondaryDraftScheduleStep %s'>"
        "<div class='secondaryDraftScheduleDot'></div>"
        "<div class='secondaryDraftScheduleStepBody'>"
        "<div class='secondaryDraftScheduleStepLabel'>",
        step_class != NULL ? step_class : "pending");
    kbo_html_append_escaped(buffer, label != NULL ? label : "-");
    kbo_window_text_appendf(buffer, "</div><div class='secondaryDraftScheduleStepDate'>");
    kbo_html_append_escaped(buffer, date_text != NULL && date_text[0] != '\0' ? date_text : "-");
    kbo_window_text_appendf(buffer, "</div></div></div>");
}

static void kbo_secondary_draft_schedule_append_season_rail(
    KboWindowTextBuffer* buffer,
    const uint32_t* seasons,
    int season_count,
    uint32_t selected_season)
{
    kbo_window_text_appendf(
        buffer,
        "<div class='secondaryDraftSeasonRail'>"
        "<div class='secondaryDraftSeasonRailTitle'>\xec\x8b\x9c\xec\xa6\x8c \xeb\xaa\xa9\xeb\xa1\x9d</div>"
        "<div class='secondaryDraftSeasonChips'>");
    int selected_found = 0;
    for (int i = 0; i < season_count; i++) {
        if (seasons[i] == selected_season) {
            selected_found = 1;
        }
        kbo_window_text_appendf(
            buffer,
            "<a class='secondaryDraftSeasonChip %s' href='kbo://secondary-draft/year/%u'>%u</a>",
            seasons[i] == selected_season ? "active" : "",
            seasons[i],
            seasons[i]);
    }
    if (!selected_found && selected_season != 0u) {
        kbo_window_text_appendf(
            buffer,
            "<a class='secondaryDraftSeasonChip active' href='kbo://secondary-draft/year/%u'>%u</a>",
            selected_season,
            selected_season);
    }
    if (season_count <= 0 && selected_season == 0u) {
        kbo_window_text_appendf(
            buffer,
            "<span class='secondaryDraftSeasonChip disabled'>-</span>");
    }
    kbo_window_text_appendf(buffer, "</div></div>");
}

static void kbo_secondary_draft_schedule_append_focus(
    KboWindowTextBuffer* buffer,
    uint32_t season,
    uint32_t today)
{
    KboSecondaryDraftRunSummary summary;
    memset(&summary, 0, sizeof(summary));
    KboSecondaryDraftWindow window;
    memset(&window, 0, sizeof(window));
    int has_summary = season != 0u && kbo_secondary_draft_load_run_summary(season, &summary);
    int has_window = season != 0u && kbo_secondary_draft_load_window(season, &window);
    KboSecondaryDraftScheduleState state =
        kbo_secondary_draft_schedule_state(has_summary, has_window, today, &window);

    char open_text[16] = "-";
    char deadline_text[16] = "-";
    char draft_text[16] = "-";
    char protected_text[32] = {0};
    char pick_text[32] = "-";
    char candidate_text[32] = "-";
    char protected_count_text[32] = "-";
    char cash_text[64] = "-";
    snprintf(
        protected_text,
        sizeof(protected_text),
        "%d\xeb\xaa\x85",
        KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT);

    if (has_window) {
        kbo_secondary_draft_ui_format_date(window.protection_open_yyyymmdd, open_text, sizeof(open_text));
        kbo_secondary_draft_ui_format_date(
            window.protection_deadline_yyyymmdd,
            deadline_text,
            sizeof(deadline_text));
        kbo_secondary_draft_ui_format_date(window.draft_yyyymmdd, draft_text, sizeof(draft_text));
    }
    if (has_summary) {
        char raw_cash[48] = {0};
        if (!has_window) {
            kbo_secondary_draft_ui_format_date(summary.event_yyyymmdd, draft_text, sizeof(draft_text));
        }
        snprintf(pick_text, sizeof(pick_text), "%d\xeb\xaa\x85", summary.pick_count);
        snprintf(candidate_text, sizeof(candidate_text), "%d\xeb\xaa\x85", summary.candidate_count);
        snprintf(protected_count_text, sizeof(protected_count_text), "%d\xeb\xaa\x85", summary.protected_count);
        kbo_secondary_draft_ui_format_cash(summary.cash_total, raw_cash, sizeof(raw_cash));
        snprintf(cash_text, sizeof(cash_text), "$%s", raw_cash);
    }

    kbo_window_text_appendf(
        buffer,
        "<section class='secondaryDraftScheduleFocus'>"
        "<div class='secondaryDraftScheduleHead'>"
        "<div class='secondaryDraftScheduleTitleBlock'>"
        "<div class='secondaryDraftScheduleEyebrow'>\xec\x84\xa0\xed\x83\x9d\xeb\x90\x9c \xec\x8b\x9c\xec\xa6\x8c</div>"
        "<div class='secondaryDraftScheduleTitle'>%u \xec\x8b\x9c\xec\xa6\x8c</div>"
        "</div>"
        "<div class='secondaryDraftScheduleBadge %s'>",
        season,
        kbo_secondary_draft_schedule_state_class(state));
    kbo_html_append_escaped(buffer, kbo_secondary_draft_schedule_status_text(state));
    kbo_window_text_appendf(
        buffer,
        "</div></div><div class='secondaryDraftScheduleMetrics'>");
    kbo_secondary_draft_schedule_append_metric(buffer, "\xec\xa7\x84\xed\x96\x89 \xed\x98\x84\xed\x99\xa9", kbo_secondary_draft_schedule_status_text(state), "accent");
    kbo_secondary_draft_schedule_append_metric(buffer, "\xeb\xb3\xb4\xed\x98\xb8 \xed\x95\x9c\xeb\x8f\x84", protected_text, "");
    kbo_secondary_draft_schedule_append_metric(buffer, "\xec\xa7\x80\xeb\xaa\x85 \xec\x88\x98", pick_text, "");
    kbo_secondary_draft_schedule_append_metric(buffer, "\xed\x9b\x84\xeb\xb3\xb4", candidate_text, "");
    kbo_secondary_draft_schedule_append_metric(buffer, "\xeb\xb3\xb4\xed\x98\xb8", protected_count_text, "");
    kbo_secondary_draft_schedule_append_metric(buffer, "\xeb\xb3\xb4\xec\x83\x81\xea\xb8\x88", cash_text, "");
    kbo_window_text_appendf(buffer, "</div><div class='secondaryDraftScheduleTimeline'>");
    kbo_secondary_draft_schedule_append_step(buffer, "\xeb\xb3\xb4\xed\x98\xb8 \xeb\xaa\x85\xeb\x8b\xa8 \xec\xa0\x9c\xec\xb6\x9c \xec\x8b\x9c\xec\x9e\x91", open_text, kbo_secondary_draft_schedule_step_class(state, 0));
    kbo_secondary_draft_schedule_append_step(buffer, "\xeb\xb3\xb4\xed\x98\xb8 \xeb\xaa\x85\xeb\x8b\xa8 \xec\xa0\x9c\xec\xb6\x9c \xeb\xa7\x88\xea\xb0\x90", deadline_text, kbo_secondary_draft_schedule_step_class(state, 1));
    kbo_secondary_draft_schedule_append_step(buffer, "\xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8\xec\x9d\xbc", draft_text, kbo_secondary_draft_schedule_step_class(state, 2));
    kbo_window_text_appendf(buffer, "</div></section>");
}

static void kbo_secondary_draft_schedule_append_history_row(
    KboWindowTextBuffer* buffer,
    uint32_t season,
    uint32_t selected_season,
    uint32_t today)
{
    KboSecondaryDraftRunSummary summary;
    memset(&summary, 0, sizeof(summary));
    KboSecondaryDraftWindow window;
    memset(&window, 0, sizeof(window));
    int has_summary = kbo_secondary_draft_load_run_summary(season, &summary);
    int has_window = kbo_secondary_draft_load_window(season, &window);
    KboSecondaryDraftScheduleState state =
        kbo_secondary_draft_schedule_state(has_summary, has_window, today, &window);
    char open_text[16] = "-";
    char deadline_text[16] = "-";
    char draft_text[16] = "-";
    char window_text[64] = "-";
    char result_text[64] = "-";
    char result_subtext[64] = "-";
    char cash_text[48] = "-";
    if (has_window) {
        kbo_secondary_draft_ui_format_date(window.protection_open_yyyymmdd, open_text, sizeof(open_text));
        kbo_secondary_draft_ui_format_date(
            window.protection_deadline_yyyymmdd,
            deadline_text,
            sizeof(deadline_text));
        kbo_secondary_draft_ui_format_date(window.draft_yyyymmdd, draft_text, sizeof(draft_text));
        snprintf(window_text, sizeof(window_text), "%s ~ %s", open_text, deadline_text);
    }
    if (has_summary) {
        if (!has_window) {
            kbo_secondary_draft_ui_format_date(summary.event_yyyymmdd, draft_text, sizeof(draft_text));
        }
        kbo_secondary_draft_ui_format_cash(summary.cash_total, cash_text, sizeof(cash_text));
        snprintf(result_text, sizeof(result_text), "\xec\xa7\x80\xeb\xaa\x85 %d\xeb\xaa\x85", summary.pick_count);
        snprintf(
            result_subtext,
            sizeof(result_subtext),
            "\xed\x9b\x84\xeb\xb3\xb4 %d / \xeb\xb3\xb4\xed\x98\xb8 %d",
            summary.candidate_count,
            summary.protected_count);
    }

    kbo_window_text_appendf(
        buffer,
        "<tr class='%s'><td class='roDate' data-sort-value='%u'>%u</td><td class='roStatus'>",
        season == selected_season ? "selected" : "",
        season,
        season);
    kbo_html_append_escaped(buffer, kbo_secondary_draft_schedule_status_text(state));
    kbo_window_text_appendf(
        buffer,
        "</td><td class='roTimeline'><div class='secondaryDraftTableMain'>");
    kbo_html_append_escaped(buffer, window_text);
    kbo_window_text_appendf(
        buffer,
        "</div><div class='secondaryDraftTableSub'>\xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8 ");
    kbo_html_append_escaped(buffer, draft_text);
    kbo_window_text_appendf(
        buffer,
        "</div></td><td class='roResult'><div class='secondaryDraftTableMain'>");
    kbo_html_append_escaped(buffer, result_text);
    kbo_window_text_appendf(buffer, "</div><div class='secondaryDraftTableSub'>");
    kbo_html_append_escaped(buffer, result_subtext);
    kbo_window_text_appendf(
        buffer,
        "</div></td><td class='roCash' data-sort-value='%lld'>",
        has_summary ? (long long)summary.cash_total : 0ll);
    kbo_html_append_escaped(buffer, cash_text);
    kbo_window_text_appendf(buffer, "</td><td class='roAction'>");
    if (season == selected_season) {
        kbo_window_text_appendf(buffer, "<span class='secondaryDraftInlineState'>\xed\x98\x84\xec\x9e\xac</span>");
    } else {
        kbo_window_text_appendf(
            buffer,
            "<a class='secondaryDraftInlineLink' href='kbo://secondary-draft/year/%u'>\xeb\xb3\xb4\xea\xb8\xb0</a>",
            season);
    }
    kbo_window_text_appendf(buffer, "</td></tr>");
}

void kbo_secondary_draft_ui_append_schedule_view(
    KboWindowTextBuffer* buffer,
    const uint32_t* seasons,
    int season_count,
    uint32_t selected_season)
{
    uint32_t focus_season = selected_season;
    if (focus_season == 0u && season_count > 0 && seasons != NULL) {
        focus_season = seasons[0];
    }
    uint32_t today = kbo_secondary_draft_schedule_today();

    kbo_window_text_appendf(buffer, "<div class='secondaryDraftScheduleView'>");
    kbo_secondary_draft_schedule_append_season_rail(buffer, seasons, season_count, focus_season);
    kbo_secondary_draft_schedule_append_focus(buffer, focus_season, today);

    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap secondaryDraftScheduleHistory'>"
        "<table class='ootpRosterTable secondaryDraftScheduleTable'><thead><tr>"
        "<th class='roDate' data-sort-type='number'>\xec\x8b\x9c\xec\xa6\x8c</th>"
        "<th class='roStatus' data-sort-type='text'>\xec\x83\x81\xed\x83\x9c</th>"
        "<th class='roTimeline' data-sort-type='text'>\xec\x9d\xbc\xec\xa0\x95</th>"
        "<th class='roResult' data-sort-type='text'>\xea\xb2\xb0\xea\xb3\xbc</th>"
        "<th class='roCash' data-sort-type='number'>\xeb\xb3\xb4\xec\x83\x81\xea\xb8\x88</th>"
        "<th class='roAction'>\xec\x84\xa0\xed\x83\x9d</th>"
        "</tr></thead><tbody>");

    int emitted = 0;
    int selected_found = 0;
    if (seasons != NULL) {
        for (int i = 0; i < season_count; i++) {
            if (seasons[i] == focus_season) {
                selected_found = 1;
            }
            kbo_secondary_draft_schedule_append_history_row(buffer, seasons[i], focus_season, today);
            emitted++;
        }
    }
    if (!selected_found && focus_season != 0u) {
        kbo_secondary_draft_schedule_append_history_row(buffer, focus_season, focus_season, today);
        emitted++;
    }
    if (emitted <= 0) {
        kbo_secondary_draft_ui_append_empty_row(
            buffer,
            6,
            "\xec\x95\x84\xec\xa7\x81 \xeb\x93\xb1\xeb\xa1\x9d\xeb\x90\x9c \x32\xec\xb0\xa8 \xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8 \xec\x9d\xbc\xec\xa0\x95\xec\x9d\xb4 \xec\x97\x86\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
