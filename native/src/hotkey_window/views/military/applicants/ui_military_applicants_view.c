#include "../internal/ui_military_view_internal.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../../core/dates/constants/kbo_date_constants.h"

int kbo_military_resolve_application_window(
    uint32_t* out_today,
    uint32_t* out_anchor,
    uint32_t* out_announcement)
{
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();

    uint32_t anchor = kbo_get_latest_offseason_starts_event(today);
    if (anchor == 0u) {
        anchor = kbo_recent_phase_transition_offseason_anchor(league_id, today);
    }
    if (anchor == 0u
            && g_kbo_custom_event_last_offseason_transition_anchor != 0u
            && g_kbo_custom_event_last_offseason_transition_anchor <= today) {
        anchor = g_kbo_custom_event_last_offseason_transition_anchor;
    }
    if (anchor == 0u
            && g_kbo_foreign_priority_last_scheduled_date != 0u
            && g_kbo_foreign_priority_last_scheduled_date <= today) {
        anchor = g_kbo_foreign_priority_last_scheduled_date;
    }

    uint32_t announcement = kbo_add_one_month_yyyymmdd(anchor);
    if (out_today != NULL) { *out_today = today; }
    if (out_anchor != NULL) { *out_anchor = anchor; }
    if (out_announcement != NULL) { *out_announcement = announcement; }
    if (anchor == 0u || announcement == 0u) {
        return 0;
    }
    return today >= anchor && today <= announcement;
}

int kbo_military_applicant_position_bucket(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET, sizeof(uint8_t))) {
        return 4;
    }
    switch (player[OOTP27_PLAYER_POSITION_GROUP_OFFSET]) {
    case 1u: return 0;
    case 2u: return 1;
    case 3u:
    case 4u:
    case 5u:
    case 6u: return 2;
    case 7u:
    case 8u:
    case 9u: return 3;
    default: break;
    }
    return 4;
}

void kbo_military_refresh_applicants_for_hotkey_view(void)
{
    uint32_t entry_year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    (void)kbo_current_date_tick_latest_components(&entry_year, &month, &day);
    if (entry_year < KBO_SEASON_YEAR_MIN || entry_year > KBO_RECORD_YEAR_MAX) {
        return;
    }

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint32_t sang_id = sang != NULL && memory_range_readable(sang, OOTP27_KBO_TEAM_READABLE_BYTES)
        ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET)
        : 0u;
    if (sang_id == 0u) {
        return;
    }
    kbo_refresh_military_selection_candidates_from_memory(
        (uint16_t)entry_year,
        sang_id,
        "hotkey_ui_applicants");
}

void kbo_webview_append_military_applicant_summary(
    KboWindowTextBuffer* buffer,
    int application_active,
    int pending,
    int pitchers,
    int catchers,
    int infielders,
    int outfielders,
    int other,
    uint32_t anchor,
    uint32_t announcement)
{
    char anchor_text[16] = "-";
    char announcement_text[16] = "-";
    kbo_military_format_yyyymmdd(anchor, anchor_text, sizeof(anchor_text));
    kbo_military_format_yyyymmdd(announcement, announcement_text, sizeof(announcement_text));

    char summary_text[320] = {0};
    snprintf(
        summary_text,
        sizeof(summary_text),
        "보기: 지원자 - 상태: %s - 포지션: 투수 %d / 포수 %d / 내야 %d / 외야 %d / 기타 %d - 지원자 %d명 - 기간: %s ~ %s",
        application_active ? "열림" : "닫힘",
        pitchers,
        catchers,
        infielders,
        outfielders,
        other,
        pending,
        anchor_text,
        announcement_text);
    kbo_webview_append_roster_top_bar(buffer, summary_text);
}

