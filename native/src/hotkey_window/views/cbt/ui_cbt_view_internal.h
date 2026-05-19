#ifndef KBOFIX_UI_CBT_VIEW_INTERNAL_H_
#define KBOFIX_UI_CBT_VIEW_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../competitive_balance_tax/records/cbt_records.h"
#include "../../../competitive_balance_tax/exceptions/cbt_exceptions.h"
#include "../../../competitive_balance_tax/rules/cbt_rules.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../custom_events/runtime/markers/custom_event_markers.h"
#include "../../../custom_events/runtime/names/custom_event_names.h"
#include "../../../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../support/assets/paths/ui_asset_paths.h"
#include "../../support/assets/paths/ui_image_sources.h"
#include "../../support/assets/names/support_names.h"
#include "../../support/actions/ui_team_actions.h"
#include "../../support/roster/cells/ui_roster_cells.h"
#include "../../support/text/buffer/ui_text_buffer.h"
#include "ui_cbt_view.h"

void kbo_cbt_format_usd(char* out, size_t out_size, int32_t value);
int kbo_cbt_load_sorted(KboCbtRecord** out_records, int* out_count);
int kbo_cbt_is_latest_for_team(const KboCbtRecord* records, int index);
void kbo_cbt_team_name(const KboCbtRecord* rec, char* out, size_t out_size);
void kbo_webview_append_cbt_history_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id);
void kbo_webview_append_cbt_exceptions_view(KboWindowTextBuffer* buffer, uint32_t selected_team_id);
void kbo_webview_append_cbt_rules_view(KboWindowTextBuffer* buffer);

#endif
