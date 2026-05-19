#include "ui_cbt_view_internal.h"

void kbo_webview_append_cbt_history_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id)
{
    KboCbtRecord* records = NULL;
    int count = 0;
    if (!kbo_cbt_load_sorted(&records, &count)) return;

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);

    char team_name[96];
    kbo_hub_copy_team_display_name_by_id(selected_team_id, team_name, sizeof(team_name), "구단 없음");
    char summary[256];
    snprintf(summary, sizeof(summary), "구단 기록 - %s", team_name);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary);
    kbo_window_text_appendf(buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable'><thead><tr>"
        "<th data-sort-type='number' style='width:64px'>시즌</th>"
        "<th data-sort-type='number' style='width:92px'>총연봉</th>"
        "<th data-sort-type='number' style='width:92px'>기준선</th>"
        "<th data-sort-type='number' style='width:92px'>초과액</th>"
        "<th data-sort-type='number' style='width:54px'>세율</th>"
        "<th data-sort-type='number' style='width:92px'>납부액</th>"
        "<th data-sort-type='number' style='width:64px'>연속</th>"
        "<th data-sort-type='text' style='width:92px'>상태</th>"
        "<th data-sort-type='number' style='width:84px'>지명권</th>"
        "</tr></thead><tbody>");

    int rendered = 0;
    for (int i = 0; i < count; i++) {
        const KboCbtRecord* rec = &records[i];
        if (rec->team_id != selected_team_id) continue;
        char payroll[32], threshold[32], overage[32], tax[32];
        int draft = 0;
        const char* row_style = rec->overage > 0 ? " style='color:#e88;'" : "";
        const char* status = rec->overage > 0 ? "초과" : "정상";
        kbo_cbt_format_usd(payroll, sizeof(payroll), rec->payroll);
        kbo_cbt_format_usd(threshold, sizeof(threshold), rec->threshold);
        kbo_cbt_format_usd(overage, sizeof(overage), rec->overage);
        kbo_cbt_format_usd(tax, sizeof(tax), rec->tax_amount);
        if (kbo_cbt_is_latest_for_team(records, i)
            && rec->overage > 0
            && rec->consecutive_count >= rules.draft_penalty_min_consecutive) {
            draft = (int)rules.draft_penalty_stages;
        }

        kbo_window_text_appendf(buffer, "<tr%s><td>%u</td><td>", row_style, rec->season);
        kbo_html_append_escaped(buffer, payroll);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, threshold);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, overage);
        kbo_window_text_appendf(buffer, "</td><td>%u%%</td><td>", rec->tax_rate_pct);
        kbo_html_append_escaped(buffer, tax);
        if (draft > 0) {
            kbo_window_text_appendf(buffer,
                "</td><td>%u</td><td>%s</td><td style='color:#e88;'>-%d</td></tr>",
                rec->consecutive_count, status, draft);
        } else {
            kbo_window_text_appendf(buffer,
                "</td><td>%u</td><td>%s</td><td>-</td></tr>",
                rec->consecutive_count, status);
        }
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(buffer,
            "<tr><td colspan='9' class='roEmptyMessage'>선택한 구단의 사치세 기록이 없습니다.</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    HeapFree(GetProcessHeap(), 0, records);
}

static void kbo_cbt_copy_button_asset_src(const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) return;
    out[0] = '\0';
    char path[MAX_PATH] = {0};
    kbo_hub_skin_button_image_path(file_name, path, sizeof(path));
    kbo_webview_copy_image_src(path, out, out_size);
}

