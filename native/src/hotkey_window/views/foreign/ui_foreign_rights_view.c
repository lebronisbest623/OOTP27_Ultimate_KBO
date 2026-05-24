#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../foreign/waiver_core/api/foreign_waiver_core.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/rights/query/foreign_waiver_rights_query.h"
#include "../../../military_service/selection/events/military_selection_event.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../support/assets/names/support_names.h"
#include "../../support/assets/paths/ui_asset_paths.h"
#include "ui_foreign_rights_view.h"
#include "../../support/assets/nations/ui_nation_helpers.h"
#include "../../support/roster/cells/ui_roster_cells.h"
#include "../../support/text/buffer/ui_text_buffer.h"
#include "../../support/assets/names/ui_uniform_numbers.h"
#include "../../support/actions/ui_team_actions.h"
#include "../../ui_html_helpers/position_helpers.h"
#include "ui_foreign_rights_snapshot.h"

static void kbo_webview_append_rights_action(
    KboWindowTextBuffer* buffer,
    const char* class_name,
    const char* command,
    const char* title,
    const char* disabled_title,
    const char* symbol,
    uint32_t player_id,
    const char* player_name,
    int enabled)
{
    if (enabled) {
        kbo_window_text_appendf(
            buffer,
            "<a class='rightsAction %s' title='%s' href='kbo://%s/%u' data-player='",
            class_name,
            title,
            command,
            player_id);
    } else {
        kbo_window_text_appendf(
            buffer,
            "<span class='rightsAction %s disabled' title='%s' data-player='",
            class_name,
            disabled_title);
    }
    kbo_html_append_escaped(buffer, player_name != NULL && player_name[0] != '\0' ? player_name : "알 수 없는 선수");
    kbo_window_text_appendf(
        buffer,
        "'>%s</%s>",
        symbol,
        enabled ? "a" : "span");
}

static void kbo_webview_append_candidate_card(
    KboWindowTextBuffer* buffer,
    const KboForeignRightsUiSnapshotRow* row,
    uint32_t selected_foreign_player_id,
    int action_available,
    int window_open)
{
    char retained_text[16] = {0};
    char expires_text[16] = {0};
    char flags[96] = {0};
    char status[128] = {0};
    if (row == NULL) {
        return;
    }
    kbo_military_format_yyyymmdd(row->retained_on, retained_text, sizeof(retained_text));
    kbo_military_format_yyyymmdd(row->expires_on, expires_text, sizeof(expires_text));
    snprintf(flags, sizeof(flags), "%s%s%s%s%s",
        row->restricted ? "제한 " : "",
        row->secondary_restricted ? "2차제한 " : "",
        row->dfa ? "DFA " : "",
        row->loan_active ? "임대 " : "",
        row->injury_active ? "부상 " : "");
    const char* state_label = "미결정";
    if (row->has_active_right) {
        state_label = "행사";
    } else if (row->skip_chosen) {
        state_label = "미행사";
    } else if (row->retain_requested) {
        state_label = "행사 요청";
    }
    if (flags[0] != '\0') {
        snprintf(status, sizeof(status), "%s %s", state_label, flags);
    } else {
        snprintf(status, sizeof(status), "%s", state_label);
    }
    int retain_chosen = row->has_active_right || row->retain_requested;

    kbo_window_text_appendf(
        buffer,
        "<tr%s><td class='roAction'><span class='rightsActions'>",
        row->player_id == selected_foreign_player_id ? " class='selected'" : "");
    const char* blocked_title = window_open ? "내가 맡은 구단이 아닙니다" : "보류권 처리 기간이 아닙니다";
    kbo_webview_append_rights_action(
        buffer,
        "rightsAdd",
        "retain",
        "보류권 행사",
        retain_chosen ? "이미 보류권을 행사했습니다" : blocked_title,
        "+",
        row->player_id,
        row->player_name,
        action_available && window_open && !retain_chosen);
    kbo_webview_append_rights_action(
        buffer,
        "rightsRelease",
        "release",
        "보류권 미행사",
        row->skip_chosen ? "이미 미행사 처리했습니다" : blocked_title,
        "-",
        row->player_id,
        row->player_name,
        action_available && window_open && !row->skip_chosen);
    kbo_window_text_appendf(
        buffer,
        "</span></td><td class='roPo'>%s</td><td class='roNum'>",
        row->position_label);
    kbo_html_append_escaped(buffer, row->uniform_number);
    kbo_window_text_appendf(buffer, "</td>");
    kbo_webview_append_player_name_link_cell(
        buffer,
        row->player_name[0] != '\0' ? row->player_name : "알 수 없는 선수",
        row->player_id,
        "kbo://select/");
    kbo_window_text_appendf(buffer, "<td class='roTeam'>");
    kbo_html_append_escaped(buffer, row->team_abbrev[0] != '\0' ? row->team_abbrev : "-");
    kbo_window_text_appendf(buffer, "</td>");
    kbo_webview_append_roster_nation_cell(buffer, row->nation_id, kbo_hub_nation_flag_asset_path);
    if (row->age > 0u) {
        kbo_window_text_appendf(buffer, "<td class='roAge'>%u</td>", (uint32_t)row->age);
    } else {
        kbo_window_text_appendf(buffer, "<td class='roAge'></td>");
    }
    kbo_window_text_appendf(buffer, "<td class='roDate'>");
    kbo_html_append_escaped(buffer, retained_text);
    kbo_window_text_appendf(buffer, "</td><td class='roDate'>");
    kbo_html_append_escaped(buffer, expires_text);
    kbo_window_text_appendf(buffer, "</td><td class='roStatus'>");
    kbo_html_append_escaped(buffer, status);
    kbo_window_text_appendf(buffer, "</td></tr>");
}

