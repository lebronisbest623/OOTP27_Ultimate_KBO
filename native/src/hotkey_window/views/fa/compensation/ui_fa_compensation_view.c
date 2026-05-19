#include "../ui_fa_views_internal.h"

int kbo_fa_compensation_record_is_final(const KboFaCompensationRecord* rec)
{
    if (rec == NULL) {
        return 0;
    }
    return rec->status == KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED
        || rec->status == KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED;
}

int kbo_fa_compensation_debug_row_count(
    const KboFaCompensationProtectionDebugRow* rows,
    int count,
    uint32_t fa_player_id)
{
    int row_count = 0;
    if (rows == NULL || count <= 0 || fa_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (rows[i].fa_player_id == fa_player_id) {
            row_count++;
        }
    }
    return row_count;
}

const char* kbo_fa_compensation_decision_label(
    const KboFaCompensationRecord* rec,
    const KboFaCompensationDecisionRow* decision,
    int has_decision)
{
    if (has_decision && decision != NULL) {
        if (strcmp(decision->action, "CASH_ONLY") == 0) {
            return "현금 보상";
        }
        if (decision->selected_player_name[0] != '\0') {
            return decision->selected_player_name;
        }
        return "선수+현금";
    }
    if (rec != NULL && rec->requires_player_compensation && rec->protect_count > 0u) {
        return "보호 명단 대기";
    }
    return "현금 보상";
}

static const char* kbo_fa_compensation_subview_label(int subview)
{
    switch (subview) {
    case KBO_HUB_FA_COMP_SUBVIEW_LEDGER:     return "장부";
    case KBO_HUB_FA_COMP_SUBVIEW_BOARD:      return "보상 보드";
    case KBO_HUB_FA_COMP_SUBVIEW_CANDIDATES: return "선수 후보";
    default:                                 return "";
    }
}

static void kbo_webview_append_fa_compensation_subtabs(KboWindowTextBuffer* buffer, int selected_subview)
{
    kbo_window_text_appendf(buffer, "<nav class='faCompTabs'>");
    for (int i = 0; i < KBO_HUB_FA_COMP_SUBVIEW_COUNT; i++) {
        kbo_window_text_appendf(
            buffer,
            "<a class='faCompTab %s' href='kbo://fa-comp/view/%d'>",
            i == selected_subview ? "active" : "",
            i);
        kbo_html_append_escaped(buffer, kbo_fa_compensation_subview_label(i));
        kbo_window_text_appendf(buffer, "</a>");
    }
    kbo_window_text_appendf(buffer, "</nav>");
}

static void kbo_webview_append_fa_compensation_text_action(
    KboWindowTextBuffer* buffer,
    int enabled,
    const char* class_name,
    const char* title,
    const char* href,
    const char* label)
{
    if (enabled) {
        kbo_window_text_appendf(buffer, "<a class='rightsTextAction");
        if (class_name != NULL && class_name[0] != '\0') {
            kbo_window_text_appendf(buffer, " ");
            kbo_html_append_escaped(buffer, class_name);
        }
        kbo_window_text_appendf(buffer, "'");
        if (title != NULL && title[0] != '\0') {
            kbo_window_text_appendf(buffer, " title='");
            kbo_html_append_escaped(buffer, title);
            kbo_window_text_appendf(buffer, "'");
        }
        kbo_window_text_appendf(buffer, " href='");
        kbo_html_append_escaped(buffer, href);
        kbo_window_text_appendf(buffer, "'>");
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer, "</a>");
    } else {
        kbo_window_text_appendf(buffer, "<span class='rightsTextAction disabled");
        if (class_name != NULL && class_name[0] != '\0') {
            kbo_window_text_appendf(buffer, " ");
            kbo_html_append_escaped(buffer, class_name);
        }
        kbo_window_text_appendf(buffer, "' title='내가 맡은 구단이 아닙니다'>");
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer, "</span>");
    }
}

