#include "ui_cbt_view_internal.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"

static int kbo_cbt_view_compare(const void* a, const void* b)
{
    const KboCbtRecord* ra = (const KboCbtRecord*)a;
    const KboCbtRecord* rb = (const KboCbtRecord*)b;
    if (ra->season != rb->season) return ra->season > rb->season ? -1 : 1;
    if (ra->overage != rb->overage) return ra->overage > rb->overage ? -1 : 1;
    return 0;
}

static int kbo_cbt_record_visible_in_hub(const KboCbtRecord* rec, uint32_t today)
{
    if (rec == NULL || rec->season == 0u) {
        return 0;
    }
    uint32_t current_year = today / 10000u;
    if (today == 0u || rec->season != current_year) {
        return 1;
    }
    if (rec->processed_date != 0u && today >= rec->processed_date) {
        return 1;
    }

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    uint32_t opening_day = 0u;
    if (!kbo_cbt_exception_resolve_opening_day(rec->season, &opening_day)) {
        return 0;
    }
    uint32_t announcement = kbo_add_days_yyyymmdd(opening_day, rules.announcement_days_after_opening);
    if (announcement == 0u || today < announcement) {
        return 0;
    }
    return kbo_custom_event_processed_marker_exists_for_kind(
        announcement,
        KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT);
}

void kbo_cbt_format_usd(char* out, size_t out_size, int32_t value)
{
    if (value <= 0) {
        snprintf(out, out_size, "-");
    } else if (value >= 1000000) {
        int m = value / 1000000;
        int tenth = (value % 1000000) / 100000;
        snprintf(out, out_size, tenth == 0 ? "$%dM" : "$%d.%dM", m, tenth);
    } else if (value >= 1000) {
        snprintf(out, out_size, "$%dk", value / 1000);
    } else {
        snprintf(out, out_size, "$%d", value);
    }
}

int kbo_cbt_load_sorted(KboCbtRecord** out_records, int* out_count)
{
    KboCbtRecord* records = (KboCbtRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_RECORDS_MAX * sizeof(KboCbtRecord));
    if (records == NULL) return 0;
    int loaded = kbo_cbt_load_records(records, KBO_CBT_RECORDS_MAX, NULL, 0);
    uint32_t today = 0u;
    kbo_current_date_tick_latest_published_date(&today);
    int filtered = 0;
    for (int i = 0; i < loaded; i++) {
        if (!kbo_cbt_record_visible_in_hub(&records[i], today)) {
            continue;
        }
        if (filtered != i) {
            records[filtered] = records[i];
        }
        filtered++;
    }
    if (filtered < loaded) {
        memset(records + filtered, 0, (SIZE_T)(loaded - filtered) * sizeof(records[0]));
    }
    *out_count = filtered;
    if (*out_count > 0) {
        qsort(records, (size_t)*out_count, sizeof(KboCbtRecord), kbo_cbt_view_compare);
    }
    *out_records = records;
    return 1;
}

int kbo_cbt_is_latest_for_team(const KboCbtRecord* records, int index)
{
    for (int i = 0; i < index; i++) {
        if (records[i].team_id == records[index].team_id) return 0;
    }
    return 1;
}

void kbo_cbt_team_name(const KboCbtRecord* rec, char* out, size_t out_size)
{
    out[0] = '\0';
    if (rec != NULL && rec->team_id != 0u) {
        kbo_hub_copy_team_abbrev_by_id(rec->team_id, out, out_size, NULL);
    }
    if (out[0] == '\0') snprintf(out, out_size, rec != NULL ? "T%u" : "-", rec != NULL ? rec->team_id : 0u);
}

static int kbo_cbt_ratio_pct(const KboCbtRecord* rec)
{
    if (rec->threshold <= 0) return 0;
    return (int)((int64_t)rec->payroll * 100LL / (int64_t)rec->threshold);
}

static void kbo_cbt_format_ratio_label(char* out, size_t out_size, const KboCbtRecord* rec)
{
    int ratio = rec->threshold > 0
        ? (int)((int64_t)rec->payroll * 1000LL / (int64_t)rec->threshold)
        : 0;
    if (ratio >= 1000) {
        snprintf(out, out_size, "%d.%03d", ratio / 1000, ratio % 1000);
    } else {
        snprintf(out, out_size, ".%03d", ratio);
    }
}

