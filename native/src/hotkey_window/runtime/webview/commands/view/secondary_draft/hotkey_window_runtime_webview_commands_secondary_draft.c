#include "../hotkey_window_runtime_webview_commands_view_internal.h"

#include "../../../../../../core/dates/constants/kbo_date_constants.h"
#include "../../../../../views/secondary_draft/snapshot/ui_secondary_draft_draft_snapshot.h"
#include "../../../../../views/secondary_draft/snapshot/ui_secondary_draft_list_snapshot.h"

static uint32_t kbo_webview_parse_u32_segment(const char** text)
{
    if (text == NULL || *text == NULL) {
        return 0u;
    }
    uint32_t value = (uint32_t)strtoul(*text, NULL, 10);
    const char* slash = strchr(*text, '/');
    *text = slash != NULL ? slash + 1 : NULL;
    return value;
}

int kbo_webview_handle_secondary_draft_command(const char* cmd)
{
    const char* base = "secondary-draft/";
    if (strncmp(cmd, base, strlen(base)) != 0) {
        return 0;
    }
    const char* text = cmd + strlen(base);

    if (strncmp(text, "year/", 5) == 0) {
        uint32_t year = (uint32_t)strtoul(text + 5, NULL, 10);
        if (year == 0u || (year >= KBO_SEASON_YEAR_MIN && year <= KBO_RECORD_YEAR_MAX)) {
            g_kbo_hub_selected_secondary_draft_year = year;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SECONDARY_DRAFT;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }

    if (strncmp(text, "protect/", 8) == 0 || strncmp(text, "unprotect/", 10) == 0) {
        int protect = strncmp(text, "protect/", 8) == 0;
        const char* cursor = text + (protect ? 8 : 10);
        uint32_t season = kbo_webview_parse_u32_segment(&cursor);
        uint32_t team_id = kbo_webview_parse_u32_segment(&cursor);
        uint32_t player_id = cursor != NULL ? (uint32_t)strtoul(cursor, NULL, 10) : 0u;
        int action_allowed = kbo_webview_team_action_allowed(team_id, protect
            ? "hub_secondary_draft_protect"
            : "hub_secondary_draft_unprotect");
        int ok = action_allowed
            ? kbo_secondary_draft_set_protected_player(
                season,
                team_id,
                player_id,
                protect,
                protect ? "hub_secondary_draft_protect" : "hub_secondary_draft_unprotect")
            : 0;
        kbo_log_runtimef(
            "secondary draft UI list action=%s season=%u team=%u player=%u allowed=%d ok=%d",
            protect ? "protect" : "unprotect",
            season,
            team_id,
            player_id,
            action_allowed,
            ok);
        if (ok) {
            kbo_secondary_draft_ui_list_snapshot_invalidate();
            kbo_secondary_draft_ui_draft_snapshot_invalidate();
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SECONDARY_DRAFT;
        g_kbo_hub_selected_secondary_draft_subview = KBO_HUB_SECONDARY_DRAFT_SUBVIEW_LIST;
        g_kbo_hub_selected_secondary_draft_year = season;
        if (team_id != 0u) {
            g_kbo_hub_selected_team_id = team_id;
        }
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }

    if (strncmp(text, "autofill/", 9) == 0 || strncmp(text, "submit/", 7) == 0) {
        int autofill = strncmp(text, "autofill/", 9) == 0;
        const char* cursor = text + (autofill ? 9 : 7);
        uint32_t season = kbo_webview_parse_u32_segment(&cursor);
        uint32_t team_id = cursor != NULL ? (uint32_t)strtoul(cursor, NULL, 10) : 0u;
        int action_allowed = kbo_webview_team_action_allowed(team_id, autofill
            ? "hub_secondary_draft_autofill"
            : "hub_secondary_draft_submit");
        int ok = 0;
        if (action_allowed) {
            ok = autofill
                ? kbo_secondary_draft_autofill_protected_list(season, team_id, "hub_secondary_draft_autofill")
                : kbo_secondary_draft_submit_protected_list(season, team_id, "hub_secondary_draft_submit");
        }
        kbo_log_runtimef(
            "secondary draft UI list action=%s season=%u team=%u allowed=%d ok=%d",
            autofill ? "autofill" : "submit",
            season,
            team_id,
            action_allowed,
            ok);
        if (ok) {
            kbo_secondary_draft_ui_list_snapshot_invalidate();
            kbo_secondary_draft_ui_draft_snapshot_invalidate();
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SECONDARY_DRAFT;
        g_kbo_hub_selected_secondary_draft_subview = KBO_HUB_SECONDARY_DRAFT_SUBVIEW_LIST;
        g_kbo_hub_selected_secondary_draft_year = season;
        if (team_id != 0u) {
            g_kbo_hub_selected_team_id = team_id;
        }
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }

    if (strncmp(text, "pick/", 5) == 0) {
        const char* cursor = text + 5;
        uint32_t season = kbo_webview_parse_u32_segment(&cursor);
        uint32_t team_id = kbo_webview_parse_u32_segment(&cursor);
        uint32_t player_id = cursor != NULL ? (uint32_t)strtoul(cursor, NULL, 10) : 0u;
        int action_allowed = kbo_webview_team_action_allowed(team_id, "hub_secondary_draft_pick");
        int ok = action_allowed
            ? kbo_secondary_draft_manual_pick_player(season, team_id, player_id, "hub_secondary_draft_pick")
            : 0;
        kbo_log_runtimef(
            "secondary draft UI pick season=%u team=%u player=%u allowed=%d ok=%d",
            season,
            team_id,
            player_id,
            action_allowed,
            ok);
        if (ok) {
            kbo_secondary_draft_ui_list_snapshot_invalidate();
            kbo_secondary_draft_ui_draft_snapshot_invalidate();
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SECONDARY_DRAFT;
        g_kbo_hub_selected_secondary_draft_subview = KBO_HUB_SECONDARY_DRAFT_SUBVIEW_DRAFT;
        g_kbo_hub_selected_secondary_draft_year = season;
        if (team_id != 0u) {
            g_kbo_hub_selected_team_id = team_id;
        }
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }

    int subview = atoi(text);
    if (subview >= 0 && subview < KBO_HUB_SECONDARY_DRAFT_SUBVIEW_COUNT) {
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SECONDARY_DRAFT;
        g_kbo_hub_selected_secondary_draft_subview = subview;
        g_kbo_hub_open_dropdown = 0;
    }
    kbo_webview_navigate_current();
    return 1;
}
