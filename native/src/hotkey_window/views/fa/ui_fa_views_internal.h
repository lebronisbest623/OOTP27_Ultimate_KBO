#ifndef KBO_HOTKEY_WINDOW_UI_FA_VIEWS_INTERNAL_H
#define KBO_HOTKEY_WINDOW_UI_FA_VIEWS_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../fa_compensation/decisions/fa_compensation_decisions.h"
#include "../../../fa_compensation/history/fa_compensation_history.h"
#include "../../../fa_compensation/records/fa_compensation_records.h"
#include "../../../fa_compensation/state/fa_compensation_state.h"
#include "../../../fa_declaration/fa_declaration.h"
#include "../../../fa_market_classification/api/fa_market_classification.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../support/assets/names/support_names.h"
#include "../../support/assets/nations/ui_nation_helpers.h"
#include "../../support/assets/paths/ui_asset_paths.h"
#include "ui_fa_views.h"
#include "../../support/roster/cells/ui_roster_cells.h"
#include "../../support/text/buffer/ui_text_buffer.h"
#include "../../ui_html_helpers/position_helpers.h"

void kbo_webview_append_fa_cases_view(KboWindowTextBuffer* buffer, uint32_t selected_league_id);
int kbo_fa_compensation_record_is_final(const KboFaCompensationRecord* rec);
int kbo_fa_compensation_debug_row_count(
    const KboFaCompensationProtectionDebugRow* rows,
    int count,
    uint32_t fa_player_id);
const char* kbo_fa_compensation_decision_label(
    const KboFaCompensationRecord* rec,
    const KboFaCompensationDecisionRow* decision,
    int has_decision);
void kbo_webview_append_fa_compensation_view(
    KboWindowTextBuffer* buffer,
    int selected_compensation_subview,
    uint32_t selected_compensation_player_id);
void kbo_webview_append_fa_rights_exercise_view(KboWindowTextBuffer* buffer, uint32_t selected_league_id);
void kbo_webview_append_fa_view(
    KboWindowTextBuffer* buffer,
    int selected_fa_subview,
    int selected_fa_compensation_subview,
    uint32_t selected_compensation_player_id,
    uint32_t selected_league_id);

#endif