static void kbo_cbt_append_overview_css(KboWindowTextBuffer* buffer)
{
    kbo_window_text_appendf(buffer,
        "<style>"
        ".cbtOverview{display:grid;grid-template-rows:31px minmax(0,1fr);height:100%%;min-height:0;background:#181818;border-top:1px solid #070707}"
        ".cbtReportBar{height:31px;display:flex;align-items:center;gap:18px;padding:0 8px;background:#222;border-bottom:1px solid #070707;color:#dcdcdc;font-family:var(--ui-font);font-size:13px;font-weight:900;white-space:nowrap;overflow:hidden}"
        ".cbtReportItem{display:inline-flex;align-items:center;gap:6px;min-width:0}.cbtReportLabel{color:#9d9d9d;text-transform:uppercase}.cbtReportValue{color:#f0f0f0}.cbtReportValue.warn{color:#ff8a00}.cbtReportValue.good{color:#b7d6b8}"
        ".cbtReportBody{min-height:0;display:grid;grid-template-columns:minmax(360px,42%%) minmax(0,1fr);gap:0;overflow:hidden}"
        ".cbtLeagueTableWrap{min-height:0;border-right:1px solid #050505;background:#1c1c1c;overflow:auto}.cbtLeagueTable{width:100%%;border-collapse:collapse;table-layout:fixed;font-family:var(--ui-font);font-size:12px}"
        ".cbtLeagueTable th{height:24px;padding:0 6px;background:#2b2b2b;color:#f1f1f1;border-bottom:1px solid #050505;font-weight:900;text-align:left;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".cbtLeagueTable td{height:24px;padding:0 6px;background:#202020;color:#e8e8e8;border-bottom:1px solid #151515;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.cbtLeagueTable tr:nth-child(even) td{background:#252525}"
        ".cbtLeagueTable .num{text-align:right}.cbtLeagueTable .status{color:#80c47f}.cbtLeagueTable .status.over{color:#ff8a00;font-weight:900}"
        ".cbtChartWrap{min-height:0;padding:7px 9px 9px;background:#1a1a1a;overflow:hidden}"
        ".cbtChartHead{height:24px;display:flex;align-items:center;justify-content:space-between;gap:10px;border-bottom:1px solid #303030;margin-bottom:6px;overflow:hidden}"
        ".cbtChartTitle{min-width:0;color:#f0f0f0;font-size:13px;font-weight:900;line-height:18px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;text-transform:uppercase}"
        ".cbtChartMeta{flex:none;color:#9d9d9d;font-size:12px;font-weight:900;white-space:nowrap}"
        ".cbtPlot{position:relative;height:calc(100%% - 30px);min-height:210px;padding:10px 8px 28px 43px;background:#181818;border:1px solid #070707;overflow:hidden}"
        ".cbtGrid{position:absolute;left:43px;right:8px;height:1px;background:#333}.cbtGrid.zero{background:#060606}.cbtAxisLabel{position:absolute;left:0;width:36px;margin-top:-7px;color:#cfcfcf;font-size:11px;font-weight:900;text-align:right}"
        ".cbtBars{position:absolute;left:47px;right:12px;bottom:28px;top:10px;display:flex;align-items:flex-end;gap:5px;padding:0 2px}"
        ".cbtBarSlot{position:relative;flex:1 1 0;min-width:27px;height:100%%;display:flex;align-items:flex-end;justify-content:center}"
        ".cbtBar{width:100%%;max-width:34px;min-height:2px;background:#6a6a6a;border:1px solid #929292;border-bottom-color:#363636;box-shadow:inset 0 1px 0 rgba(255,255,255,.12)}.cbtBar.over{background:#963313;border-color:#e0763e;border-bottom-color:#4a1608}.cbtBar.clean{background:#37613c;border-color:#6f9a71;border-bottom-color:#1d3b21}"
        ".cbtBarLabel{position:absolute;left:50%%;transform:translateX(-50%%);margin-bottom:4px;color:#f2f2f2;font-size:11px;font-weight:900;text-shadow:0 1px 1px #000;white-space:nowrap}"
        ".cbtBarTeam{position:absolute;left:50%%;bottom:-22px;transform:translateX(-50%%);max-width:56px;color:#e7e7e7;font-size:11px;font-weight:900;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;text-align:center}"
        ".cbtEmpty{height:100%%;display:grid;grid-template-columns:300px minmax(0,1fr);gap:0;align-items:stretch}"
        ".cbtEmptyStatus{display:flex;flex-direction:column;justify-content:center;gap:6px;padding:0 14px;background:#202020;border-right:1px solid #070707}"
        ".cbtEmptyTitle{color:#ededed;font-size:13px;font-weight:900;text-transform:uppercase;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".cbtEmptyLine{color:#aaa;font-size:12px;font-weight:700;line-height:1.35}.cbtEmptyLine strong{color:#d8d8d8}"
        ".cbtEmptyPreview{position:relative;min-height:0;background:#181818;overflow:hidden}"
        ".cbtEmptyPreview:before{content:'';position:absolute;left:43px;right:8px;top:50%%;height:1px;background:#303030;box-shadow:0 -72px 0 #303030,0 72px 0 #303030}"
        ".cbtEmptyPreviewText{position:absolute;left:24px;bottom:18px;color:#878787;font-size:13px;font-weight:900}"
        "</style>");
}

static void kbo_webview_append_cbt_overview_view(KboWindowTextBuffer* buffer)
{
    KboCbtRecord* records = NULL;
    int count = 0;
    if (!kbo_cbt_load_sorted(&records, &count)) return;

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);

    int clubs = 0, violations = 0, penalties = 0;
    int32_t total_tax = 0;
    uint32_t latest_season = count > 0 ? records[0].season : 0u;
    int32_t latest_threshold = count > 0 ? records[0].threshold : 0;
    for (int i = 0; i < count; i++) {
        if (!kbo_cbt_is_latest_for_team(records, i)) continue;
        clubs++;
        if (records[i].overage > 0) {
            violations++;
            total_tax += records[i].tax_amount;
            if (records[i].consecutive_count >= rules.draft_penalty_min_consecutive) penalties++;
        }
    }

    char summary[256];
    snprintf(summary, sizeof(summary),
        count > 0 ? "%u시즌 개요 - %d개 구단 - 기준선 초과 %d개" : "아직 기록 없음 - 개막일 스냅샷에서 생성",
        latest_season, clubs, violations);

    char tax_text[32], threshold_text[32];
    kbo_cbt_format_usd(tax_text, sizeof(tax_text), total_tax);
    kbo_cbt_format_usd(threshold_text, sizeof(threshold_text), latest_threshold);
    uint32_t current_year = 0u, cm = 0u, cd = 0u;
    kbo_current_date_tick_latest_components(&current_year, &cm, &cd);
    uint32_t opening_day = 0u;
    uint32_t announcement_day = 0u;
    if (current_year != 0u && kbo_cbt_exception_resolve_opening_day(current_year, &opening_day)) {
        announcement_day = kbo_add_days_yyyymmdd(opening_day, rules.announcement_days_after_opening);
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary);
    kbo_cbt_append_overview_css(buffer);
    kbo_window_text_appendf(buffer,
        "<div class='cbtOverview'><div class='cbtReportBar'>"
        "<span class='cbtReportItem'><span class='cbtReportLabel'>기준선</span><span class='cbtReportValue'>");
    kbo_html_append_escaped(buffer, threshold_text);
    kbo_window_text_appendf(buffer,
        "</span></span><span class='cbtReportItem'><span class='cbtReportLabel'>초과</span><span class='cbtReportValue %s'>%d개 구단</span></span>"
        "<span class='cbtReportItem'><span class='cbtReportLabel'>납부액</span><span class='cbtReportValue %s'>",
        violations > 0 ? "warn" : "good", violations, total_tax > 0 ? "warn" : "good");
    kbo_html_append_escaped(buffer, tax_text);
    kbo_window_text_appendf(buffer,
        "</span></span><span class='cbtReportItem'><span class='cbtReportLabel'>지명권</span><span class='cbtReportValue %s'>%d</span></span>"
        "</div><div class='cbtReportBody'>",
        penalties > 0 ? "warn" : "good", penalties);

    if (count <= 0) {
        char opening_text[16] = "-";
        char announcement_text[16] = "-";
        if (opening_day != 0u) {
            snprintf(opening_text, sizeof(opening_text), "%04u-%02u-%02u", opening_day / 10000u, (opening_day / 100u) % 100u, opening_day % 100u);
        }
        if (announcement_day != 0u) {
            snprintf(announcement_text, sizeof(announcement_text), "%04u-%02u-%02u", announcement_day / 10000u, (announcement_day / 100u) % 100u, announcement_day % 100u);
        }
        kbo_window_text_appendf(buffer,
            "<div class='cbtEmpty'>"
            "<div class='cbtEmptyStatus'>"
            "<div class='cbtEmptyTitle'>기록 대기</div>"
            "<div class='cbtEmptyLine'>개막일 <strong>");
        kbo_html_append_escaped(buffer, opening_text);
        kbo_window_text_appendf(buffer, "</strong></div><div class='cbtEmptyLine'>발표일 <strong>");
        kbo_html_append_escaped(buffer, announcement_text);
        kbo_window_text_appendf(buffer,
            "</strong></div>"
            "<div class='cbtEmptyLine'>발표 이벤트가 처리되면 구단별 납부 내역과 기준선 차트가 표시됩니다.</div>"
            "</div><div class='cbtEmptyPreview'><div class='cbtEmptyPreviewText'>리그 기준선 차트 준비 중</div></div>"
            "</div></div></div>");
        HeapFree(GetProcessHeap(), 0, records);
        return;
    }

    kbo_window_text_appendf(buffer,
        "<section class='cbtLeagueTableWrap'><table class='cbtLeagueTable'>"
        "<thead><tr><th style='width:58px'>구단</th><th class='num'>페이롤</th><th class='num'>초과액</th><th class='num'>세금</th><th style='width:66px'>상태</th></tr></thead><tbody>");

    for (int i = 0; i < count; i++) {
        const KboCbtRecord* rec = &records[i];
        if (!kbo_cbt_is_latest_for_team(records, i)) continue;
        char team[96], payroll[32], overage[32], tax[32];
        kbo_cbt_team_name(rec, team, sizeof(team));
        kbo_cbt_format_usd(payroll, sizeof(payroll), rec->payroll);
        kbo_cbt_format_usd(overage, sizeof(overage), rec->overage);
        kbo_cbt_format_usd(tax, sizeof(tax), rec->tax_amount);
        kbo_window_text_appendf(buffer, "<tr><td>");
        kbo_html_append_escaped(buffer, team);
        kbo_window_text_appendf(buffer, "</td><td class='num'>");
        kbo_html_append_escaped(buffer, payroll);
        kbo_window_text_appendf(buffer, "</td><td class='num'>");
        kbo_html_append_escaped(buffer, overage);
        kbo_window_text_appendf(buffer, "</td><td class='num'>");
        kbo_html_append_escaped(buffer, tax);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='status %s'>%s</td></tr>",
            rec->overage > 0 ? "over" : "",
            rec->overage > 0 ? "초과" : "정상");
    }

    kbo_window_text_appendf(buffer,
        "</tbody></table></section><section class='cbtChartWrap'>"
        "<div class='cbtChartHead'><div class='cbtChartTitle'>리그 사치세 기준선</div><div class='cbtChartMeta'>%u시즌</div></div>"
        "<div class='cbtPlot'>"
        "<div class='cbtGrid' style='top:0%%'></div><div class='cbtAxisLabel' style='top:0%%'>1.400</div>"
        "<div class='cbtGrid' style='top:14.2857%%'></div><div class='cbtAxisLabel' style='top:14.2857%%'>1.200</div>"
        "<div class='cbtGrid' style='top:28.5714%%'></div><div class='cbtAxisLabel' style='top:28.5714%%'>1.000</div>"
        "<div class='cbtGrid' style='top:42.8571%%'></div><div class='cbtAxisLabel' style='top:42.8571%%'>.800</div>"
        "<div class='cbtGrid' style='top:57.1428%%'></div><div class='cbtAxisLabel' style='top:57.1428%%'>.600</div>"
        "<div class='cbtGrid' style='top:71.4285%%'></div><div class='cbtAxisLabel' style='top:71.4285%%'>.400</div>"
        "<div class='cbtGrid' style='top:85.7142%%'></div><div class='cbtAxisLabel' style='top:85.7142%%'>.200</div>"
        "<div class='cbtGrid zero' style='top:100%%'></div><div class='cbtAxisLabel' style='top:100%%'>.000</div>"
        "<div class='cbtBars'>",
        latest_season);

    for (int i = 0; i < count; i++) {
        const KboCbtRecord* rec = &records[i];
        if (!kbo_cbt_is_latest_for_team(records, i)) continue;
        char team[96], label[16];
        int ratio = kbo_cbt_ratio_pct(rec);
        int capped = ratio < 0 ? 0 : (ratio > 140 ? 140 : ratio);
        int height = capped * 100 / 140;
        kbo_cbt_team_name(rec, team, sizeof(team));
        kbo_cbt_format_ratio_label(label, sizeof(label), rec);

        kbo_window_text_appendf(buffer,
            "<div class='cbtBarSlot'><div class='cbtBarLabel' style='bottom:calc(%d%% + 2px)'>",
            height);
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer,
            "</div><div class='cbtBar %s' style='height:%d%%'></div><div class='cbtBarTeam'>",
            rec->overage > 0 ? "over" : "clean",
            height);
        kbo_html_append_escaped(buffer, team);
        kbo_window_text_appendf(buffer, "</div></div>");
    }

    kbo_window_text_appendf(buffer, "</div></div></section></div></div>");
    HeapFree(GetProcessHeap(), 0, records);
}

void kbo_webview_append_cbt_view(KboWindowTextBuffer* buffer, int subview, uint32_t selected_team_id)
{
    if (buffer == NULL) return;
    if (subview == KBO_HUB_CBT_SUBVIEW_RULES) {
        kbo_webview_append_cbt_rules_view(buffer);
    } else if (subview == KBO_HUB_CBT_SUBVIEW_EXCEPTIONS) {
        kbo_webview_append_cbt_exceptions_view(buffer, selected_team_id);
    } else if (subview == KBO_HUB_CBT_SUBVIEW_HISTORY) {
        kbo_webview_append_cbt_history_view(buffer, selected_team_id);
    } else {
        kbo_webview_append_cbt_overview_view(buffer);
    }
}
