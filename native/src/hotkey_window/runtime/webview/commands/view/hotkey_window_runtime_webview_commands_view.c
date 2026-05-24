#include "hotkey_window_runtime_webview_commands_view_internal.h"
#include "../../../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../../../competitive_balance_tax/exceptions/cbt_exceptions.h"
#include "../../../../../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../../../../../core/dates/constants/kbo_date_constants.h"

static int kbo_webview_cbt_exception_window_open(uint32_t season)
{
    uint32_t year = 0, month = 0, day = 0;
    if (!kbo_current_date_tick_latest_components(&year, &month, &day)
            || year != season) {
        return 0;
    }

    uint32_t current_date = year * 10000u + month * 100u + day;
    return kbo_cbt_exception_designation_window_open(season, current_date);
}

static void kbo_webview_cbt_exception_player_name(
    uint32_t season,
    uint32_t team_id,
    const char* player_key,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) return;
    out[0] = '\0';
    if (player_key == NULL || player_key[0] == '\0') return;

    KboFaSalarySnapshotGrade* grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (grades == NULL) return;

    int count = kbo_fa_salary_snapshot_load_grade_rows(season, grades, KBO_FA_SALARY_SNAPSHOT_GRADE_MAX, NULL, 0);
    for (int i = 0; i < count; i++) {
        if (grades[i].ranking_team_id == team_id && strcmp(grades[i].player_key, player_key) == 0) {
            snprintf(out, out_size, "%s", grades[i].player_name);
            break;
        }
    }
    HeapFree(GetProcessHeap(), 0, grades);
}

