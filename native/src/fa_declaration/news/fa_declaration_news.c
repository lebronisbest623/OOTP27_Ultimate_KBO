#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fa_declaration_news.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/news/ledger/core_news_ledger.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/news/templates/core_news_templates.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_string.h"
#include "render/fa_declaration_news_render.h"
#include "../../core/dates/constants/kbo_date_constants.h"

#define KBO_FA_DECLARATION_NEWS_DECLARED_LIMIT 8
#define KBO_FA_DECLARATION_NEWS_DEFERRED_LIMIT 6
#define KBO_FA_DECLARATION_NEWS_LEDGER_DOMAIN "fa_declaration"

static int kbo_get_fa_declaration_news_marker_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_declaration_news_markers.txt", out, out_size);
}

static int kbo_fa_declaration_news_marker_exists(const char* marker)
{
    if (marker == NULL || marker[0] == '\0') {
        return 0;
    }
    if (kbo_custom_news_ledger_completed(KBO_FA_DECLARATION_NEWS_LEDGER_DOMAIN, marker)) {
        return 1;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_news_marker_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int found = 0;
    if (ReadFile(file, buffer, size, &read, NULL)) {
        buffer[read] = '\0';
        found = strstr(buffer, marker) != NULL;
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buffer);
    if (found) {
        kbo_custom_news_ledger_record_completed(
            KBO_FA_DECLARATION_NEWS_LEDGER_DOMAIN,
            marker,
            "legacy_marker_backfill",
            "fa_declaration_news_marker_exists");
    }
    return found;
}

static void kbo_fa_declaration_news_persist_marker(const char* marker, const char* source)
{
    if (marker == NULL || marker[0] == '\0') {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_news_marker_path(path, sizeof(path))) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "KBO FA declaration news marker skipped source=%s marker=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            marker,
            GetLastError(),
            path);
        return;
    }

    DWORD written = 0;
    DWORD marker_len = (DWORD)strlen(marker);
    int marker_written = WriteFile(file, marker, marker_len, &written, NULL);
    DWORD newline_written = 0;
    int newline_ok = WriteFile(file, "\r\n", 2u, &newline_written, NULL);
    if (marker_written && written == marker_len && newline_ok && newline_written == 2u) {
        kbo_custom_news_ledger_record_completed(
            KBO_FA_DECLARATION_NEWS_LEDGER_DOMAIN,
            marker,
            "legacy_marker_persist",
            source);
    }
    CloseHandle(file);
}