void kbo_webview_append_foreign_rights_view(
    KboWindowTextBuffer* buffer,
    const char* window_status,
    uint32_t selected_team_id,
    uint32_t* selected_foreign_player_id)
{
            KBO_PROFILE_BEGIN(profile_foreign_rights_view);
            KBO_PROFILE_BEGIN(profile_foreign_rights_prepare);
            KboForeignRightsUiSnapshot snapshot;
            memset(&snapshot, 0, sizeof(snapshot));
            int updating = 0;
            int has_snapshot = kbo_foreign_rights_ui_snapshot_get(
                selected_team_id,
                &snapshot,
                &updating);
            if (selected_foreign_player_id != NULL
                    && *selected_foreign_player_id == 0u
                    && snapshot.top_player_id != 0u) {
                *selected_foreign_player_id = snapshot.top_player_id;
            }
            int action_available = kbo_hub_ui_team_action_available(
                selected_team_id,
                "hub_foreign_rights_render");
            KBO_PROFILE_END(profile_foreign_rights_prepare, "webview.foreign_rights.prepare");

            kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
            kbo_webview_append_roster_top_bar(buffer, window_status);
            kbo_window_text_appendf(
                buffer,
                "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable foreignRightsTable'><thead><tr>"
                "<th class='roAction'>보류권</th><th class='roPo' data-sort-type='text'>포지션</th><th class='roNum' data-sort-type='number'></th>"
                "<th class='roName' data-sort-type='text'>선수</th><th class='roTeam' data-sort-type='text'>구단</th>"
                "<th class='roNat' data-sort-type='text'>국적*</th><th class='roAge' data-sort-type='number'>나이</th>"
                "<th class='roDate' data-sort-type='date'>보류일</th><th class='roDate' data-sort-type='date'>만료일</th>"
                "<th class='roStatus' data-sort-type='text'>상태</th></tr></thead><tbody>");

            int rendered = 0;
            KBO_PROFILE_BEGIN(profile_foreign_rights_render);
            if (!has_snapshot && updating && selected_team_id != 0u) {
                kbo_window_text_appendf(buffer, "<tr><td colspan='10'>외국인 보류권 목록을 준비 중입니다.</td></tr>");
                rendered = 1;
            }
            for (int i = 0; i < snapshot.count; i++) {
                kbo_webview_append_candidate_card(
                    buffer,
                    &snapshot.rows[i],
                    selected_foreign_player_id != NULL ? *selected_foreign_player_id : 0u,
                    action_available,
                    snapshot.window_open);
                rendered++;
            }
            KBO_PROFILE_END(profile_foreign_rights_render, "webview.foreign_rights.render");
            if (rendered == 0) {
                kbo_window_text_appendf(buffer, "<tr><td colspan='10'></td></tr>");
            }
            kbo_profiler_record_us("webview.foreign_rights.rows", (unsigned long long)rendered);
            kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
            KBO_PROFILE_END(profile_foreign_rights_view, "webview.foreign_rights.total");
}
