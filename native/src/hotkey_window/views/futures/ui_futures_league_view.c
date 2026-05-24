#include "ui_futures_league_view.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/profiling/profiler.h"
#include "../../runtime/hotkey_window_runtime_shared.h"
#include "../../support/actions/ui_team_actions.h"
#include "ui_futures_offer_snapshot.h"
#include "ui_futures_league_view_helpers.h"
#include "../../../team/independent_acquisition/ui/independent_acquisition_ui.h"

static void kbo_futures_ui_append_context_bar(
    KboWindowTextBuffer* buffer,
    const KboIndependentAcquisitionUiContext* context,
    int row_count,
    const char* mode)
{
    char open_text[16] = "-";
    char close_text[16] = "-";
    char cash_text[32] = "$0";
    char buyer_name[96] = "-";
    char summary[384] = {0};
    if (context != NULL) {
        kbo_futures_ui_format_yyyymmdd(context->open_date, open_text, sizeof(open_text));
        kbo_futures_ui_format_yyyymmdd(context->close_date, close_text, sizeof(close_text));
        kbo_futures_ui_format_cash(context->buyer_cash, cash_text, sizeof(cash_text));
        if (context->buyer_team_id != 0u) {
            kbo_futures_ui_copy_team_name(context->buyer_team_id, buyer_name, sizeof(buyer_name));
        }
        snprintf(
            summary,
            sizeof(summary),
            "보기: %s - 구매 구단: %s - 기간: %s - 창구: %s ~ %s - 보유 자금: %s - 판매 구단: %d - 표시: %d",
            mode != NULL ? mode : "-",
            buyer_name,
            context->window_open ? "열림" : "닫힘",
            open_text,
            close_text,
            cash_text,
            context->seller_count,
            row_count);
    } else {
        snprintf(summary, sizeof(summary), "보기: %s - 표시: %d", mode != NULL ? mode : "-", row_count);
    }
    kbo_webview_append_roster_top_bar(buffer, summary);
}

static void kbo_futures_ui_append_empty_row(KboWindowTextBuffer* buffer, int colspan, const char* text)
{
    kbo_window_text_appendf(buffer, "<tr><td class='roEmptyMessage' colspan='%d'>", colspan);
    kbo_html_append_escaped(buffer, text != NULL && text[0] != '\0' ? text : "-");
    kbo_window_text_appendf(buffer, "</td></tr>");
}

