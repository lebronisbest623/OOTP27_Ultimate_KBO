#include "../hotkey_window_runtime_content.h"
#include "../../hotkey_window_domain_contract.h"
#include "../../../../fa_market_classification/api/fa_market_classification.h"
#include "../../../../foreign/quota/counts/foreign_quota_counts.h"

void kbo_webview_append_asian_quota_view(KboWindowTextBuffer* buffer)
{
    int configured_nations = kbo_load_asian_quota_nation_ids_once();
    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    int counts_available = kbo_count_team_asian_quota_probe_cached(
        g_kbo_hub_selected_team_id,
        &team_foreign,
        &team_asian,
        &team_non_asian);
    uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);

    char summary_text[256] = {0};
    snprintf(
        summary_text,
        sizeof(summary_text),
        counts_available
            ? "View: Foreign Players - Raw %u / Asian %u / Non-Asian %u / Effective %u - AQ Nations: %d"
            : "View: Foreign Players - Raw - / Asian - / Non-Asian - / Effective - - AQ Nations: %d",
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective,
        configured_nations);

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable foreignRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>PO</th><th class='roNum' data-sort-type='number'></th><th class='roName' data-sort-type='text'>Name</th>"
        "<th class='roSlot' data-sort-type='text'>Slot</th><th class='roTeam' data-sort-type='text'>Team</th>"
        "<th class='roNat' data-sort-type='text'>Nationality*</th><th class='roAge' data-sort-type='number'>Age</th>"
        "<th class='roEntry' data-sort-type='number'>연봉</th><th class='roStatus' data-sort-type='text'>Status</th>"
        "</tr></thead><tbody>");

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    int rendered = 0;
    if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        for (int32_t i = 0; i < player_count && rendered < 500; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) { continue; }
            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
            if (player_id == 0u
                    || !kbo_player_is_foreign_for_kbo_rights(player)
                    || !kbo_player_current_assignment_matches_team_or_affiliate(player, g_kbo_hub_selected_team_id)) {
                continue;
            }

            char player_name[96] = {0};
            char uniform_number[8] = {0};
            char current_team_abbrev[16] = {0};
            char flags[96] = {0};
            char salary_text[32] = "-";
            uint16_t age = memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))
                ? *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)
                : 0u;
            int32_t salary = memory_range_readable(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET, sizeof(int32_t))
                ? *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET)
                : 0;
            kbo_fa_market_format_salary(salary, salary_text, sizeof(salary_text));
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            kbo_webview_copy_player_uniform_number(player_id, uniform_number, sizeof(uniform_number));
            kbo_hub_copy_team_abbrev_by_id(current_team_id, current_team_abbrev, sizeof(current_team_abbrev), NULL);
            snprintf(flags, sizeof(flags), "%s%s%s%s%s",
                player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
                player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
                player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
                player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
                player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
            if (flags[0] == '\0') { snprintf(flags, sizeof(flags), "Active"); }

            kbo_window_text_appendf(
                buffer,
                "<tr><td class='roPo'>%s</td><td class='roNum'>",
                kbo_webview_player_position_label(player, 0u));
            kbo_html_append_escaped(buffer, uniform_number);
            kbo_window_text_appendf(buffer, "</td>");
            kbo_webview_append_player_name_cell(buffer, player_name[0] != '\0' ? player_name : "Unknown player", player_id);
            kbo_window_text_appendf(buffer, "<td class='roSlot'>");
            kbo_html_append_escaped(buffer, kbo_hub_foreign_slot_code_for_player(player));
            kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
            kbo_html_append_escaped(buffer, current_team_abbrev[0] != '\0' ? current_team_abbrev : "-");
            kbo_window_text_appendf(buffer, "</td>");
            kbo_webview_append_roster_nation_cell(buffer, nation_id, kbo_hub_nation_flag_asset_path);
            if (age > 0u) {
                kbo_window_text_appendf(buffer, "<td class='roAge'>%u</td>", (uint32_t)age);
            } else {
                kbo_window_text_appendf(buffer, "<td class='roAge'></td>");
            }
            kbo_window_text_appendf(buffer, "<td class='roEntry' data-sort-value='%d'>", salary);
            kbo_html_append_escaped(buffer, salary_text);
            kbo_window_text_appendf(buffer, "</td><td class='roStatus'>");
            kbo_html_append_escaped(buffer, flags);
            kbo_window_text_appendf(buffer, "</td></tr>");
            rendered++;
        }
    }
    if (rendered == 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='9'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}

