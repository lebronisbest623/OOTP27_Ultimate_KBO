#include "../ui_asian_games_view_internal.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"

int kbo_webview_weekday_for_yyyymmdd(uint32_t yyyymmdd)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    st.wYear = (WORD)year;
    st.wMonth = (WORD)month;
    st.wDay = (WORD)day;
    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) {
        return -1;
    }
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    uint64_t days_since_1601 = value.QuadPart / 10000000ull / 86400ull;
    return (int)((days_since_1601 + 1ull) % 7ull);
}

const char* kbo_webview_asian_games_schedule_status(
    uint32_t event_date,
    KboCustomEventKind event_kind,
    uint32_t fired_date,
    uint32_t today,
    int event_exists,
    int auto_schedule,
    const char** out_class)
{
    if (event_date == 0u) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "미정";
    }
    int processed = (fired_date == event_date)
        || kbo_custom_event_processed_marker_exists_for_kind(event_date, event_kind);
    if (processed) {
        if (out_class != NULL) { *out_class = "roReady"; }
        return "완료";
    }
    if (today == event_date) {
        if (out_class != NULL) { *out_class = "roOrange"; }
        return "오늘";
    }
    if (today != 0u && today > event_date) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "지난 일정";
    }
    if (event_exists) {
        if (out_class != NULL) { *out_class = "roServing"; }
        return "등록됨";
    }
    if (!auto_schedule) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "시드";
    }
    if (out_class != NULL) { *out_class = "roSoon"; }
    return "대기";
}

void kbo_webview_append_asian_games_schedule_row(
    KboWindowTextBuffer* buffer,
    uint32_t event_date,
    KboCustomEventKind event_kind,
    const char* event_label,
    const char* action_text,
    const char* impact_text,
    uint32_t fired_date,
    uint32_t today,
    uint32_t league_id,
    int auto_schedule)
{
    int event_exists = league_id != 0u && event_date != 0u
        ? kbo_custom_event_exists_by_kind_for_date(league_id, event_date, event_kind)
        : 0;
    const char* status_class = "";
    const char* status = kbo_webview_asian_games_schedule_status(
        event_date,
        event_kind,
        fired_date,
        today,
        event_exists,
        auto_schedule,
        &status_class);

    char date_text[16] = {0};
    const char* weekday = "";
    if (event_date != 0u) {
        snprintf(
            date_text,
            sizeof(date_text),
            "%04u-%02u-%02u",
            event_date / 10000u,
            (event_date / 100u) % 100u,
            event_date % 100u);
        weekday = kbo_hub_weekday_abbrev(kbo_webview_weekday_for_yyyymmdd(event_date));
    } else {
        snprintf(date_text, sizeof(date_text), "미정");
    }

    kbo_window_text_appendf(
        buffer,
        "<tr><td class='roDate'>%s</td><td class='roPo'>%s</td><td class='roName'>",
        date_text,
        weekday);
    kbo_html_append_escaped(buffer, event_label);
    kbo_window_text_appendf(buffer, "</td><td class='roLeague'>");
    kbo_html_append_escaped(buffer, action_text);
    kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
    kbo_html_append_escaped(buffer, impact_text);
    kbo_window_text_appendf(buffer, "</td><td class='roStatus %s'>", status_class);
    kbo_html_append_escaped(buffer, status);
    kbo_window_text_appendf(buffer, "</td></tr>");
}

