#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ui_secondary_draft_view.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/league_roles/kbo_league_roles.h"
#include "../../../core/logging/core_log.h"
#include "../../../custom_events/secondary_draft/secondary_draft_internal.h"
#include "../../runtime/hotkey_window_runtime_shared.h"
#include "../../support/actions/ui_team_actions.h"

typedef struct KboSecondaryDraftUiContext {
    uint32_t season;
    uint32_t today;
    uint32_t league_id;
    KboSecondaryDraftWindow window;
    int window_loaded;
    int has_summary;
    int run_exists;
    int result_count;
    KboSecondaryDraftRunSummary summary;
    KboSecondaryDraftTeam teams[KBO_SECONDARY_DRAFT_TEAM_MAX];
    int team_count;
    int selected_team_index;
    KboSecondaryDraftCandidate* candidates;
    int candidate_count;
    int protected_count;
} KboSecondaryDraftUiContext;

static void kbo_secondary_draft_ui_format_date(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u/%02u/%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

static void kbo_secondary_draft_ui_format_cash_i64(int64_t value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (value <= 0) {
        snprintf(out, out_size, "-");
        return;
    }
    if (value >= 100000000ll) {
        int64_t eok = value / 100000000ll;
        int64_t tenth = (value % 100000000ll) / 10000000ll;
        snprintf(out, out_size, tenth == 0 ? "%lld억" : "%lld.%lld억", (long long)eok, (long long)tenth);
        return;
    }
    snprintf(out, out_size, "%lld", (long long)value);
}

static void kbo_secondary_draft_ui_format_cash_u32(uint32_t value, char* out, size_t out_size)
{
    kbo_secondary_draft_ui_format_cash_i64((int64_t)value, out, out_size);
}

static void kbo_secondary_draft_ui_format_service_days(uint16_t service_days, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    uint32_t years = (uint32_t)service_days / KBO_SECONDARY_DRAFT_SERVICE_DAYS_PER_SEASON;
    uint32_t days = (uint32_t)service_days % KBO_SECONDARY_DRAFT_SERVICE_DAYS_PER_SEASON;
    snprintf(out, out_size, "%uy %ud", years, days);
}

static uint32_t kbo_secondary_draft_ui_default_season(uint32_t requested)
{
    if (requested != 0u && kbo_secondary_draft_is_odd_season(requested)) {
        return requested;
    }

    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        return requested != 0u ? requested : 0u;
    }

    uint32_t year = today / 10000u;
    if (year == 0u) {
        return requested != 0u ? requested : 0u;
    }
    if (year & 1u) {
        return year;
    }

    uint32_t next_odd = year + 1u;
    KboSecondaryDraftWindow next_window;
    memset(&next_window, 0, sizeof(next_window));
    if (kbo_secondary_draft_load_window(next_odd, &next_window)) {
        return next_odd;
    }

    uint32_t prev_odd = year > 0u ? year - 1u : 0u;
    if (prev_odd != 0u
            && (kbo_secondary_draft_sql_run_exists(prev_odd)
                || kbo_secondary_draft_sql_result_count(prev_odd) > 0
                || kbo_secondary_draft_load_window(prev_odd, &next_window))) {
        return prev_odd;
    }
    return next_odd;
}

static int kbo_secondary_draft_ui_candidate_cmp(const void* left, const void* right)
{
    const KboSecondaryDraftCandidate* a = (const KboSecondaryDraftCandidate*)left;
    const KboSecondaryDraftCandidate* b = (const KboSecondaryDraftCandidate*)right;
    if (a->owner_index != b->owner_index) {
        return a->owner_index < b->owner_index ? -1 : 1;
    }
    if (a->protected_player != b->protected_player) {
        return a->protected_player ? 1 : -1;
    }
    if (a->value_score != b->value_score) {
        return a->value_score > b->value_score ? -1 : 1;
    }
    if (a->player_id != b->player_id) {
        return a->player_id < b->player_id ? -1 : 1;
    }
    return 0;
}

static int kbo_secondary_draft_ui_protection_window_open(const KboSecondaryDraftUiContext* ctx);
static int kbo_secondary_draft_ui_run_ready(const KboSecondaryDraftUiContext* ctx);

static int kbo_secondary_draft_ui_subview_should_load_candidates(
    const KboSecondaryDraftUiContext* ctx,
    int selected_subview)
{
    if (selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_PROTECTION) {
        return kbo_secondary_draft_ui_protection_window_open(ctx);
    }
    if (selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_DRAFT) {
        return kbo_secondary_draft_ui_run_ready(ctx);
    }
    return 0;
}

