#include "../../hotkey_window_webview.h"
#include "../../../hotkey_window_domain_contract.h"
#include "../../../../../core/dates/constants/kbo_date_constants.h"

int kbo_webview_handle_event_and_fa_command(const char* cmd)
{

    const char* agames_roster_year_prefix = "agames/roster/year/";
    if (strncmp(cmd, agames_roster_year_prefix, strlen(agames_roster_year_prefix)) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        uint32_t year = (uint32_t)strtoul(cmd + strlen(agames_roster_year_prefix), NULL, 10);
        if (year == 0u || (year >= KBO_SEASON_YEAR_MIN && year <= KBO_RECORD_YEAR_MAX)) {
            g_kbo_hub_selected_agames_roster_year = year;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_GAMES;
        g_kbo_hub_selected_agames_subview = KBO_HUB_AGAMES_SUBVIEW_ROSTER;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }

    if (strncmp(cmd, "agames/", 7) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 7);
        if (subview >= 0 && subview < KBO_HUB_AGAMES_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_GAMES;
            g_kbo_hub_selected_agames_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-market/filter/", 17) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int filter = atoi(cmd + 17);
        if (filter < 0 || filter > 7) {
            filter = 0;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
        g_kbo_hub_fa_market_filter = filter;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-market/position/", 19) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int position_filter = atoi(cmd + 19);
        if (position_filter < 0 || position_filter > 14) {
            position_filter = 0;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
        g_kbo_hub_fa_market_position_filter = position_filter;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa/", 3) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 3);
        if (subview >= 0 && subview < KBO_HUB_FA_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
            g_kbo_hub_selected_fa_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/view/", 13) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 13);
        if (subview >= 0 && subview < KBO_HUB_FA_COMP_SUBVIEW_COUNT) {
            g_kbo_hub_selected_fa_compensation_subview = subview;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/select/", 15) == 0) {
        const char* text = cmd + 15;
        uint32_t fa_player_id = (uint32_t)strtoul(text, NULL, 10);
        const char* slash = strchr(text, '/');
        uint32_t selected_player_id = slash != NULL ? (uint32_t)strtoul(slash + 1, NULL, 10) : 0u;
        if (fa_player_id != 0u && selected_player_id != 0u) {
            uint32_t action_team_id = kbo_fa_compensation_original_team_for_player(fa_player_id);
            if (kbo_webview_team_action_allowed(action_team_id, "hub_manual_compensation_select")) {
                kbo_manual_select_fa_compensation_player(fa_player_id, selected_player_id, "hub_manual_compensation_select");
            }
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_selected_fa_compensation_subview = KBO_HUB_FA_COMP_SUBVIEW_DECISION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/cash-only/", 18) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 18, NULL, 10);
        if (fa_player_id != 0u) {
            uint32_t action_team_id = kbo_fa_compensation_original_team_for_player(fa_player_id);
            if (kbo_webview_team_action_allowed(action_team_id, "hub_cash_only_select")) {
                kbo_manual_select_fa_compensation_cash_only(fa_player_id, "hub_cash_only_select");
            }
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_selected_fa_compensation_subview = KBO_HUB_FA_COMP_SUBVIEW_DECISION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/submit/", 15) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 15, NULL, 10);
        if (fa_player_id != 0u) {
            uint32_t action_team_id = kbo_fa_compensation_signing_team_for_player(fa_player_id);
            if (kbo_webview_team_action_allowed(action_team_id, "hub_protected_list_submit")) {
                kbo_manual_submit_fa_compensation_protected_list(fa_player_id, "hub_protected_list_submit");
            }
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_selected_fa_compensation_subview = KBO_HUB_FA_COMP_SUBVIEW_DECISION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/detail/", 15) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 15, NULL, 10);
        if (fa_player_id != 0u) {
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_selected_fa_compensation_subview = KBO_HUB_FA_COMP_SUBVIEW_DECISION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/candidates/", 19) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 19, NULL, 10);
        if (fa_player_id != 0u) {
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_selected_fa_compensation_subview = KBO_HUB_FA_COMP_SUBVIEW_DECISION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    return 0;
}
