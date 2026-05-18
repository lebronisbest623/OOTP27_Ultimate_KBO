#ifndef KBO_HOTKEY_WINDOW_UI_MILITARY_VIEW_INTERNAL_H
#define KBO_HOTKEY_WINDOW_UI_MILITARY_VIEW_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/dates/core_current_date.h"
#include "../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../core/news/live/core_live_news.h"
#include "../../../../custom_events/runtime/lookup/custom_event_lookup.h"
#include "../../../../custom_events/runtime/state/custom_event_state.h"
#include "../../../../custom_events/schedules/priority/foreign_priority_event_schedule.h"
#include "../../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../military_service/selection/draft/military_draft_queue.h"
#include "../../../../military_service/players/state/military_player_state.h"
#include "../../../../military_service/selection/events/military_selection_event.h"
#include "../../../../military_service/selection/results/military_selection_results_store.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/lookup/team_lookup.h"
#include "../../../support/assets/names/support_names.h"
#include "../api/ui_military_view.h"
#include "../../../support/roster/cells/ui_roster_cells.h"
#include "../../../support/text/buffer/ui_text_buffer.h"
#include "../../../support/assets/names/ui_uniform_numbers.h"
#include "../../../ui_html_helpers/position_helpers.h"

void kbo_webview_append_military_roster_view(KboWindowTextBuffer* buffer);
int kbo_military_resolve_application_window(
    uint32_t* out_today,
    uint32_t* out_anchor,
    uint32_t* out_announcement);
int kbo_military_applicant_position_bucket(uint8_t* player);
void kbo_military_refresh_applicants_for_hotkey_view(void);
void kbo_webview_append_military_applicant_summary(
    KboWindowTextBuffer* buffer,
    int application_active,
    int pending,
    int pitchers,
    int catchers,
    int infielders,
    int outfielders,
    int other,
    uint32_t anchor,
    uint32_t announcement);
void kbo_webview_append_military_applicants_view(KboWindowTextBuffer* buffer);
void kbo_webview_append_military_results_view(KboWindowTextBuffer* buffer, uint32_t* selected_results_year);
void kbo_webview_append_military_view(KboWindowTextBuffer* buffer, int selected_military_subview, uint32_t* selected_results_year);

#endif