static int kbo_secondary_draft_ui_load_context(
    KboSecondaryDraftUiContext* ctx,
    uint32_t selected_team_id,
    uint32_t* selected_season,
    int selected_subview,
    int load_candidates)
{
    if (ctx == NULL) {
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_secondary_draft_ui_load_context);
    memset(ctx, 0, sizeof(*ctx));
    uint32_t requested = selected_season != NULL ? *selected_season : 0u;
    ctx->season = kbo_secondary_draft_ui_default_season(requested);
    if (selected_season != NULL && ctx->season != 0u) {
        *selected_season = ctx->season;
    }
    if (ctx->season == 0u) {
        KBO_PROFILE_END(profile_secondary_draft_ui_load_context, "webview.secondary_draft.load_context");
        return 0;
    }

    (void)kbo_current_date_tick_latest_published_date(&ctx->today);
    KBO_PROFILE_BEGIN(profile_secondary_draft_ui_window);
    ctx->window_loaded = kbo_secondary_draft_load_window(ctx->season, &ctx->window);
    ctx->league_id = ctx->window_loaded && ctx->window.league_id != 0u
        ? ctx->window.league_id
        : kbo_league_role_main_league_id();
    if (ctx->league_id == 0u) {
        ctx->league_id = kbo_resolve_kbo_league_id();
    }
    KBO_PROFILE_END(profile_secondary_draft_ui_window, "webview.secondary_draft.load_window");
    if (ctx->league_id == 0u) {
        KBO_PROFILE_END(profile_secondary_draft_ui_load_context, "webview.secondary_draft.load_context");
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_secondary_draft_ui_status);
    ctx->has_summary = kbo_secondary_draft_load_run_summary(ctx->season, &ctx->summary);
    ctx->result_count = kbo_secondary_draft_sql_result_count(ctx->season);
    ctx->run_exists = ctx->has_summary || ctx->result_count > 0;
    KBO_PROFILE_END(profile_secondary_draft_ui_status, "webview.secondary_draft.load_status");

    KBO_PROFILE_BEGIN(profile_secondary_draft_ui_teams);
    ctx->team_count = kbo_secondary_draft_collect_main_teams(
        ctx->league_id,
        ctx->teams,
        KBO_SECONDARY_DRAFT_TEAM_MAX);
    if (ctx->team_count > 1) {
        qsort(ctx->teams, (size_t)ctx->team_count, sizeof(ctx->teams[0]), kbo_secondary_draft_team_order_cmp);
    }
    KBO_PROFILE_END(profile_secondary_draft_ui_teams, "webview.secondary_draft.load_teams");
    ctx->selected_team_index = kbo_secondary_draft_team_index_by_id(
        ctx->teams,
        ctx->team_count,
        selected_team_id);
    if (ctx->selected_team_index < 0 && ctx->team_count > 0) {
        ctx->selected_team_index = 0;
    }

    int should_load_candidates = load_candidates
        && kbo_secondary_draft_ui_subview_should_load_candidates(ctx, selected_subview);
    if (!should_load_candidates) {
        if (ctx->has_summary) {
            ctx->candidate_count = ctx->summary.candidate_count;
            ctx->protected_count = ctx->summary.protected_count;
        }
        if (load_candidates) {
            kbo_profiler_record_us("webview.secondary_draft.collect_candidates.skipped_window", 0);
        }
        KBO_PROFILE_END(profile_secondary_draft_ui_load_context, "webview.secondary_draft.load_context");
        return 1;
    }

    ctx->candidates = (KboSecondaryDraftCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_SECONDARY_DRAFT_CANDIDATE_MAX * sizeof(KboSecondaryDraftCandidate));
    if (ctx->candidates != NULL && ctx->team_count > 0) {
        KBO_PROFILE_BEGIN(profile_secondary_draft_ui_candidates);
        ctx->candidate_count = kbo_secondary_draft_collect_candidates(
            ctx->season,
            ctx->teams,
            ctx->team_count,
            ctx->candidates,
            KBO_SECONDARY_DRAFT_CANDIDATE_MAX);
        KBO_PROFILE_END(profile_secondary_draft_ui_candidates, "webview.secondary_draft.collect_candidates");
        KBO_PROFILE_BEGIN(profile_secondary_draft_ui_protected);
        ctx->protected_count = kbo_secondary_draft_mark_protected_players(
            ctx->candidates,
            ctx->candidate_count,
            ctx->team_count,
            ctx->season);
        KBO_PROFILE_END(profile_secondary_draft_ui_protected, "webview.secondary_draft.mark_protected");
        KBO_PROFILE_BEGIN(profile_secondary_draft_ui_sort);
        if (ctx->candidate_count > 1) {
            qsort(
                ctx->candidates,
                (size_t)ctx->candidate_count,
                sizeof(ctx->candidates[0]),
                kbo_secondary_draft_ui_candidate_cmp);
        }
        KBO_PROFILE_END(profile_secondary_draft_ui_sort, "webview.secondary_draft.sort_candidates");
    }
    KBO_PROFILE_END(profile_secondary_draft_ui_load_context, "webview.secondary_draft.load_context");
    return 1;
}

static void kbo_secondary_draft_ui_free_context(KboSecondaryDraftUiContext* ctx)
{
    if (ctx != NULL && ctx->candidates != NULL) {
        HeapFree(GetProcessHeap(), 0, ctx->candidates);
        ctx->candidates = NULL;
    }
}

static int kbo_secondary_draft_ui_protection_window_open(const KboSecondaryDraftUiContext* ctx)
{
    if (ctx == NULL || ctx->run_exists || !ctx->window_loaded || ctx->today == 0u) {
        return 0;
    }
    if (ctx->window.protection_open_yyyymmdd != 0u && ctx->today < ctx->window.protection_open_yyyymmdd) {
        return 0;
    }
    if (ctx->window.protection_deadline_yyyymmdd != 0u && ctx->today > ctx->window.protection_deadline_yyyymmdd) {
        return 0;
    }
    return 1;
}

static int kbo_secondary_draft_ui_run_ready(const KboSecondaryDraftUiContext* ctx)
{
    if (ctx == NULL || ctx->run_exists || !ctx->window_loaded || ctx->today == 0u
            || ctx->window.draft_yyyymmdd == 0u) {
        return 0;
    }
    return ctx->today >= ctx->window.draft_yyyymmdd;
}

