#ifndef KBO_HOTKEY_WINDOW_UI_ASIAN_GAMES_VIEW_INTERNAL_H
#define KBO_HOTKEY_WINDOW_UI_ASIAN_GAMES_VIEW_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../custom_events/asian_games/roster/asian_games_roster_store.h"
#include "../../../custom_events/asian_games_schedule_seed/state/state_and_paths.h"
#include "../../../custom_events/asian_games_schedule_seed/query/query_helpers.h"
#include "../../../custom_events/asian_games/state/asian_games_state.h"
#include "../../../custom_events/runtime/lookup/custom_event_lookup.h"
#include "../../../custom_events/runtime/markers/custom_event_markers.h"
#include "../../../custom_events/runtime/state/custom_event_state.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../support/assets/names/support_names.h"
#include "../../support/assets/paths/ui_asset_paths.h"
#include "ui_asian_games_view.h"
#include "../../support/text/date/ui_date_format.h"
#include "../../support/text/language/ui_language.h"
#include "../../support/assets/nations/ui_nation_helpers.h"
#include "../../support/roster/cells/ui_roster_cells.h"
#include "../../support/text/buffer/ui_text_buffer.h"
#include "../../support/assets/names/ui_uniform_numbers.h"
#include "../../ui_html_helpers/position_helpers.h"

int kbo_webview_asian_games_schedule(KboAsianGamesScheduleSeed* out);
void kbo_webview_format_asian_games_date(uint32_t yyyymmdd, char* out, size_t out_size);
void kbo_webview_format_asian_games_date_range(uint32_t start, uint32_t end, char* out, size_t out_size);
const char* kbo_webview_asian_games_tournament_status_class(const KboAsianGamesScheduleSeed* schedule);
const char* kbo_webview_asian_games_tournament_phase(
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t today,
    const char** out_class);
void kbo_webview_append_asian_games_tournament_row(
    KboWindowTextBuffer* buffer,
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t today);
void kbo_webview_append_asian_games_tournaments_view(KboWindowTextBuffer* buffer);
const char* kbo_webview_asian_games_history_result_label(uint8_t result);
int kbo_webview_weekday_for_yyyymmdd(uint32_t yyyymmdd);
const char* kbo_webview_asian_games_schedule_status(
    uint32_t event_date,
    KboCustomEventKind event_kind,
    uint32_t fired_date,
    uint32_t today,
    int event_exists,
    int auto_schedule,
    const char** out_class);
void kbo_webview_append_asian_games_schedule_row(
    KboWindowTextBuffer* buffer,
    uint32_t event_date,
    KboCustomEventKind event_kind,
    const char* event_label,
    const char* action_text,
    const char* impact_text,
    uint32_t fired_date,
    uint32_t today,
    uint32_t league_id,
    int auto_schedule);
void kbo_webview_append_asian_games_schedule_view(KboWindowTextBuffer* buffer);
void kbo_webview_append_asian_games_view(
    KboWindowTextBuffer* buffer,
    int selected_agames_subview,
    uint32_t* selected_roster_year);

#endif
