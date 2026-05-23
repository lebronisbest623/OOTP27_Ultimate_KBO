#ifndef KBOFIX_SRC_HOTKEY_WINDOW_UI_SECONDARY_DRAFT_VIEW_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_UI_SECONDARY_DRAFT_VIEW_H_

#include <stdint.h>

#include "../../support/text/buffer/ui_text_buffer.h"

#define KBO_HUB_SECONDARY_DRAFT_SUBVIEW_SCHEDULE 0
#define KBO_HUB_SECONDARY_DRAFT_SUBVIEW_LIST     1
#define KBO_HUB_SECONDARY_DRAFT_SUBVIEW_DRAFT    2
#define KBO_HUB_SECONDARY_DRAFT_SUBVIEW_COUNT    3

void kbo_webview_append_secondary_draft_view(
    KboWindowTextBuffer* buffer,
    int selected_subview,
    uint32_t* selected_season);

#endif