static const char* kbo_secondary_draft_ui_protection_locked_message(const KboSecondaryDraftUiContext* ctx)
{
    if (ctx == NULL) {
        return "2차 드래프트 컨텍스트를 찾지 못했습니다.";
    }
    if (ctx->run_exists) {
        return "완료된 시즌입니다. 결과 탭에서 확인하세요.";
    }
    if (!ctx->window_loaded) {
        return "2차 드래프트 일정이 등록되지 않았습니다.";
    }
    if (ctx->today == 0u) {
        return "현재 날짜를 확인하지 못했습니다.";
    }
    if (ctx->window.protection_open_yyyymmdd != 0u && ctx->today < ctx->window.protection_open_yyyymmdd) {
        return "보호명단 제출 기간 전입니다.";
    }
    if (ctx->window.protection_deadline_yyyymmdd != 0u && ctx->today > ctx->window.protection_deadline_yyyymmdd) {
        return "보호명단 제출 기간이 지났습니다.";
    }
    return "보호명단 제출 기간에만 후보 명단이 열립니다.";
}

static const char* kbo_secondary_draft_ui_draft_locked_message(const KboSecondaryDraftUiContext* ctx)
{
    if (ctx == NULL) {
        return "2차 드래프트 컨텍스트를 찾지 못했습니다.";
    }
    if (ctx->run_exists) {
        return "완료된 시즌입니다. 결과 탭에서 확인하세요.";
    }
    if (!ctx->window_loaded || ctx->window.draft_yyyymmdd == 0u) {
        return "2차 드래프트 일정이 등록되지 않았습니다.";
    }
    if (ctx->today == 0u) {
        return "현재 날짜를 확인하지 못했습니다.";
    }
    if (ctx->today < ctx->window.draft_yyyymmdd) {
        return "드래프트 실행일 전입니다.";
    }
    return "드래프트 실행일 이후에만 후보 보드가 열립니다.";
}

static void kbo_secondary_draft_ui_append_empty_row(
    KboWindowTextBuffer* buffer,
    int colspan,
    const char* text)
{
    kbo_window_text_appendf(buffer, "<tr><td class='roEmptyMessage' colspan='%d'>", colspan);
    kbo_html_append_escaped(buffer, text != NULL && text[0] != '\0' ? text : "-");
    kbo_window_text_appendf(buffer, "</td></tr>");
}

static void kbo_secondary_draft_ui_append_text_action(
    KboWindowTextBuffer* buffer,
    int enabled,
    const char* href,
    const char* label,
    const char* disabled_title)
{
    if (enabled && href != NULL && href[0] != '\0') {
        kbo_window_text_appendf(buffer, "<a class='rightsTextAction' href='");
        kbo_html_append_escaped(buffer, href);
        kbo_window_text_appendf(buffer, "'>");
        kbo_html_append_escaped(buffer, label);
        kbo_window_text_appendf(buffer, "</a>");
        return;
    }
    kbo_window_text_appendf(buffer, "<span class='rightsTextAction disabled' title='");
    kbo_html_append_escaped(buffer, disabled_title != NULL ? disabled_title : "");
    kbo_window_text_appendf(buffer, "'>");
    kbo_html_append_escaped(buffer, label);
    kbo_window_text_appendf(buffer, "</span>");
}

static void kbo_secondary_draft_ui_append_context_bar(
    KboWindowTextBuffer* buffer,
    const KboSecondaryDraftUiContext* ctx,
    const char* mode)
{
    char open_text[16] = "-";
    char deadline_text[16] = "-";
    char draft_text[16] = "-";
    char selected_team[96] = "-";
    char summary[384] = {0};
    if (ctx != NULL) {
        kbo_secondary_draft_ui_format_date(ctx->window.protection_open_yyyymmdd, open_text, sizeof(open_text));
        kbo_secondary_draft_ui_format_date(ctx->window.protection_deadline_yyyymmdd, deadline_text, sizeof(deadline_text));
        kbo_secondary_draft_ui_format_date(ctx->window.draft_yyyymmdd, draft_text, sizeof(draft_text));
        if (ctx->selected_team_index >= 0 && ctx->selected_team_index < ctx->team_count) {
            snprintf(selected_team, sizeof(selected_team), "%s", ctx->teams[ctx->selected_team_index].name);
        }
        snprintf(
            summary,
            sizeof(summary),
            "보기: %s - 시즌: %u - 선택: %s - 보호: %s ~ %s - 드래프트: %s - 팀: %d - 후보: %d - 결과: %d",
            mode != NULL ? mode : "-",
            ctx->season,
            selected_team,
            open_text,
            deadline_text,
            draft_text,
            ctx->team_count,
            ctx->candidate_count,
            ctx->result_count);
    } else {
        snprintf(summary, sizeof(summary), "보기: %s - 시즌 미확인", mode != NULL ? mode : "-");
    }
    kbo_webview_append_roster_top_bar(buffer, summary);
}

