#include "ui_fa_cases_view_internal.h"
#include "../../../../team/lookup/team_lookup.h"

static void kbo_webview_append_fa_market_grade_cell(
    KboWindowTextBuffer* buffer,
    const KboFaMarketClassification* row,
    const char* grade_display,
    uint32_t grade_sort_rank)
{
    int has_grade = grade_display != NULL && strcmp(grade_display, "-") != 0;
    kbo_window_text_appendf(
        buffer,
        "<td class='roGrade%s' data-sort-value='%u'><span class='faGradeValue'>",
        has_grade ? " hasHover" : "",
        grade_sort_rank);
    kbo_html_append_escaped(buffer, grade_display != NULL ? grade_display : "-");
    kbo_window_text_appendf(buffer, "</span>");

    if (has_grade && row != NULL) {
        char previous_team[16] = "-";
        char salary_text[32] = "-";
        char salary_with_currency[40] = "-";
        uint32_t team_id = row->fa_grade_snapshot_team_id != 0u
            ? row->fa_grade_snapshot_team_id
            : kbo_fa_market_display_team_id(row);

        kbo_hub_copy_team_abbrev_by_id(team_id, previous_team, sizeof(previous_team), "-");
        kbo_fa_market_format_salary(row->fa_grade_salary, salary_text, sizeof(salary_text));
        if (salary_text[0] != '\0' && strcmp(salary_text, "-") != 0) {
            snprintf(salary_with_currency, sizeof(salary_with_currency), "$%s", salary_text);
        }

        kbo_window_text_appendf(
            buffer,
            "<span class='faGradeTooltip'>"
            "<span class='faGradeTipRow'><span class='faGradeTipLabel'>이전 구단</span><span class='faGradeTipValue'>");
        kbo_html_append_escaped(buffer, previous_team);
        kbo_window_text_appendf(
            buffer,
            "</span></span><span class='faGradeTipRow'><span class='faGradeTipLabel'>직전 연봉</span><span class='faGradeTipValue'>");
        kbo_html_append_escaped(buffer, salary_with_currency);
        kbo_window_text_appendf(buffer, "</span></span></span>");
    }

    kbo_window_text_appendf(buffer, "</td>");
}

void kbo_webview_append_fa_cases_view(KboWindowTextBuffer* buffer, uint32_t selected_league_id)
{
    if (buffer == NULL) {
        return;
    }
    KBO_PROFILE_BEGIN(profile_fa_cases_view);

    static KboFaMarketClassification s_cached_rows[KBO_FA_MARKET_UI_MAX_ROWS];
    static KboFaMarketScanSummary s_cached_summary;
    static uint32_t s_cached_league_id = 0u;
    static uint32_t s_cached_today = 0u;
    static uint32_t s_cached_year = 0u;
    static int s_cached_count = -1;

    uint32_t today = 0u;
    uint32_t current_year = 0u;
    kbo_get_current_yyyymmdd(&today);
    kbo_current_year_relaxed(&current_year);

    KboFaMarketScanSummary summary = {0};
    int count = 0;
    int cache_hit = 0;
    if (s_cached_count >= 0
            && s_cached_league_id == selected_league_id
            && s_cached_today == today
            && s_cached_year == current_year) {
        count = s_cached_count;
        summary = s_cached_summary;
        cache_hit = 1;
    } else {
        KBO_PROFILE_BEGIN(profile_fa_cases_collect);
        count = kbo_collect_fa_market_classifications(
            selected_league_id,
            s_cached_rows,
            KBO_FA_MARKET_UI_MAX_ROWS,
            &summary,
            0,
            "f2_webview_all_players");
        s_cached_league_id = selected_league_id;
        s_cached_today = today;
        s_cached_year = current_year;
        s_cached_count = count;
        s_cached_summary = summary;
        KBO_PROFILE_END(profile_fa_cases_collect, "webview.fa_market.collect");
    }

    KBO_PROFILE_BEGIN(profile_fa_cases_render);
    int filtered_count = 0;
    for (int i = 0; i < count; i++) {
        if (kbo_fa_market_row_matches_filter(&s_cached_rows[i], g_kbo_hub_fa_market_filter)
                && kbo_fa_market_row_matches_position_filter(
                    &s_cached_rows[i],
                    g_kbo_hub_fa_market_position_filter)) {
            filtered_count++;
        }
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights faCases faMarketView'>");
    kbo_webview_append_fa_market_filter_bar(buffer, filtered_count, count);

    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap faMarketTableWrap'><table class='ootpRosterTable faCasesTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>포지션</th>"
        "<th class='roName' data-sort-type='text'>선수</th>"
        "<th data-sort-type='text'>유형</th>"
        "<th class='roGrade' data-sort-type='number'>등급</th>"
        "<th class='roAge' data-sort-type='number'>나이</th>"
        "<th class='roNat' data-sort-type='text'>국적</th>"
        "<th data-sort-type='text'>보류권</th>"
        "</tr></thead><tbody>");

    for (int i = 0; i < count; i++) {
        KboFaMarketClassification* row = &s_cached_rows[i];
        if (!kbo_fa_market_row_matches_filter(row, g_kbo_hub_fa_market_filter)
                || !kbo_fa_market_row_matches_position_filter(
                    row,
                    g_kbo_hub_fa_market_position_filter)) {
            continue;
        }
        char rights_abbrev[16] = "-";
        const char* grade_display = kbo_fa_market_display_grade(row->grade);
        uint32_t grade_sort_rank = kbo_fa_market_display_grade_sort_rank(row->grade);
        kbo_hub_copy_team_abbrev_by_id(row->rights_team_id, rights_abbrev, sizeof(rights_abbrev), "-");
        kbo_window_text_appendf(buffer, "<tr>");
        kbo_window_text_appendf(buffer, "<td class='roPo'>");
        kbo_html_append_escaped(
            buffer,
            kbo_webview_position_label_from_values(row->position_group, row->position_role));
        kbo_window_text_appendf(buffer, "</td>");
        kbo_webview_append_player_name_cell(buffer, row->player_name, row->player_id);
        kbo_window_text_appendf(buffer, "<td>");
        kbo_html_append_escaped(buffer, kbo_fa_market_display_case_label(row->case_label));
        kbo_window_text_appendf(buffer, "</td>");
        kbo_webview_append_fa_market_grade_cell(buffer, row, grade_display, grade_sort_rank);
        kbo_window_text_appendf(buffer, "<td class='roAge'>%u</td>", (uint32_t)row->age);
        kbo_webview_append_roster_nation_cell(buffer, row->nation_id, kbo_hub_nation_flag_asset_path);
        kbo_window_text_appendf(buffer, "<td>");
        kbo_html_append_escaped(buffer, rights_abbrev);
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    if (filtered_count == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='7'>현재 필터와 일치하는 FA 선수를 찾지 못했습니다.</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    KBO_PROFILE_END(profile_fa_cases_render, cache_hit
        ? "webview.fa_market.render.cache_hit"
        : "webview.fa_market.render.fresh");
    KBO_PROFILE_END(profile_fa_cases_view, cache_hit
        ? "webview.fa_market.total.cache_hit"
        : "webview.fa_market.total.fresh");
}
