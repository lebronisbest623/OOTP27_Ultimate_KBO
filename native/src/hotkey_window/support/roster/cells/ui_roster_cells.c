#include <stdint.h>

#include "../../../runtime/hotkey_window_runtime_shared.h"
#include "../../../../captain/api/captain_selection.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"
#include "../../text/language/ui_language.h"
#include "ui_roster_cells.h"

static void kbo_webview_append_player_id_attrs(KboWindowTextBuffer* buffer, uint32_t player_id)
{
    if (buffer == NULL || player_id == 0u) {
        return;
    }
    kbo_window_text_appendf(buffer, " data-player-id='%u' data-kbo-player-hover='1'", player_id);
}

static int kbo_webview_player_is_selected_team_captain(uint32_t player_id)
{
    if (player_id == 0u || g_kbo_hub_selected_team_id == 0u || g_kbo_hub_selected_league_id == 0u) {
        return 0;
    }

    uint32_t current_year = 0u;
    uint32_t current_month = 0u;
    uint32_t current_day = 0u;
    if (!kbo_current_date_tick_latest_components(&current_year, &current_month, &current_day)
            || current_year == 0u) {
        return 0;
    }

    return kbo_captain_player_is_team_captain(
        current_year,
        g_kbo_hub_selected_league_id,
        g_kbo_hub_selected_team_id,
        player_id);
}

static void kbo_webview_append_captain_badge_if_needed(KboWindowTextBuffer* buffer, uint32_t player_id)
{
    if (buffer == NULL || !kbo_webview_player_is_selected_team_captain(player_id)) {
        return;
    }

    kbo_window_text_appendf(buffer, "<span class='captainBadge' title='");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\xa3\xbc\xec\x9e\xa5", "Captain"));
    kbo_window_text_appendf(buffer, "'>C</span>");
}

void kbo_webview_append_player_name_cell(KboWindowTextBuffer* buffer, const char* player_name, uint32_t player_id)
{
    kbo_window_text_appendf(buffer, "<td class='roName'");
    kbo_webview_append_player_id_attrs(buffer, player_id);
    kbo_window_text_appendf(buffer, "><span class='roNameInner'><span class='roNameText'>");
    kbo_html_append_escaped(buffer, player_name != NULL && player_name[0] != '\0' ? player_name : "Unknown player");
    kbo_window_text_appendf(buffer, "</span>");
    kbo_webview_append_captain_badge_if_needed(buffer, player_id);
    kbo_window_text_appendf(buffer, "</span></td>");
}

void kbo_webview_append_player_name_link_cell(
    KboWindowTextBuffer* buffer,
    const char* player_name,
    uint32_t player_id,
    const char* href_prefix)
{
    kbo_window_text_appendf(buffer, "<td class='roName'");
    kbo_webview_append_player_id_attrs(buffer, player_id);
    kbo_window_text_appendf(buffer, "><span class='roNameInner'><a href='%s%u'", href_prefix != NULL ? href_prefix : "#", player_id);
    kbo_webview_append_player_id_attrs(buffer, player_id);
    kbo_window_text_appendf(buffer, ">");
    kbo_html_append_escaped(buffer, player_name != NULL && player_name[0] != '\0' ? player_name : "Unknown player");
    kbo_window_text_appendf(buffer, "</a>");
    kbo_webview_append_captain_badge_if_needed(buffer, player_id);
    kbo_window_text_appendf(buffer, "</span></td>");
}

void kbo_webview_append_roster_top_bar(KboWindowTextBuffer* buffer, const char* right_text)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<div class='rosterTopBar'>");
    if (right_text != NULL && right_text[0] != '\0') {
        kbo_window_text_appendf(buffer, "<div class='rosterTopText'>");
        kbo_html_append_escaped(buffer, right_text);
        kbo_window_text_appendf(buffer, "</div>");
    }
    kbo_window_text_appendf(buffer, "</div>");
}
