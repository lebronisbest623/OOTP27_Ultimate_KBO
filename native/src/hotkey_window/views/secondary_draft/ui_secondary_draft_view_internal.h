#ifndef KBOFIX_SRC_HOTKEY_WINDOW_UI_SECONDARY_DRAFT_VIEW_INTERNAL_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_UI_SECONDARY_DRAFT_VIEW_INTERNAL_H_

#include <stdint.h>
#include <stddef.h>

#include "../../runtime/hotkey_window_runtime_shared.h"
#include "../../support/actions/ui_team_actions.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../custom_events/secondary_draft/secondary_draft.h"
#include "ui_secondary_draft_view.h"

uint32_t kbo_secondary_draft_ui_default_season(void);
void kbo_secondary_draft_ui_format_date(uint32_t yyyymmdd, char* out, size_t out_size);
void kbo_secondary_draft_ui_format_cash(int64_t amount, char* out, size_t out_size);
void kbo_secondary_draft_ui_append_empty_row(
    KboWindowTextBuffer* buffer,
    int colspan,
    const char* text);
void kbo_secondary_draft_ui_append_top_bar(
    KboWindowTextBuffer* buffer,
    const KboSecondaryDraftRunSummary* summary,
    const uint32_t* seasons,
    int season_count,
    uint32_t selected_season,
    int row_count);
void kbo_secondary_draft_ui_append_action_bar(
    KboWindowTextBuffer* buffer,
    uint32_t season,
    uint32_t team_id,
    int submitted,
    int saved_count,
    int action_available,
    const char* status_text);
void kbo_secondary_draft_ui_append_results_table(
    KboWindowTextBuffer* buffer,
    KboSecondaryDraftResultRow* rows,
    int row_count);

void kbo_secondary_draft_ui_append_schedule_view(
    KboWindowTextBuffer* buffer,
    const uint32_t* seasons,
    int season_count,
    uint32_t selected_season);
void kbo_secondary_draft_ui_append_list_view(
    KboWindowTextBuffer* buffer,
    uint32_t season,
    uint32_t team_id);
void kbo_secondary_draft_ui_append_draft_view(
    KboWindowTextBuffer* buffer,
    uint32_t season,
    uint32_t drafting_team_id,
    KboSecondaryDraftResultRow* result_rows,
    int result_count);

#endif