static void kbo_secondary_draft_ui_append_season_toolbar(
    KboWindowTextBuffer* buffer,
    const KboSecondaryDraftUiContext* ctx,
    int selected_subview)
{
    if (buffer == NULL || ctx == NULL || ctx->season == 0u) {
        return;
    }
    uint32_t prev = ctx->season > 2u ? ctx->season - 2u : 0u;
    uint32_t next = ctx->season + 2u;
    const KboSecondaryDraftTeam* team = NULL;
    if (ctx->selected_team_index >= 0 && ctx->selected_team_index < ctx->team_count) {
        team = &ctx->teams[ctx->selected_team_index];
    }
    int protection_open = kbo_secondary_draft_ui_protection_window_open(ctx);
    int submitted_count = 0;
    int submitted = selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_PROTECTION
            && protection_open
            && team != NULL
        ? kbo_secondary_draft_sql_team_submitted(ctx->season, team->team_id, &submitted_count)
        : 0;

    kbo_window_text_appendf(
        buffer,
        "<div style='height:30px;display:flex;align-items:center;justify-content:space-between;gap:8px;padding:0 8px;background:#181818;border:1px solid #171717;border-bottom:0'>"
        "<div style='display:flex;align-items:center;gap:6px;min-width:0'>");
    if (prev != 0u) {
        kbo_window_text_appendf(buffer, "<a class='rightsTextAction' href='kbo://secondary-draft/season/%u'>이전</a>", prev);
    }
    kbo_window_text_appendf(buffer, "<span style='color:#e8e8e8;font-size:12px;font-weight:900'>%u시즌</span>", ctx->season);
    kbo_window_text_appendf(buffer, "<a class='rightsTextAction' href='kbo://secondary-draft/season/%u'>다음</a>", next);
    kbo_window_text_appendf(buffer, "</div><div style='display:flex;align-items:center;gap:6px;min-width:0'>");
    if (selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_PROTECTION && team != NULL) {
        char auto_href[96] = {0};
        char submit_href[96] = {0};
        int action_available = protection_open
            ? kbo_hub_ui_team_action_available(team->team_id, "hub_secondary_draft_toolbar_render")
            : 0;
        snprintf(auto_href, sizeof(auto_href), "kbo://secondary-draft/auto/%u/%u", ctx->season, team->team_id);
        snprintf(submit_href, sizeof(submit_href), "kbo://secondary-draft/submit/%u/%u", ctx->season, team->team_id);
        kbo_secondary_draft_ui_append_text_action(
            buffer,
            protection_open && !submitted && action_available,
            auto_href,
            "자동 채우기",
            action_available ? "보호명단 제출 창구가 아닙니다" : "내가 맡은 구단이 아닙니다");
        kbo_secondary_draft_ui_append_text_action(
            buffer,
            protection_open && !submitted && action_available,
            submit_href,
            submitted ? "제출됨" : "명단 제출",
            action_available ? "보호명단 제출 창구가 아닙니다" : "내가 맡은 구단이 아닙니다");
        if (submitted) {
            kbo_window_text_appendf(buffer, "<span style='color:#bdbdbd;font-size:12px;font-weight:900'>%d명 제출</span>", submitted_count);
        }
    } else if (selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_DRAFT
            && kbo_secondary_draft_ui_run_ready(ctx)
            && team != NULL
            && kbo_hub_ui_team_action_available(team->team_id, "hub_secondary_draft_run_render")) {
        kbo_window_text_appendf(buffer, "<a class='rightsTextAction' href='kbo://secondary-draft/run/%u'>드래프트 실행</a>", ctx->season);
    } else if (ctx->run_exists) {
        kbo_window_text_appendf(buffer, "<span class='rightsTextAction disabled'>완료</span>");
    } else {
        kbo_window_text_appendf(buffer, "<span class='rightsTextAction disabled'>대기</span>");
    }
    kbo_window_text_appendf(buffer, "</div></div>");
}

