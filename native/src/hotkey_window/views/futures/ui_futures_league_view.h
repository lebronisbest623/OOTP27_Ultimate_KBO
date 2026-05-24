#ifndef KBOFIX_SRC_HOTKEY_WINDOW_UI_FUTURES_LEAGUE_VIEW_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_UI_FUTURES_LEAGUE_VIEW_H_

#include <stdint.h>

#include "../../support/text/buffer/ui_text_buffer.h"

#define KBO_HUB_FUTURES_SUBVIEW_OFFER   0
#define KBO_HUB_FUTURES_SUBVIEW_PENDING 1
#define KBO_HUB_FUTURES_SUBVIEW_RESULT  2
#define KBO_HUB_FUTURES_SUBVIEW_COUNT   3

void kbo_webview_append_futures_league_view(
    KboWindowTextBuffer* buffer,
    int selected_futures_subview,
    uint32_t selected_team_id);
void kbo_futures_offer_ui_snapshot_invalidate(void);

#endif
