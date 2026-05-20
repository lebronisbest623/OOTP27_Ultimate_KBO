#include "../ui_fa_views_internal.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"

static int kbo_fa_rights_report_row_latest_for_player(
    const KboFaDeclarationReportRow* rows,
    int count,
    int index)
{
    if (rows == NULL || index < 0 || index >= count || rows[index].player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (i == index || rows[i].player_id != rows[index].player_id || rows[i].season != rows[index].season) {
            continue;
        }
        if (rows[i].declaration_date > rows[index].declaration_date) {
            return 0;
        }
        if (rows[i].declaration_date == rows[index].declaration_date && i > index) {
            return 0;
        }
    }
    return 1;
}

static void kbo_fa_rights_append_status_cell(KboWindowTextBuffer* buffer, const KboFaDeclarationReportRow* row)
{
    const char* label = row != NULL && row->declared ? "선언" : "미선언";
    const char* class_name = row != NULL && row->declared ? "declared" : "deferred";
    kbo_window_text_appendf(buffer, "<td><span class='faRightsStatus %s'>", class_name);
    kbo_html_append_escaped(buffer, label);
    kbo_window_text_appendf(buffer, "</span></td>");
}

static void kbo_fa_rights_append_row(KboWindowTextBuffer* buffer, const KboFaDeclarationReportRow* row)
{
    char team_abbrev[16] = "-";
    char salary_text[32] = "-";
    kbo_hub_copy_team_abbrev_by_id(row->team_id, team_abbrev, sizeof(team_abbrev), "-");
    kbo_fa_market_format_salary(row->salary, salary_text, sizeof(salary_text));

    kbo_window_text_appendf(buffer, "<tr>");
    kbo_fa_rights_append_status_cell(buffer, row);
    kbo_webview_append_player_name_cell(buffer, row->player_name, row->player_id);
    kbo_window_text_appendf(buffer, "<td>");
    kbo_html_append_escaped(buffer, team_abbrev);
    kbo_window_text_appendf(buffer, "</td><td>");
    kbo_html_append_escaped(buffer, kbo_fa_market_display_grade(row->grade));
    kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%d'>", row->salary);
    kbo_html_append_escaped(buffer, salary_text);
    kbo_window_text_appendf(buffer, "</td></tr>");
}

static void kbo_fa_rights_append_table(
    KboWindowTextBuffer* buffer,
    const KboFaDeclarationReportRow* rows,
    const int* indices,
    int index_count)
{
    kbo_window_text_appendf(buffer, "<div class='rosterTopBar'><div class='rosterTopText'>권리 행사 대상 %d명</div></div>", index_count);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable faRightsExerciseTable'><thead><tr>"
        "<th data-sort-type='text'>상태</th>"
        "<th class='roName' data-sort-type='text'>선수</th>"
        "<th data-sort-type='text'>원소속</th>"
        "<th data-sort-type='number'>등급</th>"
        "<th class='roEntry' data-sort-type='number'>연봉</th>"
        "</tr></thead><tbody>");
    if (index_count == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='5'>해당하는 선수가 없습니다.</td></tr>");
    } else {
        for (int i = 0; i < index_count; i++) {
            const KboFaDeclarationReportRow* row = &rows[indices[i]];
            kbo_fa_rights_append_row(buffer, row);
        }
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section>");
}

void kbo_webview_append_fa_rights_exercise_view(KboWindowTextBuffer* buffer, uint32_t selected_league_id)
{
    if (buffer == NULL) {
        return;
    }

    uint32_t current_year = 0u;
    uint32_t current_month = 0u;
    uint32_t current_day = 0u;
    kbo_current_date_tick_latest_components(
        &current_year,
        &current_month,
        &current_day);

    KboFaDeclarationReportRow* rows = (KboFaDeclarationReportRow*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_DECLARATION_REPORT_MAX * sizeof(KboFaDeclarationReportRow));
    if (rows == NULL) {
        kbo_window_text_appendf(buffer, "<div class='rights rosterRights'><p>FA 권리 행사 명단을 불러올 수 없습니다.</p></div>");
        return;
    }

    char path[MAX_PATH] = {0};
    int count = kbo_load_fa_declaration_report_rows(rows, KBO_FA_DECLARATION_REPORT_MAX, path, sizeof(path));
    int indices[KBO_FA_DECLARATION_REPORT_MAX];
    int index_count = 0;
    int declared_count = 0;
    int deferred_count = 0;
    for (int i = 0; i < count && index_count < KBO_FA_DECLARATION_REPORT_MAX; i++) {
        if (rows[i].season != current_year
                || (selected_league_id != 0u && rows[i].league_id != 0u && rows[i].league_id != selected_league_id)
                || !kbo_fa_rights_report_row_latest_for_player(rows, count, i)) {
            continue;
        }
        indices[index_count++] = i;
        if (rows[i].declared) {
            declared_count++;
        } else {
            deferred_count++;
        }
    }

    kbo_window_text_appendf(
        buffer,
        "<style>"
        ".faRightsSummary{height:34px;display:flex;align-items:center;gap:18px;padding:0 8px;background:#1f1f1f;border:1px solid #303030;border-radius:4px;font-size:13px;font-weight:900;color:#f2f2f2}"
        ".faRightsStatus{display:inline-block;min-width:52px;padding:2px 7px;border-radius:3px;text-align:center;font-size:12px;font-weight:900}.faRightsStatus.declared{background:#173d22;color:#7ee08a}.faRightsStatus.deferred{background:#3c3020;color:#ffc665}"
        ".faRightsExerciseTable td:last-child{max-width:360px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        "</style><div class='rights rosterRights faRightsExercise'>"
        "<div class='faRightsSummary'><span>FA 권리 행사</span><span>대상 %d명</span><span>선언 %d명</span><span>미선언 %d명</span></div>",
        index_count,
        declared_count,
        deferred_count);

    if (count <= 0) {
        kbo_window_text_appendf(
            buffer,
            "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable'><tbody>"
            "<tr><td>아직 FA 권리 행사 기록이 없습니다. FA 선언 이벤트가 처리되면 이 탭에 선언/미선언 명단이 표시됩니다.</td></tr>"
            "</tbody></table></section></div>");
        HeapFree(GetProcessHeap(), 0, rows);
        return;
    }

    kbo_fa_rights_append_table(buffer, rows, indices, index_count);
    kbo_window_text_appendf(buffer, "</div>");
    HeapFree(GetProcessHeap(), 0, rows);
}