static void kbo_futures_ui_append_snapshot_nation_cell(
    KboWindowTextBuffer* buffer,
    const KboFuturesOfferUiSnapshotRow* row)
{
    if (buffer == NULL || row == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<td class='roNat' title='");
    kbo_html_append_escaped(buffer, row->nation_label);
    kbo_window_text_appendf(buffer, " nation#%u'><span class='roNatWrap'><img class='roNatFlag' alt='", row->nation_id);
    kbo_html_append_escaped(buffer, row->nation_abbrev);
    kbo_window_text_appendf(buffer, "' title='");
    kbo_html_append_escaped(buffer, row->nation_label);
    kbo_window_text_appendf(buffer, " nation#%u' src='", row->nation_id);
    kbo_html_append_escaped(buffer, row->nation_flag_src);
    kbo_window_text_appendf(buffer, "'><span class='roNatText'>");
    kbo_html_append_escaped(buffer, row->nation_abbrev);
    kbo_window_text_appendf(buffer, "</span></span></td>");
}

static void kbo_futures_ui_append_offer_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id)
{
    KBO_PROFILE_BEGIN(profile_futures_offer_view);
    uint32_t buyer_team_id = kbo_futures_ui_resolve_buyer_team_id(selected_team_id);
    KboFuturesOfferUiSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    int updating = 0;
    KBO_PROFILE_BEGIN(profile_futures_offer_collect);
    int has_snapshot = kbo_futures_offer_ui_snapshot_get(
        buyer_team_id,
        &snapshot,
        &updating);
    int count = has_snapshot ? snapshot.count : 0;
    KboIndependentAcquisitionUiContext* context = &snapshot.context;
    int action_available = kbo_hub_ui_team_action_available(
        buyer_team_id,
        "hub_independent_acquisition_offer_render");
    KBO_PROFILE_END(profile_futures_offer_collect, "webview.futures.offer.collect");
    kbo_profiler_record_us("webview.futures.offer.rows", (unsigned long long)count);

    KBO_PROFILE_BEGIN(profile_futures_offer_render);
    kbo_window_text_appendf(buffer, "<div class='rights rosterRights futuresRights'>");
    kbo_futures_ui_append_context_bar(buffer, context, count, updating ? "제안 업데이트 중" : "제안");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable futuresOfferTable'><thead><tr>"
        "<th class='roAction'>제안</th><th class='roPo' data-sort-type='text'>포지션</th>"
        "<th class='roName' data-sort-type='text'>선수</th><th class='roClub' data-sort-type='text'>독립 구단</th>"
        "<th class='roNat' data-sort-type='text'>국적</th><th class='roAge' data-sort-type='number'>나이</th>"
        "<th class='roSlot' data-sort-type='text'>분류</th><th class='roCash' data-sort-type='number'>비용</th>"
        "<th class='roStatus' data-sort-type='text'>상태</th>"
        "</tr></thead><tbody>");

    if (!has_snapshot && updating && buyer_team_id != 0u) {
        kbo_futures_ui_append_empty_row(buffer, 9, "독립 구단 영입 후보를 준비 중입니다.");
    } else if (!context->policy_enabled) {
        kbo_futures_ui_append_empty_row(buffer, 9, "커스텀 외국인 정책이 비활성화되어 있습니다.");
    } else if (!context->buyer_valid) {
        kbo_futures_ui_append_empty_row(buffer, 9, "먼저 KBO 구단을 선택하세요.");
    } else if (context->seller_count <= 0) {
        kbo_futures_ui_append_empty_row(buffer, 9, "시드 파일에서 독립 구단을 찾지 못했습니다.");
    } else if (count <= 0) {
        kbo_futures_ui_append_empty_row(buffer, 9, "이 구단에 표시할 독립 구단 선수가 없습니다.");
    }

    for (int i = 0; i < count; i++) {
        KboFuturesOfferUiSnapshotRow* row = &snapshot.rows[i];
        char status[32] = "가능";
        if (row->status_label[0] != '\0') {
            snprintf(status, sizeof(status), "%s", row->status_label);
        }
        if (row->already_decided) {
            snprintf(status, sizeof(status), "결과");
        } else if (row->already_requested) {
            snprintf(status, sizeof(status), "대기");
        } else if (!context->window_open) {
            snprintf(status, sizeof(status), "마감");
        } else if (!action_available) {
            snprintf(status, sizeof(status), "권한 없음");
        }

        kbo_window_text_appendf(buffer, "<tr><td class='roAction'><span class='rightsActions'>");
        if (context->window_open
                && action_available
                && !row->offer_blocked
                && !row->already_requested
                && !row->already_decided) {
            kbo_window_text_appendf(
                buffer,
                "<a class='rightsAction rightsAdd' title='영입 제안' href='kbo://futures-offer/submit/%u/%u/%u'>제안</a>",
                buyer_team_id,
                row->seller_team_id,
                row->player_id);
        } else {
            kbo_window_text_appendf(buffer, "<span class='rightsAction rightsAdd disabled' title='");
            kbo_html_append_escaped(buffer, status);
            kbo_window_text_appendf(buffer, "'>제안</span>");
        }
        kbo_window_text_appendf(buffer, "</span></td><td class='roPo'>");
        kbo_html_append_escaped(buffer, row->position_label);
        kbo_window_text_appendf(buffer, "</td>");
        kbo_webview_append_player_name_cell(buffer, row->player_name, row->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, row->seller_name);
        kbo_window_text_appendf(buffer, "</td>");
        kbo_futures_ui_append_snapshot_nation_cell(buffer, row);
        kbo_window_text_appendf(
            buffer,
            "<td class='roAge'>%u</td><td class='roSlot'>",
            (uint32_t)row->age);
        kbo_html_append_escaped(buffer, row->slot_label);
        kbo_window_text_appendf(buffer, "</td><td class='roCash' data-sort-value='%d'>", row->cash_cost);
        kbo_html_append_escaped(buffer, row->cash_text);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roStatus'>");
        kbo_html_append_escaped(buffer, status);
        kbo_window_text_appendf(buffer, "</td></tr>");
    }

    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    KBO_PROFILE_END(profile_futures_offer_render, "webview.futures.offer.render");
    KBO_PROFILE_END(profile_futures_offer_view, "webview.futures.offer.total");
}