static void kbo_webview_append_cbt_exception_css(KboWindowTextBuffer* buffer)
{
    char plus_up[2048] = {0}, plus_over[2048] = {0}, plus_down[2048] = {0};
    char minus_up[2048] = {0}, minus_over[2048] = {0}, minus_down[2048] = {0};

    kbo_cbt_copy_button_asset_src("list_buttons_plus_up.png", plus_up, sizeof(plus_up));
    kbo_cbt_copy_button_asset_src("list_buttons_plus_over.png", plus_over, sizeof(plus_over));
    kbo_cbt_copy_button_asset_src("list_buttons_plus_down.png", plus_down, sizeof(plus_down));
    kbo_cbt_copy_button_asset_src("list_buttons_minus_up.png", minus_up, sizeof(minus_up));
    kbo_cbt_copy_button_asset_src("list_buttons_minus_over.png", minus_over, sizeof(minus_over));
    kbo_cbt_copy_button_asset_src("list_buttons_minus_down.png", minus_down, sizeof(minus_down));

    kbo_window_text_appendf(buffer,
        "<style>"
        ".cbtExceptionView{display:flex;flex-direction:column;gap:4px;height:100%%;min-height:0}"
        ".cbtExceptionTable .roAction{width:42px;text-align:center}.cbtExceptionTable .roRank{width:54px;text-align:right}.cbtExceptionTable .roName{width:260px}.cbtExceptionTable .roSalary{width:104px;text-align:right}.cbtExceptionTable .roCredit{width:104px;text-align:right}.cbtExceptionTable .roYears{width:84px;text-align:right}.cbtExceptionTable .roStatus{width:108px}.cbtExceptionTable .roWindow{width:96px}.cbtExceptionTable tr.selected td{color:#d6a44b!important}"
        ".cbtIconAction{display:inline-flex;align-items:center;justify-content:center;width:24px;height:24px;border:0;border-radius:0;background-color:transparent;background-repeat:no-repeat;background-position:center center;background-size:24px 24px;color:transparent;font-size:0;line-height:0;text-decoration:none;cursor:pointer}"
        ".cbtIconAction.disabled,.cbtIconAction.disabled:hover,.cbtIconAction.disabled:active{opacity:.38;cursor:default;pointer-events:none}"
        ".cbtProtectAction{background-image:url('%s')}.cbtProtectAction:hover{background-image:url('%s')}.cbtProtectAction:active{background-image:url('%s')}"
        ".cbtClearAction{background-image:url('%s')}.cbtClearAction:hover{background-image:url('%s')}.cbtClearAction:active{background-image:url('%s')}"
        ".cbtActionMuted{display:inline-flex;align-items:center;justify-content:center;width:24px;height:24px;color:#8c8c8c;font-size:12px;font-weight:900}"
        "</style>",
        plus_up, plus_over, plus_down,
        minus_up, minus_over, minus_down);
}

void kbo_webview_append_cbt_exceptions_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id)
{
    uint32_t year = 0, month = 0, day = 0;
    kbo_current_date_is_valid(&year, &month, &day);
    uint32_t current_date = year * 10000u + month * 100u + day;

    char team_name[96];
    kbo_hub_copy_team_display_name_by_id(selected_team_id, team_name, sizeof(team_name), "구단 없음");

    KboFaSalarySnapshotGrade* grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    KboCbtExceptionDesignation designations[KBO_CBT_EXCEPTION_MAX];
    int designation_count = kbo_cbt_exception_load_designations(designations, KBO_CBT_EXCEPTION_MAX);
    int grade_count = grades != NULL && year > 0
        ? kbo_fa_salary_snapshot_load_grade_rows(year, grades, KBO_FA_SALARY_SNAPSHOT_GRADE_MAX, NULL, 0)
        : 0;

    int window_open = kbo_cbt_exception_designation_window_open(year, current_date);
    int action_available = kbo_hub_ui_team_action_available(selected_team_id, "hub_cbt_exception_render");

    const KboCbtExceptionDesignation* current = NULL;
    for (int i = 0; i < designation_count; i++) {
        if (designations[i].season == year && designations[i].team_id == selected_team_id) {
            current = &designations[i];
            break;
        }
    }

    char summary[256];
    snprintf(summary, sizeof(summary), "예외 선수 - %s", team_name);
    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary);
    kbo_webview_append_cbt_exception_css(buffer);
    kbo_window_text_appendf(buffer,
        "<div class='cbtExceptionView'>"
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable cbtExceptionTable'><thead><tr>"
        "<th data-sort-type='text' class='roAction'>지정</th>"
        "<th data-sort-type='number' class='roRank'>순위</th>"
        "<th data-sort-type='text' class='roName'>선수</th>"
        "<th data-sort-type='number' class='roSalary'>연봉</th>"
        "<th data-sort-type='number' class='roCredit'>CBT 공제</th>"
        "<th data-sort-type='number' class='roYears'>시즌</th>"
        "<th data-sort-type='text' class='roStatus'>상태</th>"
        "<th data-sort-type='text' class='roWindow'>기간</th>"
        "</tr></thead><tbody>");

    int rendered = 0;
    for (int i = 0; i < grade_count; i++) {
        const KboFaSalarySnapshotGrade* grade = &grades[i];
        if (grade->ranking_team_id != selected_team_id
                || grade->foreign_flag != 0u
                || grade->salary <= 0
                || grade->player_key[0] == '\0') {
            continue;
        }
        int season_count = 0;
        if (!kbo_cbt_exception_player_eligible(selected_team_id, grade->player_key, &season_count)) {
            continue;
        }
        char salary[32];
        char credit[32];
        kbo_cbt_format_usd(salary, sizeof(salary), grade->salary);
        kbo_cbt_format_usd(credit, sizeof(credit), grade->salary / 2);
        int designated = current != NULL && strcmp(current->player_key, grade->player_key) == 0;
        kbo_window_text_appendf(
            buffer,
            "<tr%s><td class='roAction'>",
            designated ? " class='selected'" : "");
        if (window_open && action_available) {
            if (designated) {
                kbo_window_text_appendf(
                    buffer,
                    "<a class='cbtIconAction cbtClearAction' title='예외 선수 해제' href='kbo://cbt_exception/clear/%u/%u'>해제</a>",
                    year,
                    selected_team_id);
            } else {
                kbo_window_text_appendf(
                    buffer,
                    "<a class='cbtIconAction cbtProtectAction' title='예외 선수로 지정' href='kbo://cbt_exception/set/%u/%u/",
                    year,
                    selected_team_id);
                kbo_html_append_escaped(buffer, grade->player_key);
                kbo_window_text_appendf(buffer, "'>지정</a>");
            }
        } else if (window_open) {
            kbo_window_text_appendf(
                buffer,
                "<span class='cbtIconAction %s disabled' title='내가 맡은 구단이 아닙니다'>%s</span>",
                designated ? "cbtClearAction" : "cbtProtectAction",
                designated ? "해제" : "지정");
        } else {
            kbo_window_text_appendf(
                buffer,
                "<span class='cbtIconAction %s disabled' title='지정 기간이 아닙니다'>%s</span>",
                designated ? "cbtClearAction" : "cbtProtectAction",
                designated ? "해제" : "지정");
        }
        kbo_window_text_appendf(buffer, "</td><td class='roRank'>%u</td><td class='roName'>", grade->team_rank);
        kbo_html_append_escaped(buffer, grade->player_name[0] != '\0' ? grade->player_name : grade->player_key);
        kbo_window_text_appendf(buffer, "</td><td class='roSalary'>");
        kbo_html_append_escaped(buffer, salary);
        kbo_window_text_appendf(buffer, "</td><td class='roCredit'>");
        kbo_html_append_escaped(buffer, credit);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roYears'>%d</td><td class='roStatus'><strong>%s</strong></td><td class='roWindow'>%s</td></tr>",
            season_count,
            designated ? "보호" : "가능",
            window_open ? "열림" : "닫힘");
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(buffer,
            "<tr><td colspan='8' class='roEmptyMessage'>선택한 구단 스냅샷에서 예외 지정 가능 선수를 찾지 못했습니다.</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div></div>");
    if (grades != NULL) HeapFree(GetProcessHeap(), 0, grades);
}