void kbo_webview_append_fallback_text_view(KboWindowTextBuffer* buffer)
{
            char text[32768] = {0};
            kbo_build_hub_window_text(text, sizeof(text));
            kbo_window_text_appendf(buffer, "<div class='card'><pre style=\"white-space:pre-wrap;margin:0;font-family:'Malgun Gothic';line-height:1.55\">");
            kbo_html_append_escaped(buffer, text);
            kbo_window_text_appendf(buffer, "</pre></div>");
}

void kbo_webview_append_selected_view(KboWindowTextBuffer* buffer, uint32_t current_year, const char* window_status)
{
    (void)current_year;
    if (buffer == NULL) {
        return;
    }

    if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MOD_INFO) {
        kbo_webview_append_mod_info_view(buffer, g_kbo_hub_selected_mod_subview);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY) {
        kbo_webview_append_military_view(buffer, g_kbo_hub_selected_military_subview, &g_kbo_hub_selected_military_results_year);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS) {
        kbo_webview_append_foreign_rights_view(buffer, window_status, g_kbo_hub_selected_team_id, &g_kbo_hub_selected_foreign_player_id);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA) {
        if (g_kbo_hub_selected_foreign_subview == KBO_HUB_FOREIGN_SUBVIEW_RIGHTS) {
            kbo_webview_append_foreign_rights_view(buffer, window_status, g_kbo_hub_selected_team_id, &g_kbo_hub_selected_foreign_player_id);
        } else {
            kbo_webview_append_asian_quota_view(buffer);
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES) {
        kbo_webview_append_asian_games_view(
            buffer,
            g_kbo_hub_selected_agames_subview,
            &g_kbo_hub_selected_agames_roster_year);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES) {
        kbo_webview_append_fa_view(
            buffer,
            g_kbo_hub_selected_fa_subview,
            g_kbo_hub_selected_fa_compensation_subview,
            g_kbo_hub_selected_fa_compensation_player_id,
            g_kbo_hub_selected_league_id);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_SETTINGS) {
        kbo_webview_append_settings_view(buffer, g_kbo_hub_selected_settings_subview);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_REPUTATION) {
        kbo_webview_append_reputation_view(buffer, g_kbo_hub_selected_league_id);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_CBT) {
        kbo_webview_append_cbt_view(buffer, g_kbo_hub_selected_cbt_subview, g_kbo_hub_selected_team_id);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FUTURES_LEAGUE) {
        kbo_webview_append_futures_league_view(
            buffer,
            g_kbo_hub_selected_futures_subview,
            g_kbo_hub_selected_team_id);
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_SECONDARY_DRAFT) {
        kbo_webview_append_secondary_draft_view(
            buffer,
            g_kbo_hub_selected_secondary_draft_subview,
            g_kbo_hub_selected_team_id,
            &g_kbo_hub_selected_secondary_draft_season);
    } else {
        kbo_webview_append_fallback_text_view(buffer);
    }
}
int kbo_hub_selected_league_is_kbo(void)
{
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    return kbo_league_id != 0u && g_kbo_hub_selected_league_id == kbo_league_id;
}

int kbo_hub_selected_league_is_amateur_reputation_league(void)
{
    return g_kbo_hub_selected_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID
        || g_kbo_hub_selected_league_id == KBO_COLLEGE_LEAGUE_ID;
}

int kbo_hub_view_available_for_selected_league(int view)
{
    if (view == KBO_HUB_VIEW_MOD_INFO) {
        return 1;
    }
    if (view == KBO_HUB_VIEW_REPUTATION) {
        return kbo_hub_selected_league_is_amateur_reputation_league();
    }
    return kbo_hub_selected_league_is_kbo();
}

