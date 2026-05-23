#include "ui_secondary_draft_view_internal.h"

#include <string.h>

void kbo_webview_append_secondary_draft_view(
    KboWindowTextBuffer* buffer,
    int selected_subview,
    uint32_t* selected_season)
{
    if (buffer == NULL) {
        return;
    }

    uint32_t seasons[KBO_SECONDARY_DRAFT_UI_MAX_SEASONS] = {0};
    int season_count = kbo_secondary_draft_load_seasons(
        seasons,
        KBO_SECONDARY_DRAFT_UI_MAX_SEASONS);

    uint32_t season = selected_season != NULL ? *selected_season : 0u;
    if (season == 0u) {
        season = kbo_secondary_draft_ui_default_season();
        if (season == 0u && season_count > 0) {
            season = seasons[0];
        }
        if (selected_season != NULL) {
            *selected_season = season;
        }
    }
    if (selected_subview < 0 || selected_subview >= KBO_HUB_SECONDARY_DRAFT_SUBVIEW_COUNT) {
        selected_subview = KBO_HUB_SECONDARY_DRAFT_SUBVIEW_SCHEDULE;
    }

    KboSecondaryDraftRunSummary summary;
    memset(&summary, 0, sizeof(summary));
    int has_summary = season != 0u && kbo_secondary_draft_load_run_summary(season, &summary);

    KboSecondaryDraftResultRow rows[KBO_SECONDARY_DRAFT_UI_MAX_ROWS];
    int row_count = season != 0u
        ? kbo_secondary_draft_load_result_rows(season, rows, KBO_SECONDARY_DRAFT_UI_MAX_ROWS)
        : 0;

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights secondaryDraftRights'>");
    kbo_secondary_draft_ui_append_top_bar(
        buffer,
        has_summary ? &summary : NULL,
        seasons,
        season_count,
        season,
        row_count);
    if (selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_SCHEDULE) {
        kbo_secondary_draft_ui_append_schedule_view(buffer, seasons, season_count, season);
    } else if (selected_subview == KBO_HUB_SECONDARY_DRAFT_SUBVIEW_LIST) {
        kbo_secondary_draft_ui_append_list_view(buffer, season, g_kbo_hub_selected_team_id);
    } else {
        kbo_secondary_draft_ui_append_draft_view(
            buffer,
            season,
            g_kbo_hub_selected_team_id,
            rows,
            row_count);
    }
    kbo_window_text_appendf(buffer, "</div>");
}