void kbo_webview_append_cbt_rules_view(KboWindowTextBuffer* buffer)
{
    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    uint32_t current_year = 0, cm = 0, cd = 0;
    kbo_current_date_is_valid(&current_year, &cm, &cd);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, rules.enabled ? "CBT 활성화" : "CBT 비활성화");
    kbo_window_text_appendf(buffer,
        "<section class='tablewrap rosterTableWrap' style='margin-bottom:12px'>"
        "<table class='ootpRosterTable'><thead><tr>"
        "<th data-sort-type='text' style='width:240px'>설정</th><th data-sort-type='text'>값</th>"
        "</tr></thead><tbody>"
        "<tr><td>상위 선수 수</td><td>%u</td></tr>"
        "<tr><td>연간 기준선 인상률</td><td>%u%%</td></tr>"
        "<tr><td>1회 위반 세율</td><td>%u%%</td></tr>"
        "<tr><td>2회 위반 세율</td><td>%u%%</td></tr>"
        "<tr><td>3회 이상 위반 세율</td><td>%u%%</td></tr>"
        "<tr><td>지명권 페널티 최소 연속 시즌</td><td>%u</td></tr>"
        "<tr><td>지명권 페널티 단계</td><td>%u</td></tr>"
        "</tbody></table></section>",
        rules.top_player_count, rules.annual_increase_pct,
        rules.tax_rate_1, rules.tax_rate_2, rules.tax_rate_3plus,
        rules.draft_penalty_min_consecutive, rules.draft_penalty_stages);

    if (current_year > 0) {
        kbo_window_text_appendf(buffer,
            "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable'><thead><tr>"
            "<th data-sort-type='number' style='width:88px'>시즌</th>"
            "<th data-sort-type='number' style='width:120px'>기준선</th>"
            "</tr></thead><tbody>");
        uint32_t start = current_year >= 2 ? current_year - 2 : current_year;
        for (uint32_t y = start; y <= current_year + 2; y++) {
            char text[32];
            const char* row_style = (y == current_year) ? " style='color:#d6a44b'" : "";
            kbo_cbt_format_usd(text, sizeof(text), kbo_cbt_get_threshold(&rules, y));
            kbo_window_text_appendf(buffer, "<tr%s><td>%u</td><td>", row_style, y);
            kbo_html_append_escaped(buffer, text);
            kbo_window_text_appendf(buffer, y >= current_year + 1 ? " (예상)</td></tr>" : "</td></tr>");
        }
        kbo_window_text_appendf(buffer, "</tbody></table></section>");
    }

    kbo_window_text_appendf(buffer, "</div>");
}
