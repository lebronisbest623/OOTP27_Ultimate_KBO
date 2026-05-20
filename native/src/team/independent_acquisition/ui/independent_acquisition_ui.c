#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ui.h"
#include "../ai/independent_acquisition_ai_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../foreign/injury/api/foreign_injury_labels.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../assignment/org_query/team_org_assignment_query.h"
#include "../../classification/team_classification.h"
#include "../../lookup/team_lookup.h"

static int kbo_independent_acquisition_ui_window_open(
    uint32_t today,
    uint32_t open_date,
    uint32_t* out_close_date)
{
    if (out_close_date != NULL) {
        *out_close_date = 0u;
    }
    if (today == 0u || open_date == 0u || today < open_date) {
        return 0;
    }

    (void)open_date;
    return kbo_independent_team_acquisition_window_active(today, NULL, NULL);
}

static uint32_t kbo_independent_acquisition_ui_effective_season(uint32_t today, uint32_t open_date)
{
    if (open_date != 0u && today >= open_date && open_date / 10000u == today / 10000u) {
        return open_date / 10000u;
    }
    return today / 10000u;
}

int kbo_independent_acquisition_ui_context(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiContext* out_context)
{
    if (out_context == NULL) {
        return 0;
    }
    memset(out_context, 0, sizeof(*out_context));
    out_context->buyer_team_id = buyer_team_id;
    out_context->policy_enabled = kbo_fix_enabled() && kbo_custom_foreign_policy_enabled();

    uint32_t today = 0u;
    if (kbo_current_date_tick_latest_published_date(&today)) {
        out_context->today = today;
    }
    if (out_context->today != 0u) {
        out_context->open_date = kbo_independent_team_acquisition_window_open_date_for_date(out_context->today);
        out_context->season = kbo_independent_acquisition_ui_effective_season(
            out_context->today,
            out_context->open_date);
    }
    out_context->window_open = out_context->policy_enabled
        && kbo_independent_acquisition_ui_window_open(
            out_context->today,
            out_context->open_date,
            &out_context->close_date);

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    out_context->seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        &out_context->seed_rows,
        &out_context->unresolved_seed_rows);

    uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(buyer_team_id, 1);
    if (buyer_team != NULL && memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        KboIndependentAcquisitionBuyerState buyer;
        kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
        if (buyer.team_id != 0u) {
            out_context->buyer_valid = 1;
            out_context->buyer_active_count = buyer.active_count;
            out_context->buyer_effective_foreign_count = buyer.effective_foreign_count;
            out_context->buyer_cash = buyer.cash_available;
        }
    }
    return out_context->today != 0u && out_context->season != 0u;
}