static int kbo_webview_handle_cbt_exception_command(const char* cmd)
{
    if (strncmp(cmd, "cbt_exception/set/", 18) == 0) {
        uint32_t season = 0u;
        uint32_t team_id = 0u;
        char player_key[64] = {0};
        int parsed = sscanf(cmd + 18, "%u/%u/%63[A-Za-z0-9_.-]", &season, &team_id, player_key);
        int window_open = parsed == 3 ? kbo_webview_cbt_exception_window_open(season) : 0;
        int action_allowed = parsed == 3
            ? kbo_webview_team_action_allowed(team_id, "hub_cbt_exception_set")
            : 0;
        if (parsed == 3 && window_open && action_allowed) {
            char player_name[96];
            kbo_webview_cbt_exception_player_name(season, team_id, player_key, player_name, sizeof(player_name));
            if (kbo_cbt_exception_save_designation(season, team_id, player_key, player_name)) {
                kbo_log_runtimef(
                    "KBO CBT exception UI set season=%u team=%u player_key=%s player_name=%s",
                    season,
                    team_id,
                    player_key,
                    player_name);
            } else {
                kbo_log_runtimef(
                    "KBO CBT exception UI set failed season=%u team=%u player_key=%s reason=save_rejected",
                    season,
                    team_id,
                    player_key);
            }
        } else {
            kbo_log_runtimef(
                "KBO CBT exception UI set ignored parsed=%d season=%u team=%u player_key=%s window_open=%d action_allowed=%d",
                parsed,
                season,
                team_id,
                player_key,
                window_open,
                action_allowed);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_CBT;
        g_kbo_hub_selected_cbt_subview = KBO_HUB_CBT_SUBVIEW_EXCEPTIONS;
        g_kbo_hub_selected_team_id = team_id;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }

    if (strncmp(cmd, "cbt_exception/clear/", 20) == 0) {
        uint32_t season = 0u;
        uint32_t team_id = 0u;
        int parsed = sscanf(cmd + 20, "%u/%u", &season, &team_id);
        int window_open = parsed == 2 ? kbo_webview_cbt_exception_window_open(season) : 0;
        int action_allowed = parsed == 2
            ? kbo_webview_team_action_allowed(team_id, "hub_cbt_exception_clear")
            : 0;
        if (parsed == 2 && window_open && action_allowed) {
            if (kbo_cbt_exception_clear_designation(season, team_id)) {
                kbo_log_runtimef("KBO CBT exception UI clear season=%u team=%u", season, team_id);
            } else {
                kbo_log_runtimef("KBO CBT exception UI clear failed season=%u team=%u", season, team_id);
            }
        } else {
            kbo_log_runtimef(
                "KBO CBT exception UI clear ignored parsed=%d season=%u team=%u window_open=%d action_allowed=%d",
                parsed,
                season,
                team_id,
                window_open,
                action_allowed);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_CBT;
        g_kbo_hub_selected_cbt_subview = KBO_HUB_CBT_SUBVIEW_EXCEPTIONS;
        g_kbo_hub_selected_team_id = team_id;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }

    return 0;
}

static int kbo_webview_handle_futures_offer_command(const char* cmd)
{
    const char* submit_prefix = "futures-offer/submit/";
    const char* cancel_prefix = "futures-offer/cancel/";
    int cancel = 0;
    const char* text = NULL;
    if (strncmp(cmd, submit_prefix, strlen(submit_prefix)) == 0) {
        text = cmd + strlen(submit_prefix);
    } else if (strncmp(cmd, cancel_prefix, strlen(cancel_prefix)) == 0) {
        text = cmd + strlen(cancel_prefix);
        cancel = 1;
    } else {
        return 0;
    }

    uint32_t buyer_team_id = (uint32_t)strtoul(text, NULL, 10);
    const char* slash = strchr(text, '/');
    uint32_t seller_team_id = slash != NULL ? (uint32_t)strtoul(slash + 1, NULL, 10) : 0u;
    const char* second_slash = slash != NULL ? strchr(slash + 1, '/') : NULL;
    uint32_t player_id = second_slash != NULL ? (uint32_t)strtoul(second_slash + 1, NULL, 10) : 0u;

    int submit_result = KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_INVALID;
    if (buyer_team_id != 0u && seller_team_id != 0u && player_id != 0u) {
        if (cancel) {
            if (kbo_webview_team_action_allowed(buyer_team_id, "hub_independent_acquisition_offer_cancel")) {
                submit_result = kbo_independent_acquisition_ui_cancel_offer(
                    buyer_team_id,
                    seller_team_id,
                    player_id,
                    "hub_independent_acquisition_offer_cancel");
            } else {
                submit_result = KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_BLOCKED;
            }
        } else if (kbo_webview_team_action_allowed(buyer_team_id, "hub_independent_acquisition_offer")) {
            submit_result = kbo_independent_acquisition_ui_submit_offer(
                buyer_team_id,
                seller_team_id,
                player_id,
                "hub_independent_acquisition_offer");
        } else {
            submit_result = KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_BLOCKED;
        }
    }

    kbo_log_runtimef(
        "independent acquisition UI command action=%s buyer=%u seller=%u player=%u result=%d",
        cancel ? "cancel" : "submit",
        buyer_team_id,
        seller_team_id,
        player_id,
        submit_result);
    g_kbo_hub_selected_view = KBO_HUB_VIEW_FUTURES_LEAGUE;
    if (cancel) {
        g_kbo_hub_selected_futures_subview = KBO_HUB_FUTURES_SUBVIEW_PENDING;
    } else {
        g_kbo_hub_selected_futures_subview = KBO_HUB_FUTURES_SUBVIEW_OFFER;
    }
    g_kbo_hub_open_dropdown = 0;
    kbo_webview_navigate_current();
    return 1;
}

int kbo_webview_handle_view_navigation_command(const char* cmd)
{

    if (kbo_webview_handle_cbt_exception_command(cmd)) {
        return 1;
    }
    if (kbo_webview_handle_futures_offer_command(cmd)) {
        return 1;
    }
    if (kbo_webview_handle_secondary_draft_command(cmd)) {
        return 1;
    }

    if (strncmp(cmd, "view/", 5) == 0) {
        int view = atoi(cmd + 5);
        if (view >= 0 && view < KBO_HUB_NAV_COUNT) {
            if (!kbo_hub_view_available_for_selected_league(view)) {
                view = KBO_HUB_VIEW_MOD_INFO;
            }
            if (view == KBO_HUB_VIEW_FOREIGN_RIGHTS) {
                g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_QUOTA;
                g_kbo_hub_selected_foreign_subview = KBO_HUB_FOREIGN_SUBVIEW_RIGHTS;
            } else if (view == KBO_HUB_VIEW_UPCOMING_FA) {
                g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
                g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
            } else {
                g_kbo_hub_selected_view = view;
            }
            g_kbo_hub_open_dropdown = 0;
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
                    && (g_kbo_hub_selected_foreign_subview < 0
                        || g_kbo_hub_selected_foreign_subview >= KBO_HUB_FOREIGN_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_foreign_subview = KBO_HUB_FOREIGN_SUBVIEW_ROSTER;
            }
            if (view == KBO_HUB_VIEW_ASIAN_GAMES
                    && (g_kbo_hub_selected_agames_subview < 0
                        || g_kbo_hub_selected_agames_subview >= KBO_HUB_AGAMES_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_agames_subview = KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS;
            }
            if (view == KBO_HUB_VIEW_MILITARY
                    && (g_kbo_hub_selected_military_subview < 0
                        || g_kbo_hub_selected_military_subview >= KBO_HUB_MILITARY_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_military_subview = KBO_HUB_MILITARY_SUBVIEW_ROSTER;
            }
            if (view == KBO_HUB_VIEW_MOD_INFO
                    && (g_kbo_hub_selected_mod_subview < 0
                        || g_kbo_hub_selected_mod_subview >= KBO_HUB_MOD_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_README;
            }
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES
                    && (g_kbo_hub_selected_fa_subview < 0
                        || g_kbo_hub_selected_fa_subview >= KBO_HUB_FA_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
            }
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_CBT
                    && (g_kbo_hub_selected_cbt_subview < 0
                        || g_kbo_hub_selected_cbt_subview >= KBO_HUB_CBT_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_cbt_subview = KBO_HUB_CBT_SUBVIEW_OVERVIEW;
            }
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FUTURES_LEAGUE
                    && (g_kbo_hub_selected_futures_subview < 0
                        || g_kbo_hub_selected_futures_subview >= KBO_HUB_FUTURES_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_futures_subview = KBO_HUB_FUTURES_SUBVIEW_OFFER;
            }
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_SECONDARY_DRAFT
                    && (g_kbo_hub_selected_secondary_draft_subview < 0
                        || g_kbo_hub_selected_secondary_draft_subview >= KBO_HUB_SECONDARY_DRAFT_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_secondary_draft_subview = KBO_HUB_SECONDARY_DRAFT_SUBVIEW_SCHEDULE;
            }
            if (g_kbo_hub_selected_view == KBO_HUB_VIEW_SETTINGS
                    && (g_kbo_hub_selected_settings_subview < 0
                        || g_kbo_hub_selected_settings_subview >= KBO_HUB_SETTINGS_SUBVIEW_COUNT)) {
                g_kbo_hub_selected_settings_subview = KBO_HUB_SETTINGS_SUBVIEW_LEAGUE;
            }
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "secondary-draft/year/", 21) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        uint32_t year = (uint32_t)strtoul(cmd + 21, NULL, 10);
        if (year == 0u || (year >= KBO_SEASON_YEAR_MIN && year <= KBO_RECORD_YEAR_MAX)) {
            g_kbo_hub_selected_secondary_draft_year = year;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_SECONDARY_DRAFT;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "military/results/year/", 22) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        uint32_t year = (uint32_t)strtoul(cmd + 22, NULL, 10);
        if (year == 0u || (year >= KBO_SEASON_YEAR_MIN && year <= KBO_RECORD_YEAR_MAX)) {
            g_kbo_hub_selected_military_results_year = year;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MILITARY;
        g_kbo_hub_selected_military_subview = KBO_HUB_MILITARY_SUBVIEW_RESULTS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "military/", 9) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 9);
        if (subview >= 0 && subview < KBO_HUB_MILITARY_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MILITARY;
            g_kbo_hub_selected_military_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "foreign/", 8) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 8);
        if (subview >= 0 && subview < KBO_HUB_FOREIGN_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_QUOTA;
            g_kbo_hub_selected_foreign_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "cbt/", 4) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 4);
        if (subview >= 0 && subview < KBO_HUB_CBT_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_CBT;
            g_kbo_hub_selected_cbt_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "futures/", 8) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 8);
        if (subview >= 0 && subview < KBO_HUB_FUTURES_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_FUTURES_LEAGUE;
            g_kbo_hub_selected_futures_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "settings-tab/", 13) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 13);
        if (subview >= 0 && subview < KBO_HUB_SETTINGS_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_SETTINGS;
            g_kbo_hub_selected_settings_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    return 0;
}
