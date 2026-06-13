#ifndef KBOFIX_SRC_HOTKEY_WINDOW_VIEWS_SECONDARY_DRAFT_UI_SECONDARY_DRAFT_VIEW_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_VIEWS_SECONDARY_DRAFT_UI_SECONDARY_DRAFT_VIEW_H_

#include <stdint.h>

#include "../../support/text/buffer/ui_text_buffer.h"

#define KBO_HUB_SECONDARY_DRAFT_SUBVIEW_PROTECTION 0
#define KBO_HUB_SECONDARY_DRAFT_SUBVIEW_DRAFT      1
#define KBO_HUB_SECONDARY_DRAFT_SUBVIEW_RESULTS    2
#define KBO_HUB_SECONDARY_DRAFT_SUBVIEW_COUNT      3

#define KBO_HUB_SECONDARY_DRAFT_SUBVIEW_CANDIDATES KBO_HUB_SECONDARY_DRAFT_SUBVIEW_DRAFT

void kbo_webview_append_secondary_draft_view(
    KboWindowTextBuffer* buffer,
    int selected_subview,
    uint32_t selected_team_id,
    uint32_t* selected_season);

int kbo_secondary_draft_ui_auto_submit_team(uint32_t season, uint32_t team_id, const char* source);
int kbo_secondary_draft_ui_submit_team(uint32_t season, uint32_t team_id, const char* source);
int kbo_secondary_draft_ui_protect_player(
    uint32_t season,
    uint32_t team_id,
    uint32_t player_id,
    const char* source);
int kbo_secondary_draft_ui_run_draft(uint32_t season, uint32_t selected_team_id, const char* source);

#endif