void kbo_webview_append_fa_compensation_view(
    KboWindowTextBuffer* buffer,
    int selected_compensation_subview,
    uint32_t selected_compensation_player_id)
{
    if (buffer == NULL) {
        return;
    }
    if (selected_compensation_subview < 0
            || selected_compensation_subview >= KBO_HUB_FA_COMP_SUBVIEW_COUNT) {
        selected_compensation_subview = KBO_HUB_FA_COMP_SUBVIEW_LEDGER;
    }

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    KboFaCompensationProtectionDebugRow* debug_rows = (KboFaCompensationProtectionDebugRow*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaCompensationProtectionDebugRow));
    if (records == NULL || debug_rows == NULL) {
        kbo_window_text_appendf(buffer, "<div class='rights rosterRights'><section class='tablewrap rosterTableWrap'>보상 버퍼를 할당하지 못했습니다.</section></div>");
        if (records != NULL) { HeapFree(GetProcessHeap(), 0, records); }
        if (debug_rows != NULL) { HeapFree(GetProcessHeap(), 0, debug_rows); }
        return;
    }

    char path[MAX_PATH] = {0};
    int count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    int debug_count = kbo_load_fa_compensation_protection_debug_rows(debug_rows, KBO_FA_COMPENSATION_PROTECTED_LIST_MAX);
    KboFaCompensationRecord* detail_rec = NULL;

    if (selected_compensation_player_id != 0u) {
        for (int i = 0; i < count; i++) {
            if (records[i].player_id == selected_compensation_player_id
                    && records[i].requires_player_compensation
                    && records[i].protect_count > 0u) {
                detail_rec = &records[i];
                break;
            }
        }
    }
    for (int i = 0; i < count && detail_rec == NULL; i++) {
        if (records[i].requires_player_compensation && records[i].protect_count > 0u
                && !kbo_fa_compensation_record_is_final(&records[i])) {
            detail_rec = &records[i];
        }
    }
    for (int i = 0; i < count && detail_rec == NULL; i++) {
        if (records[i].requires_player_compensation && records[i].protect_count > 0u) {
            detail_rec = &records[i];
        }
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights faCompensation'>");
    kbo_webview_append_fa_compensation_subtabs(buffer, selected_compensation_subview);

    if (selected_compensation_subview == KBO_HUB_FA_COMP_SUBVIEW_BOARD && detail_rec != NULL) {
        KboFaCompensationDecisionRow board_decision;
        int board_has_decision = kbo_load_latest_fa_compensation_decision(detail_rec->player_id, &board_decision);
        int board_detail_rows = kbo_fa_compensation_debug_row_count(debug_rows, debug_count, detail_rec->player_id);
        int board_is_final = kbo_fa_compensation_record_is_final(detail_rec);
        char original_team[16] = "-";
        char signing_team[16] = "-";
        char previous_salary[32] = "-";
        char cash_with_player[32] = "-";
        char cash_only[32] = "-";

        kbo_hub_copy_team_abbrev_by_id(detail_rec->original_team_id, original_team, sizeof(original_team), "-");
        kbo_hub_copy_team_abbrev_by_id(detail_rec->signing_team_id, signing_team, sizeof(signing_team), "-");
        kbo_fa_market_format_salary(detail_rec->previous_salary, previous_salary, sizeof(previous_salary));
        kbo_fa_market_format_salary((int32_t)detail_rec->cash_with_player, cash_with_player, sizeof(cash_with_player));
        kbo_fa_market_format_salary((int32_t)detail_rec->cash_only, cash_only, sizeof(cash_only));

        kbo_window_text_appendf(buffer, "<section class='faCompBoard'>");
        kbo_window_text_appendf(buffer, "<div class='faCompBoardLead'><div class='faCompBoardTitle'>");
        kbo_html_append_escaped(buffer, detail_rec->player_name);
        kbo_window_text_appendf(buffer, "<span class='faCompBadge'>");
        kbo_html_append_escaped(buffer, kbo_fa_market_display_grade(detail_rec->grade));
        kbo_window_text_appendf(buffer, "</span></div><div class='faCompBoardSummary'>");
        if (detail_rec->requires_player_compensation && detail_rec->protect_count > 0u) {
            kbo_window_text_appendf(buffer, "이 FA 계약은 현금 보상과 보상 선수 1명이 필요합니다. 현금 보상을 선택하면 선수 보상은 제외됩니다.");
        } else {
            kbo_window_text_appendf(buffer, "이 FA 계약은 현금 보상이 필요합니다.");
        }
        kbo_window_text_appendf(buffer, "</div></div>");

        kbo_window_text_appendf(buffer, "<div class='faCompBoardPanels'>");

        kbo_window_text_appendf(buffer, "<article class='faCompPanel'><h3>FA 선수</h3><dl>");
        kbo_window_text_appendf(buffer, "<dt>선수</dt><dd>");
        kbo_html_append_escaped(buffer, detail_rec->player_name);
        kbo_window_text_appendf(buffer, "</dd><dt>등급</dt><dd>");
        kbo_html_append_escaped(buffer, kbo_fa_market_display_grade(detail_rec->grade));
        kbo_window_text_appendf(buffer, "</dd><dt>직전 연봉</dt><dd>");
        kbo_html_append_escaped(buffer, previous_salary);
        kbo_window_text_appendf(buffer, "</dd><dt>상태</dt><dd>");
        kbo_html_append_escaped(buffer, kbo_fa_compensation_status_label(detail_rec->status));
        kbo_window_text_appendf(buffer, "</dd></dl></article>");

        kbo_window_text_appendf(buffer, "<article class='faCompPanel faCompPanelFocus'><h3>보상</h3><dl>");
        kbo_window_text_appendf(buffer, "<dt>선수+현금</dt><dd>");
        kbo_html_append_escaped(buffer, cash_with_player);
        kbo_window_text_appendf(buffer, "</dd><dt>현금 보상</dt><dd>");
        kbo_html_append_escaped(buffer, cash_only);
        kbo_window_text_appendf(buffer, "</dd><dt>보호 명단</dt><dd>%u명</dd><dt>결정</dt><dd>", detail_rec->protect_count);
        kbo_html_append_escaped(buffer, kbo_fa_compensation_decision_label(detail_rec, &board_decision, board_has_decision));
        kbo_window_text_appendf(buffer, "</dd></dl></article>");

        kbo_window_text_appendf(buffer, "<article class='faCompPanel'><h3>구단 영향</h3><dl>");
        kbo_window_text_appendf(buffer, "<dt>원 소속</dt><dd>");
        kbo_html_append_escaped(buffer, original_team);
        kbo_window_text_appendf(buffer, "</dd><dt>계약 구단</dt><dd>");
        kbo_html_append_escaped(buffer, signing_team);
        kbo_window_text_appendf(buffer, "</dd><dt>보드</dt><dd>");
        if (board_is_final) {
            kbo_window_text_appendf(buffer, "완료");
        } else if (board_detail_rows <= 0) {
            kbo_window_text_appendf(buffer, "보호 명단 필요");
        } else {
            kbo_window_text_appendf(buffer, "결정 가능");
        }
        kbo_window_text_appendf(buffer, "</dd><dt>이동</dt><dd>");
        kbo_html_append_escaped(buffer, signing_team);
        kbo_window_text_appendf(buffer, " -> ");
        kbo_html_append_escaped(buffer, original_team);
        kbo_window_text_appendf(buffer, "</dd></dl></article>");

        kbo_window_text_appendf(buffer, "</div><div class='faCompActionBar'><span>");
        if (board_is_final) {
            kbo_window_text_appendf(buffer, "이 보상 건은 완료되었습니다.");
        } else if (board_detail_rows <= 0) {
            kbo_window_text_appendf(buffer, "보상 방식을 선택하기 전에 계약 구단의 보호 명단을 제출하세요.");
        } else {
            kbo_window_text_appendf(buffer, "현금 보상을 선택하거나 아래 목록에서 보상 선수를 선택하세요.");
        }
        kbo_window_text_appendf(buffer, "</span><div>");
        if (board_is_final) {
            kbo_window_text_appendf(buffer, "<span class='faCompFinal'>완료</span>");
        } else if (board_detail_rows <= 0) {
            char href[80];
            snprintf(href, sizeof(href), "kbo://fa-comp/submit/%u", detail_rec->player_id);
            kbo_webview_append_fa_compensation_text_action(
                buffer,
                kbo_hub_ui_team_action_available(detail_rec->signing_team_id, "hub_protected_list_submit_render"),
                "",
                "",
                href,
                "명단 제출");
        } else {
            char href[80];
            snprintf(href, sizeof(href), "kbo://fa-comp/cash-only/%u", detail_rec->player_id);
            kbo_webview_append_fa_compensation_text_action(
                buffer,
                kbo_hub_ui_team_action_available(detail_rec->original_team_id, "hub_cash_only_select_render"),
                "cashOnly",
                "",
                href,
                "현금 보상");
        }
        kbo_window_text_appendf(buffer, "</div></div></section>");
    } else if (selected_compensation_subview == KBO_HUB_FA_COMP_SUBVIEW_BOARD) {
        kbo_window_text_appendf(
            buffer,
            "<section class='faCompBoard faCompBoardEmpty'><div class='faCompBoardLead'><div class='faCompBoardTitle'>FA 보상</div>"
            "<div class='faCompBoardSummary'>검토할 선수 보상 건이 없습니다.</div></div></section>");
    }

    if (selected_compensation_subview == KBO_HUB_FA_COMP_SUBVIEW_LEDGER) {
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap faCompLedgerWrap'><table class='ootpRosterTable faCompTable'><thead><tr>"
        "<th data-sort-type='number'>일자</th>"
        "<th class='roName' data-sort-type='text'>FA</th>"
        "<th data-sort-type='number'>등급</th>"
        "<th data-sort-type='text'>원 소속</th>"
        "<th data-sort-type='text'>계약 구단</th>"
        "<th class='roEntry' data-sort-type='number'>직전 연봉</th>"
        "<th class='roEntry' data-sort-type='number'>선수+현금</th>"
        "<th class='roEntry' data-sort-type='number'>현금 보상</th>"
        "<th class='roStatus' data-sort-type='text'>상태</th>"
        "<th class='roAction' data-sort-type='text'>처리</th>"
        "</tr></thead><tbody>");

    for (int i = 0; i < count && i < 800; i++) {
        KboFaCompensationRecord* rec = &records[i];
        char original_team[16] = "-";
        char signing_team[16] = "-";
        char previous_salary[32] = "-";
        char cash_with_player[32] = "-";
        char cash_only[32] = "-";
        const char* grade_display = kbo_fa_market_display_grade(rec->grade);
        uint32_t grade_sort_rank = kbo_fa_market_display_grade_sort_rank(rec->grade);
        int row_selected = detail_rec != NULL && detail_rec->player_id == rec->player_id;

        kbo_hub_copy_team_abbrev_by_id(rec->original_team_id, original_team, sizeof(original_team), "-");
        kbo_hub_copy_team_abbrev_by_id(rec->signing_team_id, signing_team, sizeof(signing_team), "-");
        kbo_fa_market_format_salary(rec->previous_salary, previous_salary, sizeof(previous_salary));
        kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_with_player, sizeof(cash_with_player));
        kbo_fa_market_format_salary((int32_t)rec->cash_only, cash_only, sizeof(cash_only));

        kbo_window_text_appendf(buffer, row_selected ? "<tr class='selected'>" : "<tr>");
        kbo_window_text_appendf(buffer, "<td data-sort-value='%u'>%u</td>", rec->signed_on_yyyymmdd, rec->signed_on_yyyymmdd);
        kbo_webview_append_player_name_cell(buffer, rec->player_name, rec->player_id);
        kbo_window_text_appendf(buffer, "<td data-sort-value='%u'>", grade_sort_rank);
        kbo_html_append_escaped(buffer, grade_display);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, original_team);
        kbo_window_text_appendf(buffer, "</td><td>");
        kbo_html_append_escaped(buffer, signing_team);
        kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%d'>", rec->previous_salary);
        kbo_html_append_escaped(buffer, previous_salary);
        kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%u'>", rec->cash_with_player);
        kbo_html_append_escaped(buffer, cash_with_player);
        kbo_window_text_appendf(buffer, "</td><td class='roEntry' data-sort-value='%u'>", rec->cash_only);
        kbo_html_append_escaped(buffer, cash_only);
        kbo_window_text_appendf(buffer, "</td><td class='roStatus'>");
        kbo_html_append_escaped(buffer, kbo_fa_compensation_status_label(rec->status));
        kbo_window_text_appendf(buffer, "</td><td class='roAction'>");

        if (rec->requires_player_compensation && rec->protect_count > 0u) {
            if (rec->status == KBO_FA_COMPENSATION_STATUS_PENDING
                    || rec->status == KBO_FA_COMPENSATION_STATUS_RECORDED) {
                char href[80];
                snprintf(href, sizeof(href), "kbo://fa-comp/submit/%u", rec->player_id);
                kbo_webview_append_fa_compensation_text_action(
                    buffer,
                    kbo_hub_ui_team_action_available(rec->signing_team_id, "hub_protected_list_submit_render"),
                    "",
                    "보호 명단 제출",
                    href,
                    "제출");
            } else {
                kbo_window_text_appendf(buffer, "<a class='rightsTextAction' title='보상 보드 열기' href='kbo://fa-comp/detail/%u'>열기</a>", rec->player_id);
            }
        } else {
            kbo_html_append_escaped(buffer, "-");
        }
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    if (count == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='10' class='roEmptyMessage'>기록된 KBO FA 보상 의무가 없습니다.</td></tr>");
    } else if (count > 800) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='10' class='roEmptyMessage'>출력이 일부만 표시됩니다. 전체 내역은 fa_compensation.csv에서 확인하세요.</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section>");
    }

    if (selected_compensation_subview == KBO_HUB_FA_COMP_SUBVIEW_CANDIDATES && detail_rec != NULL) {
        KboFaCompensationDecisionRow decision;
        int has_decision = kbo_load_latest_fa_compensation_decision(detail_rec->player_id, &decision);
        int is_final = kbo_fa_compensation_record_is_final(detail_rec);

        kbo_window_text_appendf(
            buffer,
            "<section class='tablewrap rosterTableWrap faCompLists'><table class='ootpRosterTable faCompListTable'><thead><tr>"
            "<th class='faCompPool' data-sort-type='text'>구분</th>"
            "<th class='faCompRank' data-sort-type='number'>순위</th>"
            "<th class='roName' data-sort-type='text'>선수</th>"
            "<th class='faCompAge' data-sort-type='number'>나이</th>"
            "<th class='roAction' data-sort-type='text'>결정</th>"
            "</tr></thead><tbody>");

        int rendered = 0;
        static const char* list_order[] = { "auto_protected", "protected", "unprotected" };
        for (int list_index = 0; list_index < 3; list_index++) {
            const char* list_type = list_order[list_index];
            for (int i = 0; i < debug_count; i++) {
                KboFaCompensationProtectionDebugRow* row = &debug_rows[i];
                if (row->fa_player_id != detail_rec->player_id || strcmp(row->list_type, list_type) != 0) {
                    continue;
                }
                const char* label = strcmp(row->list_type, "auto_protected") == 0
                    ? "자동 보호" : (strcmp(row->list_type, "protected") == 0 ? "보호" : "선택 가능");
                int selected_player = has_decision
                    && strcmp(decision.action, "PLAYER") == 0
                    && row->player_id == decision.selected_player_id;

                kbo_window_text_appendf(buffer, selected_player ? "<tr class='selected'>" : "<tr>");
                kbo_window_text_appendf(buffer, "<td class='faCompPool'>");
                kbo_html_append_escaped(buffer, label);
                kbo_window_text_appendf(buffer, "</td><td class='faCompRank' data-sort-value='%u'>%u</td>", row->rank, row->rank);
                kbo_webview_append_player_name_cell(buffer, row->player_name, row->player_id);
                kbo_window_text_appendf(
                    buffer,
                    "<td class='faCompAge' data-sort-value='%u'>%u</td><td class='roAction'>",
                    (uint32_t)row->age,
                    (uint32_t)row->age);
                if (selected_player) {
                    kbo_window_text_appendf(buffer, "<span class='faCompPick'>선택됨</span>");
                } else if (!is_final && strcmp(row->list_type, "unprotected") == 0) {
                    char href[96];
                    snprintf(href, sizeof(href), "kbo://fa-comp/select/%u/%u", detail_rec->player_id, row->player_id);
                    kbo_webview_append_fa_compensation_text_action(
                        buffer,
                        kbo_hub_ui_team_action_available(detail_rec->original_team_id, "hub_manual_compensation_select_render"),
                        "",
                        "",
                        href,
                        "선택");
                } else {
                    kbo_html_append_escaped(buffer, "-");
                }
                kbo_window_text_appendf(buffer, "</td></tr>");
                rendered++;
            }
        }
        if (rendered == 0) {
            kbo_window_text_appendf(buffer, "<tr><td colspan='5' class='roEmptyMessage'>보호 명단이 아직 제출되지 않았습니다.</td></tr>");
        }
        kbo_window_text_appendf(buffer, "</tbody></table></section>");
    } else if (selected_compensation_subview == KBO_HUB_FA_COMP_SUBVIEW_CANDIDATES) {
        kbo_window_text_appendf(buffer, "<section class='tablewrap rosterTableWrap faCompLists'><table class='ootpRosterTable'><tbody><tr><td class='roEmptyMessage'>사용 가능한 선수 보상 보드가 없습니다.</td></tr></tbody></table></section>");
    }

    kbo_window_text_appendf(buffer, "</div>");
    HeapFree(GetProcessHeap(), 0, debug_rows);
    HeapFree(GetProcessHeap(), 0, records);
}