static void kbo_secondary_draft_ui_append_team_link(
    KboWindowTextBuffer* buffer,
    const KboSecondaryDraftTeam* team)
{
    if (buffer == NULL || team == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<span class='roNameInner'><a href='kbo://setteam/%u'>", team->team_id);
    kbo_html_append_escaped(buffer, team->name);
    kbo_window_text_appendf(buffer, "</a></span>");
}

static void kbo_secondary_draft_ui_append_candidates_view(
    KboWindowTextBuffer* buffer,
    const KboSecondaryDraftUiContext* ctx)
{
    KBO_PROFILE_BEGIN(profile_secondary_draft_render_protection);
    const KboSecondaryDraftTeam* selected_team = NULL;
    int team_index = ctx != NULL ? ctx->selected_team_index : -1;
    if (ctx != NULL && team_index >= 0 && team_index < ctx->team_count) {
        selected_team = &ctx->teams[team_index];
    }
    int submitted_count = 0;
    int protection_open = kbo_secondary_draft_ui_protection_window_open(ctx);
    int submitted = 0;
    int saved_count = 0;
    int action_available = 0;
    if (selected_team != NULL && protection_open) {
        submitted = kbo_secondary_draft_sql_team_submitted(ctx->season, selected_team->team_id, &submitted_count);
        saved_count = kbo_secondary_draft_sql_protected_count(ctx->season, selected_team->team_id);
        action_available = kbo_hub_ui_team_action_available(selected_team->team_id, "hub_secondary_draft_protect_render");
    }

    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable secondaryDraftCandidateTable'><thead><tr>"
        "<th class='roAction' data-sort-type='text'>보호</th><th class='roPo' data-sort-type='text'>포지션</th>"
        "<th class='roName' data-sort-type='text'>선수</th><th class='roAge' data-sort-type='number'>나이</th>"
        "<th data-sort-type='number'>연차</th><th data-sort-type='number'>서비스</th>"
        "<th class='roStatus' data-sort-type='text'>상태</th>"
        "</tr></thead><tbody>");

    if (selected_team == NULL) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 7, "먼저 KBO 구단을 선택하세요.");
    } else if (!protection_open) {
        kbo_secondary_draft_ui_append_empty_row(
            buffer,
            7,
            kbo_secondary_draft_ui_protection_locked_message(ctx));
    } else if (ctx->candidates == NULL || ctx->candidate_count <= 0) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 7, "이 시즌의 2차 드래프트 후보를 찾지 못했습니다.");
    }

    int rendered = 0;
    for (int i = 0; protection_open && ctx != NULL && ctx->candidates != NULL && i < ctx->candidate_count && rendered < 600; i++) {
        KboSecondaryDraftCandidate* candidate = &ctx->candidates[i];
        if (candidate->owner_index != team_index) {
            continue;
        }
        const char* status = candidate->protected_player ? "보호" : "노출";
        char href[128] = {0};
        snprintf(
            href,
            sizeof(href),
            "kbo://secondary-draft/protect/%u/%u/%u",
            ctx->season,
            selected_team->team_id,
            candidate->player_id);
        int can_protect = protection_open
            && !submitted
            && !candidate->protected_player
            && saved_count < KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT
            && action_available;

        kbo_window_text_appendf(buffer, candidate->protected_player ? "<tr class='selected'>" : "<tr>");
        kbo_window_text_appendf(buffer, "<td class='roAction'>");
        kbo_secondary_draft_ui_append_text_action(
            buffer,
            can_protect,
            href,
            candidate->protected_player ? "보호" : "보호",
            submitted ? "이미 제출된 보호명단입니다" : (action_available ? "보호명단 제출 창구가 아닙니다" : "내가 맡은 구단이 아닙니다"));
        kbo_window_text_appendf(buffer, "</td><td class='roPo'>");
        if (kbo_player_pointer_plausible(candidate->player_ptr)
                && memory_range_readable((void*)candidate->player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            kbo_html_append_escaped(
                buffer,
                kbo_webview_player_position_label((uint8_t*)candidate->player_ptr, candidate->position_role));
        } else {
            kbo_html_append_escaped(buffer, "-");
        }
        kbo_window_text_appendf(buffer, "</td>");
        kbo_webview_append_player_name_cell(buffer, candidate->player_name, candidate->player_id);
        char service_text[24] = {0};
        kbo_secondary_draft_ui_format_service_days(candidate->service_days, service_text, sizeof(service_text));
        kbo_window_text_appendf(
            buffer,
            "<td class='roAge' data-sort-value='%u'>%u</td><td data-sort-value='%d'>%d</td>"
            "<td data-sort-value='%u'>",
            (uint32_t)candidate->age,
            (uint32_t)candidate->age,
            candidate->total_seasons,
            candidate->total_seasons,
            (uint32_t)candidate->service_days);
        kbo_html_append_escaped(buffer, service_text);
        kbo_window_text_appendf(buffer, "</td><td class='roStatus'>");
        kbo_html_append_escaped(buffer, status);
        if (candidate->military_reserved) {
            kbo_window_text_appendf(buffer, " / 군복무");
        }
        kbo_window_text_appendf(buffer, "</td></tr>");
        rendered++;
    }
    if (selected_team != NULL && protection_open && rendered == 0) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 7, "선택 구단의 노출 후보가 없습니다.");
    } else if (rendered >= 600) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 7, "표시가 600명에서 잘렸습니다.");
    }
    (void)submitted_count;
    kbo_window_text_appendf(buffer, "</tbody></table></section>");
    KBO_PROFILE_END(profile_secondary_draft_render_protection, "webview.secondary_draft.render.protection");
}

static void kbo_secondary_draft_ui_append_draft_view(
    KboWindowTextBuffer* buffer,
    const KboSecondaryDraftUiContext* ctx)
{
    KBO_PROFILE_BEGIN(profile_secondary_draft_render_draft);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable secondaryDraftBoardTable'><thead><tr>"
        "<th data-sort-type='number'>순위</th><th class='roTeam' data-sort-type='text'>원 소속</th>"
        "<th class='roPo' data-sort-type='text'>포지션</th><th class='roName' data-sort-type='text'>선수</th>"
        "<th class='roAge' data-sort-type='number'>나이</th><th data-sort-type='number'>연차</th>"
        "<th data-sort-type='number'>서비스</th><th class='roStatus' data-sort-type='text'>상태</th>"
        "</tr></thead><tbody>");

    if (ctx == NULL || ctx->candidates == NULL || ctx->candidate_count <= 0) {
        kbo_secondary_draft_ui_append_empty_row(
            buffer,
            8,
            ctx != NULL && !kbo_secondary_draft_ui_run_ready(ctx)
                ? kbo_secondary_draft_ui_draft_locked_message(ctx)
                : "이 시즌의 2차 드래프트 후보를 찾지 못했습니다.");
        kbo_window_text_appendf(buffer, "</tbody></table></section>");
        KBO_PROFILE_END(profile_secondary_draft_render_draft, "webview.secondary_draft.render.draft");
        return;
    }

    int rendered = 0;
    for (int i = 0; i < ctx->candidate_count && rendered < 800; i++) {
        KboSecondaryDraftCandidate* candidate = &ctx->candidates[i];
        const KboSecondaryDraftTeam* owner = NULL;
        if (candidate->owner_index >= 0 && candidate->owner_index < ctx->team_count) {
            owner = &ctx->teams[candidate->owner_index];
        }
        int row_selected = candidate->owner_index == ctx->selected_team_index;
        kbo_window_text_appendf(buffer, row_selected ? "<tr class='selected'>" : "<tr>");
        kbo_window_text_appendf(buffer, "<td data-sort-value='%d'>%d</td><td class='roTeam'>", rendered + 1, rendered + 1);
        if (owner != NULL) {
            kbo_secondary_draft_ui_append_team_link(buffer, owner);
        } else {
            kbo_html_append_escaped(buffer, "-");
        }
        kbo_window_text_appendf(buffer, "</td><td class='roPo'>");
        if (kbo_player_pointer_plausible(candidate->player_ptr)
                && memory_range_readable((void*)candidate->player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            kbo_html_append_escaped(
                buffer,
                kbo_webview_player_position_label((uint8_t*)candidate->player_ptr, candidate->position_role));
        } else {
            kbo_html_append_escaped(buffer, "-");
        }
        kbo_window_text_appendf(buffer, "</td>");
        kbo_webview_append_player_name_cell(buffer, candidate->player_name, candidate->player_id);
        char service_text[24] = {0};
        kbo_secondary_draft_ui_format_service_days(candidate->service_days, service_text, sizeof(service_text));
        kbo_window_text_appendf(
            buffer,
            "<td class='roAge' data-sort-value='%u'>%u</td><td data-sort-value='%d'>%d</td>"
            "<td data-sort-value='%u'>",
            (uint32_t)candidate->age,
            (uint32_t)candidate->age,
            candidate->total_seasons,
            candidate->total_seasons,
            (uint32_t)candidate->service_days);
        kbo_html_append_escaped(buffer, service_text);
        kbo_window_text_appendf(buffer, "</td><td class='roStatus'>");
        kbo_html_append_escaped(buffer, candidate->protected_player ? "보호" : "노출");
        if (candidate->military_reserved) {
            kbo_window_text_appendf(buffer, " / 군복무");
        }
        kbo_window_text_appendf(buffer, "</td></tr>");
        rendered++;
    }
    if (rendered >= 800) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 8, "표시가 800명에서 잘렸습니다.");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section>");
    KBO_PROFILE_END(profile_secondary_draft_render_draft, "webview.secondary_draft.render.draft");
}