int kbo_emit_fa_declaration_retry_news(
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int deferred_retry,
    const char* source)
{
    if (event_yyyymmdd == 0u || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX || league_id == 0u
            || candidates == NULL || candidate_count <= 0 || deferred_retry <= 0) {
        return 0;
    }

    const KboFaDeclarationCandidate* candidate = kbo_fa_declaration_news_find_best_candidate(
        candidates,
        candidate_count,
        0,
        1,
        NULL,
        0);
    if (candidate == NULL || candidate->player_id == 0u) {
        return 0;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    if (month == 0u || day == 0u) {
        return 0;
    }

    char marker[96] = {0};
    snprintf(marker, sizeof(marker), "retry|%u|%u", season, league_id);
    if (kbo_fa_declaration_news_marker_exists(marker)) {
        return 0;
    }

    const char* player_name = candidate->player_name[0] != '\0'
        ? candidate->player_name
        : "FA candidate";
    char season_text[16] = {0};
    char player_link[160] = {0};
    char team_name[96] = {0};
    char team_link[128] = {0};
    char grade_text[16] = {0};
    char age_text[16] = {0};
    char retry_count_text[16] = {0};
    snprintf(season_text, sizeof(season_text), "%u", season);
    snprintf(player_link, sizeof(player_link), "<%s:player#%u>", player_name, candidate->player_id);
    kbo_fa_declaration_copy_team_name(candidate->team_id, team_name, sizeof(team_name));
    kbo_fa_declaration_copy_team_link(candidate->team_id, team_link, sizeof(team_link));
    snprintf(grade_text, sizeof(grade_text), "%s", candidate->grade[0] != '\0' ? candidate->grade : "-");
    snprintf(age_text, sizeof(age_text), "%u", (uint32_t)candidate->age);
    snprintf(retry_count_text, sizeof(retry_count_text), "%d", deferred_retry);

    KboNewsTemplateVar vars[] = {
        { "season", season_text },
        { "player_name", player_name },
        { "player_link", player_link },
        { "team_name", team_name },
        { "team_link", team_link },
        { "grade", grade_text },
        { "age", age_text },
        { "retry_count", retry_count_text },
    };

    char title[180] = {0};
    char body[2048] = {0};
    if (!kbo_news_template_render_key(
                "fa_declaration.retry.title",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                title,
                sizeof(title),
                source)
            || !kbo_news_template_render_key(
                "fa_declaration.retry.body",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                source)) {
        kbo_log_runtimef(
            "KBO FA declaration retry news skipped source=%s season=%u league_id=%u player=%u reason=templates_unavailable",
            source != NULL ? source : "",
            season,
            league_id,
            candidate->player_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        year,
        month,
        day,
        league_id,
        10u,
        title,
        body);
    if (created) {
        kbo_fa_declaration_news_persist_marker(marker, source);
    }
    kbo_log_runtimef(
        "KBO FA declaration retry news source=%s date=%u season=%u league=%u player=%u retry_count=%d created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        season,
        league_id,
        candidate->player_id,
        deferred_retry,
        created);
    return created;
}

int kbo_emit_fa_declaration_summary_news(
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared,
    int deferred,
    int deferred_retry,
    int deferred_no_market,
    const char* source)
{
    if (event_yyyymmdd == 0u || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX || league_id == 0u
            || candidates == NULL || candidate_count <= 0) {
        return 0;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    if (month == 0u || day == 0u) {
        return 0;
    }

    char marker[96] = {0};
    snprintf(marker, sizeof(marker), "summary|%u|%u", season, league_id);
    if (kbo_fa_declaration_news_marker_exists(marker)) {
        return 0;
    }

    char season_text[16] = {0};
    char declared_text[16] = {0};
    char deferred_text[16] = {0};
    char retry_text[16] = {0};
    char no_market_text[16] = {0};
    snprintf(season_text, sizeof(season_text), "%u", season);
    snprintf(declared_text, sizeof(declared_text), "%d", declared);
    snprintf(deferred_text, sizeof(deferred_text), "%d", deferred);
    snprintf(retry_text, sizeof(retry_text), "%d", deferred_retry);
    snprintf(no_market_text, sizeof(no_market_text), "%d", deferred_no_market);

    char declared_list[4096] = {0};
    char deferred_list[4096] = {0};
    kbo_fa_declaration_news_build_candidate_list(
        declared_list,
        sizeof(declared_list),
        candidates,
        candidate_count,
        1,
        -1,
        KBO_FA_DECLARATION_NEWS_DECLARED_LIMIT,
        "fa_declaration.summary.declared_line",
        source);
    kbo_fa_declaration_news_build_candidate_list(
        deferred_list,
        sizeof(deferred_list),
        candidates,
        candidate_count,
        0,
        0,
        KBO_FA_DECLARATION_NEWS_DEFERRED_LIMIT,
        "fa_declaration.summary.deferred_line",
        source);

    KboNewsTemplateVar vars[] = {
        { "season", season_text },
        { "declared_count", declared_text },
        { "deferred_count", deferred_text },
        { "retry_count", retry_text },
        { "no_market_count", no_market_text },
        { "declared_list", declared_list },
        { "deferred_list", deferred_list },
    };

    char title[180] = {0};
    char body[8192] = {0};
    if (!kbo_news_template_render_key(
                "fa_declaration.summary.title",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                title,
                sizeof(title),
                source)
            || !kbo_news_template_render_key(
                "fa_declaration.summary.body",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                source)) {
        kbo_log_runtimef(
            "KBO FA declaration news skipped source=%s season=%u league_id=%u reason=templates_unavailable",
            source != NULL ? source : "",
            season,
            league_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        year,
        month,
        day,
        league_id,
        10u,
        title,
        body);
    if (created) {
        kbo_fa_declaration_news_persist_marker(marker, source);
    }
    kbo_log_runtimef(
        "KBO FA declaration news source=%s date=%u season=%u league=%u candidates=%d declared=%d deferred=%d created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        season,
        league_id,
        candidate_count,
        declared,
        deferred,
        created);
    return created;
}
