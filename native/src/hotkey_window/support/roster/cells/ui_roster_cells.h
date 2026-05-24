#ifndef KBO_HOTKEY_WINDOW_UI_ROSTER_CELLS_H
#define KBO_HOTKEY_WINDOW_UI_ROSTER_CELLS_H

#include <stdint.h>

#include "../../text/buffer/ui_text_buffer.h"

void kbo_webview_append_player_name_cell(KboWindowTextBuffer* buffer, const char* player_name, uint32_t player_id);
void kbo_webview_append_player_name_link_cell(
    KboWindowTextBuffer* buffer,
    const char* player_name,
    uint32_t player_id,
    const char* href_prefix);
void kbo_webview_set_player_name_cell_captain_context(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint32_t captain_player_id,
    int lookup_performed);
void kbo_webview_clear_player_name_cell_captain_context(void);
void kbo_webview_append_roster_top_bar(KboWindowTextBuffer* buffer, const char* right_text);

#endif