static void kbo_secondary_draft_ui_append_results_view(
    KboWindowTextBuffer* buffer,
    const KboSecondaryDraftUiContext* ctx)
{
    KBO_PROFILE_BEGIN(profile_secondary_draft_render_results);
    KboSecondaryDraftPickRow rows[KBO_SECONDARY_DRAFT_RESULT_MAX];
    memset(rows, 0, sizeof(rows));
    int count = ctx != NULL
        ? kbo_secondary_draft_load_results(ctx->season, rows, KBO_SECONDARY_DRAFT_RESULT_MAX)
        : 0;

    char cash_total[32] = "-";
    if (ctx != NULL && ctx->has_summary) {
        kbo_secondary_draft_ui_format_cash_i64(ctx->summary.cash_total, cash_total, sizeof(cash_total));
    }
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable secondaryDraftResultTable'><thead><tr>"
        "<th data-sort-type='number'>픽</th><th data-sort-type='number'>라운드</th>"
        "<th class='roName' data-sort-type='text'>선수</th><th class='roTeam' data-sort-type='text'>원 소속</th>"
        "<th class='roTeam' data-sort-type='text'>지명</th><th class='roCash' data-sort-type='number'>보상금</th>"
        "<th class='roStatus' data-sort-type='text'>상태</th>"
        "</tr></thead><tbody>");
    if (ctx == NULL || count <= 0) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 7, "이 시즌의 2차 드래프트 결과가 없습니다.");
    }
    for (int i = 0; i < count; i++) {
        KboSecondaryDraftPickRow* row = &rows[i];
        char cash_text[32] = "-";
        kbo_secondary_draft_ui_format_cash_u32(row->cash_amount, cash_text, sizeof(cash_text));
        kbo_window_text_appendf(
            buffer,
            "<tr><td data-sort-value='%u'>%u</td><td data-sort-value='%u'>%u</td>",
            row->pick_no,
            row->pick_no,
            row->round,
            row->round);
        kbo_webview_append_player_name_cell(buffer, row->player_name, row->player_id);
        kbo_window_text_appendf(buffer, "<td class='roTeam'>");
        kbo_html_append_escaped(buffer, row->from_team_name);
        kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
        kbo_html_append_escaped(buffer, row->to_team_name);
        kbo_window_text_appendf(buffer, "</td><td class='roCash' data-sort-value='%u'>", row->cash_amount);
        kbo_html_append_escaped(buffer, cash_text);
        kbo_window_text_appendf(buffer, "</td><td class='roStatus'>");
        if (row->moved) {
            kbo_html_append_escaped(buffer, row->military_rights_transfer ? "보류권 이동" : "이적");
        } else {
            kbo_html_append_escaped(buffer, "기록");
        }
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    if (ctx != NULL && ctx->has_summary) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='7' class='roEmptyMessage'>총 %d픽 / 후보 %d명 / 보호 %d명 / 보상금 ",
            ctx->summary.pick_count,
            ctx->summary.candidate_count,
            ctx->summary.protected_count);
        kbo_html_append_escaped(buffer, cash_total);
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section>");
    KBO_PROFILE_END(profile_secondary_draft_render_results, "webview.secondary_draft.render.results");
}