void kbo_webview_append_military_applicants_view(KboWindowTextBuffer* buffer)
{
    uint32_t today = 0u;
    uint32_t anchor = 0u;
    uint32_t announcement = 0u;
    int application_active = kbo_military_resolve_application_window(&today, &anchor, &announcement);
    if (application_active) {
        kbo_military_refresh_applicants_for_hotkey_view();
    }

    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }

    int pending = 0;
    int position_counts[5] = {0};
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u) { continue; }
        if (candidate->selected == 0u) {
            pending++;
            uintptr_t player_ptr = candidate->player_ptr;
            if (!kbo_player_pointer_plausible(player_ptr)) {
                player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
            }
            int bucket = kbo_military_applicant_position_bucket(
                kbo_player_pointer_plausible(player_ptr) ? (uint8_t*)player_ptr : NULL);
            if (bucket < 0 || bucket > 4) {
                bucket = 4;
            }
            position_counts[bucket]++;
        }
    }

    (void)today;
    int summary_pending = application_active ? pending : 0;
    int summary_pitchers = application_active ? position_counts[0] : 0;
    int summary_catchers = application_active ? position_counts[1] : 0;
    int summary_infielders = application_active ? position_counts[2] : 0;
    int summary_outfielders = application_active ? position_counts[3] : 0;
    int summary_other = application_active ? position_counts[4] : 0;

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights applicantRights'>");
    kbo_webview_append_military_applicant_summary(
        buffer,
        application_active,
        summary_pending,
        summary_pitchers,
        summary_catchers,
        summary_infielders,
        summary_outfielders,
        summary_other,
        anchor,
        announcement);

    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable applicantRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>포지션</th><th class='roName' data-sort-type='text'>선수</th>"
        "<th class='roClub' data-sort-type='text'>원 소속</th><th class='roAge' data-sort-type='number'>나이</th>"
        "<th class='roEntry' data-sort-type='number'>입대 연도</th><th class='roStatus' data-sort-type='text'>상태</th>"
        "</tr></thead><tbody>");

    int rendered = 0;
    if (!application_active) {
        kbo_window_text_appendf(buffer, "<tr><td class='roEmptyMessage' colspan='6'>\xec\xa7\x80\xec\x9b\x90\xea\xb8\xb0\xea\xb0\x84\xec\x9d\xb4 \xec\x95\x84\xeb\x8b\x99\xeb\x8b\x88\xeb\x8b\xa4.</td></tr>");
    }
    for (LONG i = 0; application_active && i < count && rendered < 500; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u) { continue; }
        if (candidate->selected != 0u) { continue; }

        uintptr_t player_ptr = candidate->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
        }

        char player_name[96] = {0};
        const char* position_label = "-";
        uint16_t age = 0u;
        if (kbo_player_pointer_plausible(player_ptr)) {
            uint8_t* player = (uint8_t*)player_ptr;
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            position_label = kbo_webview_player_position_label(player, 0u);
            if (memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))) {
                age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
            }
        } else {
            snprintf(player_name, sizeof(player_name), "#%u", candidate->player_id);
        }

        char original_team_name[64] = {0};
        kbo_hub_copy_team_display_name_by_id(candidate->original_team_id, original_team_name, sizeof(original_team_name), NULL);
        if (original_team_name[0] == '\0') {
            snprintf(original_team_name, sizeof(original_team_name), "-");
        }

        kbo_window_text_appendf(buffer, "<tr><td class='roPo'>%s</td>", position_label);
        kbo_webview_append_player_name_cell(buffer, player_name[0] != '\0' ? player_name : "알 수 없는 선수", candidate->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, original_team_name);
        if (age > 0u) {
            kbo_window_text_appendf(
                buffer,
                "</td><td class='roAge'>%u</td><td class='roEntry'>%u</td><td class='roStatus'>대기</td></tr>",
                (uint32_t)age,
                (uint32_t)candidate->entry_year);
        } else {
            kbo_window_text_appendf(
                buffer,
                "</td><td class='roAge'></td><td class='roEntry'>%u</td><td class='roStatus'>대기</td></tr>",
                (uint32_t)candidate->entry_year);
        }
        rendered++;
    }

    if (application_active && rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
