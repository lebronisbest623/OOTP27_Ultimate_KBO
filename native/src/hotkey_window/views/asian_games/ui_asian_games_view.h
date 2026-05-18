#ifndef KBO_HOTKEY_WINDOW_UI_ASIAN_GAMES_VIEW_H
#define KBO_HOTKEY_WINDOW_UI_ASIAN_GAMES_VIEW_H

#include <stdint.h>

#include "../../support/text/buffer/ui_text_buffer.h"

#define KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS 0
#define KBO_HUB_AGAMES_SUBVIEW_SCHEDULE    1
#define KBO_HUB_AGAMES_SUBVIEW_ROSTER      2
#define KBO_HUB_AGAMES_SUBVIEW_COUNT       3

void kbo_webview_append_asian_games_view(
    KboWindowTextBuffer* buffer,
    int selected_agames_subview,
    uint32_t* selected_roster_year);

#endif