void kbo_webview_append_secondary_draft_view(
    KboWindowTextBuffer* buffer,
    int selected_subview,
    uint32_t selected_team_id,
    uint32_t* selected_season)
{
    if (buffer == NULL) {
        return;
    }
    if (selected_subview < 0 || selected_subview >= KBO_HUB_SECONDARY_DRAFT_SUBVIEW_COUNT) {
        selected_subview = KBO_HUB_SECONDARY_DRAFT_SUBVIEW_PROTECTION;
    }

    KBO_PROFILE_BEGIN(profile_secondary_draft_ui_total);
    KboSecondaryDraftUiContext ctx;
    int load_candidates = selected_subview != KBO_HUB_SECONDARY_DRAFT_SUBVIEW_RESULTS;
    int loaded = kbo_secondary_draft_ui_load_context(
        &ctx,
        selected_team_id,
        selected_season,
        selected_subview,
        load_candidates);
    const char* mode = selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_DRAFT
        ? "드래프트"
        : (selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_RESULTS ? "결과" : "보호명단");

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights secondaryDraft'>");
    kbo_secondary_draft_ui_append_context_bar(buffer, loaded ? &ctx : NULL, mode);
    if (!loaded) {
        kbo_window_text_appendf(
            buffer,
            "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable'><tbody>");
        kbo_secondary_draft_ui_append_empty_row(buffer, 1, "2차 드래프트 데이터를 만들 KBO 리그 컨텍스트를 찾지 못했습니다.");
        kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
        KBO_PROFILE_END(profile_secondary_draft_ui_total, "webview.secondary_draft.total");
        return;
    }
    kbo_secondary_draft_ui_append_season_toolbar(buffer, &ctx, selected_subview);
    KBO_PROFILE_BEGIN(profile_secondary_draft_ui_render);
    if (selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_DRAFT) {
        kbo_secondary_draft_ui_append_draft_view(buffer, &ctx);
    } else if (selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_RESULTS) {
        kbo_secondary_draft_ui_append_results_view(buffer, &ctx);
    } else {
        kbo_secondary_draft_ui_append_candidates_view(buffer, &ctx);
    }
    KBO_PROFILE_END(profile_secondary_draft_ui_render, "webview.secondary_draft.render");
    kbo_window_text_appendf(buffer, "</div>");
    kbo_secondary_draft_ui_free_context(&ctx);
    KBO_PROFILE_END(profile_secondary_draft_ui_total, "webview.secondary_draft.total");
}

int kbo_secondary_draft_ui_auto_submit_team(uint32_t season, uint32_t team_id, const char* source)
{
    uint32_t selected_season = season;
    KboSecondaryDraftUiContext ctx;
    if (!kbo_secondary_draft_ui_load_context(
            &ctx,
            team_id,
            &selected_season,
            KBO_HUB_SECONDARY_DRAFT_SUBVIEW_PROTECTION,
            1)) {
        kbo_log_runtimef("KBO secondary draft UI auto submit ignored season=%u team=%u reason=context_unavailable", season, team_id);
        return 0;
    }
    int result = 0;
    int team_index = kbo_secondary_draft_team_index_by_id(ctx.teams, ctx.team_count, team_id);
    if (team_index < 0) {
        kbo_log_runtimef("KBO secondary draft UI auto submit ignored season=%u team=%u reason=team_unavailable", ctx.season, team_id);
    } else if (!kbo_secondary_draft_ui_protection_window_open(&ctx)) {
        kbo_log_runtimef("KBO secondary draft UI auto submit ignored season=%u team=%u reason=window_closed", ctx.season, team_id);
    } else if (!kbo_hub_ui_team_action_available(team_id, source != NULL ? source : "hub_secondary_draft_auto_submit")) {
        kbo_log_runtimef("KBO secondary draft UI auto submit ignored season=%u team=%u reason=action_blocked", ctx.season, team_id);
    } else {
        int written = 0;
        result = kbo_secondary_draft_auto_submit_team_protection_list(
            ctx.season,
            &ctx.teams[team_index],
            team_index,
            ctx.candidates,
            ctx.candidate_count,
            source != NULL ? source : "hub_secondary_draft_auto_submit",
            &written);
        kbo_log_runtimef(
            "KBO secondary draft UI auto submit season=%u team=%u result=%d written=%d",
            ctx.season,
            team_id,
            result,
            written);
    }
    kbo_secondary_draft_ui_free_context(&ctx);
    return result;
}

int kbo_secondary_draft_ui_submit_team(uint32_t season, uint32_t team_id, const char* source)
{
    uint32_t selected_season = season;
    KboSecondaryDraftUiContext ctx;
    if (!kbo_secondary_draft_ui_load_context(
            &ctx,
            team_id,
            &selected_season,
            KBO_HUB_SECONDARY_DRAFT_SUBVIEW_PROTECTION,
            0)) {
        kbo_log_runtimef("KBO secondary draft UI submit ignored season=%u team=%u reason=context_unavailable", season, team_id);
        return 0;
    }
    int result = 0;
    int team_index = kbo_secondary_draft_team_index_by_id(ctx.teams, ctx.team_count, team_id);
    int saved_count = 0;
    if (team_index < 0 || team_id == 0u) {
        kbo_log_runtimef("KBO secondary draft UI submit ignored season=%u team=%u reason=invalid_team", ctx.season, team_id);
    } else if (!kbo_secondary_draft_ui_protection_window_open(&ctx)) {
        kbo_log_runtimef("KBO secondary draft UI submit ignored season=%u team=%u reason=window_closed", ctx.season, team_id);
    } else if (kbo_secondary_draft_sql_team_submitted(ctx.season, team_id, NULL)) {
        kbo_log_runtimef("KBO secondary draft UI submit ignored season=%u team=%u reason=already_submitted", ctx.season, team_id);
    } else if (!kbo_hub_ui_team_action_available(team_id, source != NULL ? source : "hub_secondary_draft_submit")) {
        kbo_log_runtimef("KBO secondary draft UI submit ignored season=%u team=%u reason=action_blocked", ctx.season, team_id);
    } else {
        saved_count = kbo_secondary_draft_sql_protected_count(ctx.season, team_id);
        if (saved_count > KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT) {
            kbo_log_runtimef("KBO secondary draft UI submit ignored season=%u team=%u reason=list_overflow count=%d", ctx.season, team_id, saved_count);
            kbo_secondary_draft_ui_free_context(&ctx);
            return 0;
        }
        result = kbo_secondary_draft_sql_submit_team(
            ctx.season,
            team_id,
            ctx.teams[team_index].name,
            saved_count,
            source != NULL ? source : "hub_secondary_draft_submit");
        kbo_log_runtimef(
            "KBO secondary draft UI submit season=%u team=%u protected=%d result=%d",
            ctx.season,
            team_id,
            saved_count,
            result);
    }
    kbo_secondary_draft_ui_free_context(&ctx);
    return result;
}