void kbo_webview_append_asian_games_schedule_view(KboWindowTextBuffer* buffer)
{
    KboAsianGamesScheduleSeed schedule;
    int has_schedule = kbo_webview_asian_games_schedule(&schedule);
    uint32_t schedule_year = has_schedule ? schedule.year : 0u;
    uint32_t today = 0u;
    kbo_current_date_tick_latest_published_date(&today);

    uint32_t league_id = kbo_resolve_kbo_league_id();

    uint32_t selection_date = has_schedule ? schedule.selection_date : 0u;
    uint32_t departure_date = has_schedule ? schedule.departure_date : 0u;
    uint32_t final_date = has_schedule ? schedule.final_date : 0u;
    int auto_schedule = has_schedule ? kbo_asian_games_schedule_auto_events_enabled(&schedule) : 0;

    int completed = 0;
    if (selection_date != 0u && kbo_custom_event_processed_marker_exists_for_kind(selection_date, KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION)) { completed++; }
    if (departure_date != 0u && kbo_custom_event_processed_marker_exists_for_kind(departure_date, KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE)) { completed++; }
    if (final_date != 0u && kbo_custom_event_processed_marker_exists_for_kind(final_date, KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL)) { completed++; }

    char summary_text[256] = {0};
    if (schedule_year != 0u && kbo_asian_games_schedule_has_event_dates(&schedule)) {
        char host_text[128] = {0};
        if (schedule.host_city[0] != '\0' && schedule.host_country[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s, %s", schedule.host_city, schedule.host_country);
        } else if (schedule.host_city[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s", schedule.host_city);
        }
        snprintf(
            summary_text,
            sizeof(summary_text),
            "보기: 일정 - 아시안게임 %u%s - %s - 완료 %d / 3개 이벤트",
            schedule_year,
            host_text,
            kbo_asian_games_schedule_status_label(&schedule),
            completed);
    } else if (schedule_year != 0u) {
        char host_text[128] = {0};
        if (schedule.host_city[0] != '\0' && schedule.host_country[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s, %s", schedule.host_city, schedule.host_country);
        } else if (schedule.host_city[0] != '\0') {
            snprintf(host_text, sizeof(host_text), " - %s", schedule.host_city);
        }
        snprintf(
            summary_text,
            sizeof(summary_text),
            "보기: 일정 - 아시안게임 %u%s - %s / 날짜 미정",
            schedule_year,
            host_text,
            kbo_asian_games_schedule_status_label(&schedule));
    } else {
        snprintf(summary_text, sizeof(summary_text), "보기: 일정 - 아시안게임");
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable agScheduleTable'><thead><tr>"
        "<th class='roDate' data-sort-type='date'>날짜</th><th class='roPo' data-sort-type='text'>요일</th>"
        "<th class='roName' data-sort-type='text'>이벤트</th><th class='roLeague' data-sort-type='text'>처리</th>"
        "<th class='roClub' data-sort-type='text'>영향</th><th class='roStatus' data-sort-type='text'>상태</th>"
        "</tr></thead><tbody>");

    if (schedule_year == 0u) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6' class='roEmptyMessage'></td></tr>");
    } else {
        kbo_webview_append_asian_games_schedule_row(
            buffer,
            selection_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION,
            kbo_hub_text("\xeb\x8c\x80\xed\x91\x9c\xed\x8c\x80 \xeb\xb0\x9c\xed\x91\x9c", "대표팀 발표"),
            kbo_hub_text("KBO \xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84 \xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0 \xec\x84\xa0\xeb\xb0\x9c", "KBO 아시안게임 로스터 선발"),
            kbo_hub_text("24\xeb\xaa\x85 \xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "24명 로스터"),
            g_kbo_asian_games_last_selection_fired_date,
            today,
            league_id,
            auto_schedule);
        kbo_webview_append_asian_games_schedule_row(
            buffer,
            departure_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE,
            kbo_hub_text("\xec\x84\xa0\xec\x88\x98\xeb\x8b\xa8 \xec\xb6\x9c\xea\xb5\xad", "선수단 출국"),
            kbo_hub_text("\xec\x84\xa0\xeb\xb0\x9c \xec\x84\xa0\xec\x88\x98 \xea\xb5\xac\xeb\x8b\xa8 \xec\x9d\xb4\xed\x83\x88 \xeb\xb0\x8f \xeb\x8c\x80\xec\xb2\xb4 \xed\x99\x95\xec\x9d\xb8", "선발 선수 구단 이탈 및 대체 확인"),
            kbo_hub_text("\xec\xa0\x9c\xed\x95\x9c \xeb\xaa\x85\xeb\x8b\xa8", "제한 명단"),
            g_kbo_asian_games_last_departure_fired_date,
            today,
            league_id,
            auto_schedule);
        kbo_webview_append_asian_games_schedule_row(
            buffer,
            final_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL,
            kbo_hub_text("\xea\xb2\xb0\xec\x8a\xb9 / \xeb\xb3\xb5\xea\xb7\x80", "결승 / 복귀"),
            kbo_hub_text("\xeb\x8c\x80\xed\x9a\x8c \xec\xa2\x85\xeb\xa3\x8c \xed\x9b\x84 \xea\xb5\xac\xeb\x8b\xa8 \xeb\xb3\xb5\xea\xb7\x80 \xeb\xb0\x8f \xeb\xb3\x91\xec\x97\xad \xed\x98\x9c\xed\x83\x9d \xec\xb2\x98\xeb\xa6\xac", "대회 종료 후 구단 복귀 및 병역 혜택 처리"),
            kbo_hub_text("\xea\xb8\x88\xeb\xa9\x94\xeb\x8b\xac \xeb\xa9\xb4\xec\xa0\x9c", "금메달 면제"),
            g_kbo_asian_games_last_final_fired_date,
            today,
            league_id,
            auto_schedule);
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}