void kbo_webview_append_main_tabs(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }
    static const int main_views[] = {
        KBO_HUB_VIEW_MOD_INFO,
        KBO_HUB_VIEW_ASIAN_QUOTA,
        KBO_HUB_VIEW_FUTURES_LEAGUE,
        KBO_HUB_VIEW_FA_CASES,
        KBO_HUB_VIEW_SECONDARY_DRAFT,
        KBO_HUB_VIEW_MILITARY,
        KBO_HUB_VIEW_ASIAN_GAMES,
        KBO_HUB_VIEW_CBT,
        KBO_HUB_VIEW_REPUTATION,
        KBO_HUB_VIEW_SETTINGS
    };
    for (size_t i = 0; i < sizeof(main_views) / sizeof(main_views[0]); i++) {
        int view = main_views[i];
        if (!kbo_hub_view_available_for_selected_league(view)) {
            continue;
        }
        kbo_window_text_appendf(buffer, "<a class='mainTab %s' href='kbo://view/%d'>",
            view == g_kbo_hub_selected_view ? "active" : "", view);
        kbo_html_append_escaped(buffer, kbo_hub_nav_label(view));
        kbo_window_text_appendf(buffer, "</a>");
    }
}

void kbo_webview_append_sub_tabs(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }
    if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MOD_INFO) {
        for (int i = 0; i < KBO_HUB_MOD_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://mod/%d'>",
                i == g_kbo_hub_selected_mod_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_mod_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY) {
        for (int i = 0; i < KBO_HUB_MILITARY_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://military/%d'>",
                i == g_kbo_hub_selected_military_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_military_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA) {
        for (int i = 0; i < KBO_HUB_FOREIGN_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://foreign/%d'>",
                i == g_kbo_hub_selected_foreign_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_foreign_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES) {
        for (int i = 0; i < KBO_HUB_AGAMES_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://agames/%d'>",
                i == g_kbo_hub_selected_agames_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_agames_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES) {
        for (int i = 0; i < KBO_HUB_FA_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://fa/%d'>",
                i == g_kbo_hub_selected_fa_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_fa_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_CBT) {
        for (int i = 0; i < KBO_HUB_CBT_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://cbt/%d'>",
                i == g_kbo_hub_selected_cbt_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_cbt_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_FUTURES_LEAGUE) {
        for (int i = 0; i < KBO_HUB_FUTURES_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://futures/%d'>",
                i == g_kbo_hub_selected_futures_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_futures_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_SECONDARY_DRAFT) {
        for (int i = 0; i < KBO_HUB_SECONDARY_DRAFT_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://secondary-draft/%d'>",
                i == g_kbo_hub_selected_secondary_draft_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_secondary_draft_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    } else if (g_kbo_hub_selected_view == KBO_HUB_VIEW_SETTINGS) {
        for (int i = 0; i < KBO_HUB_SETTINGS_SUBVIEW_COUNT; i++) {
            kbo_window_text_appendf(buffer, "<a class='subTab %s' href='kbo://settings-tab/%d'>",
                i == g_kbo_hub_selected_settings_subview ? "active" : "", i);
            kbo_html_append_escaped(buffer, kbo_hub_settings_subnav_label(i));
            kbo_window_text_appendf(buffer, "</a>");
        }
    }
}

const char* kbo_hub_cbt_subnav_label(int index)
{
    switch (index) {
        case KBO_HUB_CBT_SUBVIEW_OVERVIEW: return "개요";
        case KBO_HUB_CBT_SUBVIEW_HISTORY:  return "기록";
        case KBO_HUB_CBT_SUBVIEW_EXCEPTIONS: return "예외";
        case KBO_HUB_CBT_SUBVIEW_RULES:    return "규정";
        default: return "";
    }
}

int kbo_webview_current_view_has_sub_tabs(void)
{
    return g_kbo_hub_selected_view == KBO_HUB_VIEW_MOD_INFO
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_SETTINGS
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_CBT
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_FUTURES_LEAGUE
        || g_kbo_hub_selected_view == KBO_HUB_VIEW_SECONDARY_DRAFT;
}