int kbo_secondary_draft_ui_protect_player(
    uint32_t season,
    uint32_t team_id,
    uint32_t player_id,
    const char* source)
{
    uint32_t selected_season = season;
    KboSecondaryDraftUiContext ctx;
    if (!kbo_secondary_draft_ui_load_context(
            &ctx,
            team_id,
            &selected_season,
            KBO_HUB_SECONDARY_DRAFT_SUBVIEW_PROTECTION,
            1)) {
        kbo_log_runtimef("KBO secondary draft UI protect ignored season=%u team=%u player=%u reason=context_unavailable", season, team_id, player_id);
        return 0;
    }
    int result = 0;
    int team_index = kbo_secondary_draft_team_index_by_id(ctx.teams, ctx.team_count, team_id);
    int saved_count = 0;
    if (team_index < 0 || player_id == 0u) {
        kbo_log_runtimef("KBO secondary draft UI protect ignored season=%u team=%u player=%u reason=invalid_target", ctx.season, team_id, player_id);
    } else if (!kbo_secondary_draft_ui_protection_window_open(&ctx)) {
        kbo_log_runtimef("KBO secondary draft UI protect ignored season=%u team=%u player=%u reason=window_closed", ctx.season, team_id, player_id);
    } else if (kbo_secondary_draft_sql_team_submitted(ctx.season, team_id, NULL)) {
        kbo_log_runtimef("KBO secondary draft UI protect ignored season=%u team=%u player=%u reason=already_submitted", ctx.season, team_id, player_id);
    } else if (!kbo_hub_ui_team_action_available(team_id, source != NULL ? source : "hub_secondary_draft_protect")) {
        kbo_log_runtimef("KBO secondary draft UI protect ignored season=%u team=%u player=%u reason=action_blocked", ctx.season, team_id, player_id);
    } else {
        saved_count = kbo_secondary_draft_sql_protected_count(ctx.season, team_id);
        if (saved_count >= KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT) {
            kbo_log_runtimef("KBO secondary draft UI protect ignored season=%u team=%u player=%u reason=list_full", ctx.season, team_id, player_id);
            kbo_secondary_draft_ui_free_context(&ctx);
            return 0;
        }
        for (int i = 0; i < ctx.candidate_count; i++) {
            KboSecondaryDraftCandidate* candidate = &ctx.candidates[i];
            if (candidate->owner_index != team_index || candidate->player_id != player_id) {
                continue;
            }
            if (!candidate->protected_player) {
                result = kbo_secondary_draft_sql_write_protected_player(
                    ctx.season,
                    team_id,
                    player_id,
                    candidate->player_name,
                    ctx.teams[team_index].name,
                    source != NULL ? source : "hub_secondary_draft_protect");
            }
            break;
        }
        kbo_log_runtimef(
            "KBO secondary draft UI protect season=%u team=%u player=%u result=%d",
            ctx.season,
            team_id,
            player_id,
            result);
    }
    kbo_secondary_draft_ui_free_context(&ctx);
    return result;
}

int kbo_secondary_draft_ui_run_draft(uint32_t season, uint32_t selected_team_id, const char* source)
{
    uint32_t selected_season = season;
    KboSecondaryDraftUiContext ctx;
    if (!kbo_secondary_draft_ui_load_context(
            &ctx,
            selected_team_id,
            &selected_season,
            KBO_HUB_SECONDARY_DRAFT_SUBVIEW_DRAFT,
            0)) {
        kbo_log_runtimef("KBO secondary draft UI run ignored season=%u reason=context_unavailable", season);
        return 0;
    }
    int result = 0;
    uint32_t action_team_id = ctx.selected_team_index >= 0 ? ctx.teams[ctx.selected_team_index].team_id : selected_team_id;
    if (!kbo_secondary_draft_ui_run_ready(&ctx)) {
        kbo_log_runtimef("KBO secondary draft UI run ignored season=%u reason=not_ready today=%u draft=%u", ctx.season, ctx.today, ctx.window.draft_yyyymmdd);
    } else if (!kbo_hub_ui_team_action_available(action_team_id, source != NULL ? source : "hub_secondary_draft_run")) {
        kbo_log_runtimef("KBO secondary draft UI run ignored season=%u team=%u reason=action_blocked", ctx.season, action_team_id);
    } else {
        result = kbo_handle_secondary_draft_event(
            ctx.window.draft_yyyymmdd,
            source != NULL ? source : "hub_secondary_draft_run");
        kbo_log_runtimef(
            "KBO secondary draft UI run season=%u draft=%u result=%d",
            ctx.season,
            ctx.window.draft_yyyymmdd,
            result);
    }
    kbo_secondary_draft_ui_free_context(&ctx);
    return result;
}
