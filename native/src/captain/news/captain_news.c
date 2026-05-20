#include "../internal/captain_selection_internal.h"

#include "markers/captain_news_markers.h"
#include "render/captain_news_render.h"

#include <stdint.h>

#include "../../core/news/live/core_live_news.h"
#include "../../core/news/templates/core_news_templates.h"
#include "../../core/dates/constants/kbo_date_constants.h"

static void kbo_captain_initial_selection_news_marker_key(
    uint32_t season,
    uint32_t league_id,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX || league_id == 0u) {
        return;
    }
    snprintf(out, out_size, "summary|%u|%u", season, league_id);
}

int kbo_captain_initial_selection_news_exists(uint32_t season, uint32_t league_id)
{
    char marker[128] = {0};
    kbo_captain_initial_selection_news_marker_key(season, league_id, marker, sizeof(marker));
    return marker[0] != '\0' && kbo_captain_news_marker_exists(marker);
}

int kbo_emit_captain_initial_selection_news(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source)
{
    if (date == 0u || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX || league_id == 0u || rows == NULL || row_count <= 0) {
        return 0;
    }

    char marker[128] = {0};
    kbo_captain_initial_selection_news_marker_key(season, league_id, marker, sizeof(marker));
    if (marker[0] == '\0' || kbo_captain_news_marker_exists(marker)) {
        return 0;
    }

    char template_path[MAX_PATH] = {0};
    char title[160] = {0};
    char intro[4096] = {0};
    char outro[4096] = {0};
    if (!kbo_captain_news_render_key(
                "captain.summary.title",
                season,
                NULL,
                NULL,
                title,
                sizeof(title),
                template_path,
                sizeof(template_path),
                source)
            || !kbo_captain_news_render_key(
                "captain.summary.intro",
                season,
                NULL,
                NULL,
                intro,
                sizeof(intro),
                NULL,
                0u,
                source)
            || !kbo_captain_news_render_key(
                "captain.summary.outro",
                season,
                NULL,
                NULL,
                outro,
                sizeof(outro),
                NULL,
                0u,
                source)) {
        kbo_log_runtimef(
            "KBO captain summary news skipped source=%s season=%u league_id=%u reason=templates_unavailable",
            source != NULL ? source : "",
            season,
            league_id);
        return 0;
    }

    int listed = 0;
    char body[8192] = {0};
    char rendered[4096] = {0};
    if (intro[0] != '\0') {
        kbo_news_text_append(body, sizeof(body), intro);
        kbo_news_text_appendf(body, sizeof(body), "\n\n");
    }
    for (int i = 0; i < row_count; i++) {
        if (rows[i].team_id == 0u || rows[i].player_id == 0u) {
            continue;
        }
        if (!kbo_captain_news_render_key(
                "captain.summary.line",
                season,
                &rows[i],
                NULL,
                rendered,
                sizeof(rendered),
                NULL,
                0u,
                source)) {
            kbo_log_runtimef(
                "KBO captain summary news skipped source=%s season=%u league_id=%u reason=line_template_unavailable",
                source != NULL ? source : "",
                season,
                league_id);
            return 0;
        }
        kbo_news_text_append(body, sizeof(body), rendered);
        if (!kbo_news_text_ends_with_newline(rendered)) {
            kbo_news_text_appendf(body, sizeof(body), "\n");
        }
        listed++;
    }
    if (listed <= 0) {
        kbo_log_runtimef(
            "KBO captain summary news skipped source=%s season=%u league_id=%u reason=no_listed_captains",
            source != NULL ? source : "",
            season,
            league_id);
        return 0;
    }
    if (outro[0] != '\0') {
        if (!kbo_news_text_ends_with_newline(body)) {
            kbo_news_text_appendf(body, sizeof(body), "\n");
        }
        kbo_news_text_appendf(body, sizeof(body), "\n");
        kbo_news_text_append(body, sizeof(body), outro);
    }

    int created = create_kbo_native_live_news_with_body_live_required(
        date / 10000u,
        (date / 100u) % 100u,
        date % 100u,
        league_id,
        10u,
        title,
        body);
    if (created) {
        kbo_captain_news_persist_marker(marker, source);
    }
    kbo_log_runtimef(
        "KBO captain summary news source=%s date=%u season=%u league_id=%u listed=%d created=%d template=%s",
        source != NULL ? source : "",
        date,
        season,
        league_id,
        listed,
        created,
        template_path);
    return created;
}

int kbo_emit_captain_replacement_news(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const KboCaptainSelectionRow* old_row,
    const KboCaptainSelectionRow* new_row,
    const char* source)
{
    if (date == 0u || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX || league_id == 0u
            || new_row == NULL || new_row->team_id == 0u || new_row->player_id == 0u) {
        return 0;
    }

    uint32_t old_player_id = old_row != NULL ? old_row->player_id : 0u;
    char marker[160] = {0};
    snprintf(
        marker,
        sizeof(marker),
        "replacement|%u|%u|%u|%u|%u",
        season,
        league_id,
        new_row->team_id,
        old_player_id,
        new_row->player_id);
    if (kbo_captain_news_marker_exists(marker)) {
        return 0;
    }

    char template_path[MAX_PATH] = {0};
    char title[180] = {0};
    char body[2048] = {0};
    const char* body_key = old_row != NULL && old_row->player_id != 0u
        ? "captain.replacement.body_departed"
        : "captain.replacement.body_vacant";
    if (!kbo_captain_news_render_key(
                "captain.replacement.title",
                season,
                new_row,
                old_row,
                title,
                sizeof(title),
                template_path,
                sizeof(template_path),
                source)
            || !kbo_captain_news_render_key(
                body_key,
                season,
                new_row,
                old_row,
                body,
                sizeof(body),
                NULL,
                0u,
                source)) {
        kbo_log_runtimef(
            "KBO captain replacement news skipped source=%s season=%u league_id=%u team=%u old_player=%u new_player=%u reason=templates_unavailable",
            source != NULL ? source : "",
            season,
            league_id,
            new_row->team_id,
            old_player_id,
            new_row->player_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        date / 10000u,
        (date / 100u) % 100u,
        date % 100u,
        league_id,
        10u,
        title,
        body);
    if (created) {
        kbo_captain_news_persist_marker(marker, source);
    }
    kbo_log_runtimef(
        "KBO captain replacement news source=%s date=%u season=%u league_id=%u team=%u old_player=%u new_player=%u created=%d template=%s",
        source != NULL ? source : "",
        date,
        season,
        league_id,
        new_row->team_id,
        old_player_id,
        new_row->player_id,
        created,
        template_path);
    return created;
}