static void kbo_futures_ui_append_pending_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id)
{
    KBO_PROFILE_BEGIN(profile_futures_pending_view);
    uint32_t buyer_team_id = kbo_futures_ui_resolve_buyer_team_id(selected_team_id);
    KboIndependentAcquisitionUiRequestRow rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS];
    KboIndependentAcquisitionUiContext context;
    KBO_PROFILE_BEGIN(profile_futures_pending_collect);
    kbo_independent_acquisition_ui_context(buyer_team_id, &context);
    int action_available = kbo_hub_ui_team_action_available(
        buyer_team_id,
        "hub_independent_acquisition_cancel_render");
    int count = kbo_independent_acquisition_ui_load_pending_rows(
        buyer_team_id,
        rows,
        KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS);
    KBO_PROFILE_END(profile_futures_pending_collect, "webview.futures.pending.collect");
    kbo_profiler_record_us("webview.futures.pending.rows", (unsigned long long)count);

    KBO_PROFILE_BEGIN(profile_futures_pending_render);
    kbo_window_text_appendf(buffer, "<div class='rights rosterRights futuresRights'>");
    kbo_futures_ui_append_context_bar(buffer, &context, count, "대기");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable futuresPendingTable'><thead><tr>"
        "<th class='roAction'>취소</th><th class='roDate' data-sort-type='number'>일자</th><th class='roPo' data-sort-type='text'>포지션</th>"
        "<th class='roName' data-sort-type='text'>선수</th><th class='roClub' data-sort-type='text'>독립 구단</th>"
        "<th class='roNat' data-sort-type='text'>국적</th><th class='roSlot' data-sort-type='text'>분류</th>"
        "<th class='roCash' data-sort-type='number'>비용</th><th class='roStatus' data-sort-type='text'>상태</th>"
        "</tr></thead><tbody>");

    if (!context.buyer_valid) {
        kbo_futures_ui_append_empty_row(buffer, 9, "먼저 KBO 구단을 선택하세요.");
    } else if (count <= 0) {
        kbo_futures_ui_append_empty_row(buffer, 9, "대기 중인 독립 구단 영입 제안이 없습니다.");
    }
    for (int i = 0; i < count; i++) {
        KboIndependentAcquisitionUiRequestRow* row = &rows[i];
        char date_text[16] = {0};
        char player_name[96] = {0};
        char seller_name[96] = {0};
        char cash_text[32] = {0};
        kbo_futures_ui_format_yyyymmdd(row->date, date_text, sizeof(date_text));
        kbo_futures_ui_copy_player_name(row->player_ptr, row->player_id, player_name, sizeof(player_name));
        kbo_futures_ui_copy_team_name(row->seller_team_id, seller_name, sizeof(seller_name));
        kbo_futures_ui_format_cash(row->cash_cost, cash_text, sizeof(cash_text));

        kbo_window_text_appendf(
            buffer,
            "<tr><td class='roAction'><span class='rightsActions'>");
        if (action_available) {
            kbo_window_text_appendf(
                buffer,
                "<a class='rightsAction rightsCancel' title='영입 제안 취소' href='kbo://futures-offer/cancel/%u/%u/%u'>취소</a>",
                buyer_team_id,
                row->seller_team_id,
                row->player_id);
        } else {
            kbo_window_text_appendf(
                buffer,
                "<span class='rightsAction rightsCancel disabled' title='내가 맡은 구단이 아닙니다'>취소</span>");
        }
        kbo_window_text_appendf(
            buffer,
            "</span></td><td class='roDate' data-sort-value='%u'>",
            row->date);
        kbo_html_append_escaped(buffer, date_text);
        kbo_window_text_appendf(buffer, "</td><td class='roPo'>%s</td>", kbo_futures_ui_position_label(row->player_ptr));
        kbo_webview_append_player_name_cell(buffer, player_name, row->player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, seller_name);
        kbo_window_text_appendf(buffer, "</td>");
        kbo_webview_append_roster_nation_cell(buffer, row->nation_id, kbo_hub_nation_flag_asset_path);
        kbo_window_text_appendf(buffer, "<td class='roSlot'>");
        kbo_html_append_escaped(buffer, row->slot_label);
        kbo_window_text_appendf(buffer, "</td><td class='roCash' data-sort-value='%d'>", row->cash_cost);
        kbo_html_append_escaped(buffer, cash_text);
        kbo_window_text_appendf(
            buffer,
            "</td><td class='roStatus'>대기</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    KBO_PROFILE_END(profile_futures_pending_render, "webview.futures.pending.render");
    KBO_PROFILE_END(profile_futures_pending_view, "webview.futures.pending.total");
}

static const char* kbo_futures_ui_result_label(const KboIndependentAcquisitionUiResultRow* row)
{
    if (row == NULL) {
        return "-";
    }
    if (row->winning_buyer_team_id == row->request.buyer_team_id && row->transferred) {
        return "영입";
    }
    if (row->winning_buyer_team_id == row->request.buyer_team_id) {
        return "실패";
    }
    return "경합 패배";
}

static void kbo_futures_ui_append_result_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id)
{
    KBO_PROFILE_BEGIN(profile_futures_result_view);
    uint32_t buyer_team_id = kbo_futures_ui_resolve_buyer_team_id(selected_team_id);
    KboIndependentAcquisitionUiResultRow rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS];
    KboIndependentAcquisitionUiContext context;
    KBO_PROFILE_BEGIN(profile_futures_result_collect);
    kbo_independent_acquisition_ui_context(buyer_team_id, &context);
    int count = kbo_independent_acquisition_ui_load_result_rows(
        buyer_team_id,
        rows,
        KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS);
    KBO_PROFILE_END(profile_futures_result_collect, "webview.futures.result.collect");
    kbo_profiler_record_us("webview.futures.result.rows", (unsigned long long)count);

    KBO_PROFILE_BEGIN(profile_futures_result_render);
    kbo_window_text_appendf(buffer, "<div class='rights rosterRights futuresRights'>");
    kbo_futures_ui_append_context_bar(buffer, &context, count, "결과");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable futuresResultTable'><thead><tr>"
        "<th class='roDate' data-sort-type='number'>결정일</th><th class='roResult' data-sort-type='text'>결과</th>"
        "<th class='roPo' data-sort-type='text'>포지션</th><th class='roName' data-sort-type='text'>선수</th>"
        "<th class='roClub' data-sort-type='text'>독립 구단</th><th class='roTeam' data-sort-type='text'>낙찰 구단</th>"
        "<th class='roCash' data-sort-type='number'>비용</th>"
        "</tr></thead><tbody>");

    if (!context.buyer_valid) {
        kbo_futures_ui_append_empty_row(buffer, 7, "먼저 KBO 구단을 선택하세요.");
    } else if (count <= 0) {
        kbo_futures_ui_append_empty_row(buffer, 7, "독립 구단 영입 결과가 없습니다.");
    }
    for (int i = 0; i < count; i++) {
        KboIndependentAcquisitionUiResultRow* row = &rows[i];
        char date_text[16] = {0};
        char player_name[96] = {0};
        char seller_name[96] = {0};
        char winner_name[96] = {0};
        char cash_text[32] = {0};
        const char* result = kbo_futures_ui_result_label(row);
        kbo_futures_ui_format_yyyymmdd(row->decision_date, date_text, sizeof(date_text));
        kbo_futures_ui_copy_player_name(row->request.player_ptr, row->request.player_id, player_name, sizeof(player_name));
        kbo_futures_ui_copy_team_name(row->request.seller_team_id, seller_name, sizeof(seller_name));
        kbo_futures_ui_copy_team_name(row->winning_buyer_team_id, winner_name, sizeof(winner_name));
        kbo_futures_ui_format_cash(row->request.cash_cost, cash_text, sizeof(cash_text));

        kbo_window_text_appendf(
            buffer,
            "<tr><td class='roDate' data-sort-value='%u'>",
            row->decision_date);
        kbo_html_append_escaped(buffer, date_text);
        kbo_window_text_appendf(buffer, "</td><td class='roResult'>");
        kbo_html_append_escaped(buffer, result);
        kbo_window_text_appendf(buffer, "</td><td class='roPo'>%s</td>", kbo_futures_ui_position_label(row->request.player_ptr));
        kbo_webview_append_player_name_cell(buffer, player_name, row->request.player_id);
        kbo_window_text_appendf(buffer, "<td class='roClub'>");
        kbo_html_append_escaped(buffer, seller_name);
        kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
        kbo_html_append_escaped(buffer, winner_name);
        kbo_window_text_appendf(buffer, "</td><td class='roCash' data-sort-value='%d'>", row->request.cash_cost);
        kbo_html_append_escaped(buffer, cash_text);
        kbo_window_text_appendf(
            buffer,
            "</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    KBO_PROFILE_END(profile_futures_result_render, "webview.futures.result.render");
    KBO_PROFILE_END(profile_futures_result_view, "webview.futures.result.total");
}

void kbo_webview_append_futures_league_view(
    KboWindowTextBuffer* buffer,
    int selected_futures_subview,
    uint32_t selected_team_id)
{
    if (buffer == NULL) {
        return;
    }
    KBO_PROFILE_BEGIN(profile_futures_view);
    if (selected_futures_subview == KBO_HUB_FUTURES_SUBVIEW_PENDING) {
        kbo_futures_ui_append_pending_view(buffer, selected_team_id);
        KBO_PROFILE_END(profile_futures_view, "webview.futures.total");
        return;
    }
    if (selected_futures_subview == KBO_HUB_FUTURES_SUBVIEW_RESULT) {
        kbo_futures_ui_append_result_view(buffer, selected_team_id);
        KBO_PROFILE_END(profile_futures_view, "webview.futures.total");
        return;
    }
    kbo_futures_ui_append_offer_view(buffer, selected_team_id);
    KBO_PROFILE_END(profile_futures_view, "webview.futures.total");
}
