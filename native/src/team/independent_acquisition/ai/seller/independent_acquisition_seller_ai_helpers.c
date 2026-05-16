#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_seller_ai_helpers.h"

#include <stdint.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../runtime_memory/runtime_memory.h"

int64_t kbo_independent_acquisition_seller_fit_score(
    const KboIndependentAcquisitionQueuedRequest* request,
    uint8_t* player,
    uint8_t* buyer_team,
    int32_t cash_cost)
{
    if (request == NULL
            || player == NULL
            || buyer_team == NULL
            || cash_cost <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_independent_acquisition_team_has_cash(buyer_team, cash_cost)) {
        return INT64_MIN;
    }

    KboIndependentAcquisitionBuyerState buyer;
    kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
    if (buyer.team_id == 0u || buyer.team_id != request->buyer_team_id) {
        return INT64_MIN;
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    if (kbo_player_is_foreign_for_kbo_rights(player)) {
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            buyer.team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            return INT64_MIN;
        }
    }

    return kbo_independent_acquisition_score_candidate_for_buyer(
        &buyer,
        player,
        effective_before,
        effective_limit);
}

uint32_t kbo_independent_acquisition_seller_tiebreaker(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request)
{
    if (request == NULL) {
        return 0u;
    }

    uint32_t value = today
        ^ (request->buyer_team_id * 1103515245u)
        ^ (request->seller_team_id * 2246822519u)
        ^ (request->player_id * 3266489917u)
        ^ (request->date * 668265263u);
    value ^= value >> 16;
    value *= 2246822519u;
    value ^= value >> 13;
    value *= 3266489917u;
    value ^= value >> 16;
    return value;
}

static uint32_t kbo_independent_acquisition_date_serial(uint32_t yyyymmdd)
{
    if (yyyymmdd == 0u) {
        return 0u;
    }
    return kbo_date_serial(
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

int kbo_independent_acquisition_seller_pacing_deferred(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int seller_transfers,
    int seller_transfer_limit,
    uint32_t* out_window_age_days,
    uint32_t* out_target_day,
    uint32_t* out_request_age_days,
    uint32_t* out_days_remaining)
{
    if (request == NULL || seller_transfer_limit <= 1 || seller_transfers < 0) {
        return 0;
    }

    uint32_t today_serial = kbo_independent_acquisition_date_serial(today);
    uint32_t request_serial = kbo_independent_acquisition_date_serial(request->date);
    if (today_serial == 0u) {
        return 0;
    }

    uint32_t window_age_days = kbo_independent_team_acquisition_window_elapsed_days(today);
    uint32_t request_age_days =
        request_serial != 0u && today_serial >= request_serial ? today_serial - request_serial : 0u;
    uint32_t window_days = kbo_independent_team_acquisition_window_planning_days();
    uint32_t days_remaining = window_age_days < window_days ? window_days - window_age_days : 0u;
    uint32_t remaining_transfers = seller_transfer_limit > seller_transfers
        ? (uint32_t)(seller_transfer_limit - seller_transfers)
        : 0u;
    if (remaining_transfers <= 0u) {
        return 0;
    }

    uint32_t base_day =
        ((uint32_t)(seller_transfers + 1) * window_days) / (uint32_t)(seller_transfer_limit + 1);
    if (base_day < 2u) {
        base_day = 2u;
    }
    uint32_t jitter = kbo_independent_acquisition_seller_tiebreaker(
        request->season,
        request) % 3u;
    uint32_t target_day = base_day + jitter;
    if (jitter > 0u) {
        target_day--;
    }
    if (request_age_days >= 14u && target_day > 3u) {
        target_day--;
    }
    if (request_age_days >= 28u && target_day > 4u) {
        target_day--;
    }

    if (out_window_age_days != NULL) {
        *out_window_age_days = window_age_days;
    }
    if (out_target_day != NULL) {
        *out_target_day = target_day;
    }
    if (out_request_age_days != NULL) {
        *out_request_age_days = request_age_days;
    }
    if (out_days_remaining != NULL) {
        *out_days_remaining = days_remaining;
    }
    return window_age_days < target_day;
}

int kbo_independent_acquisition_seller_cooldown_deferred(
    uint32_t today,
    uint32_t last_transfer_date,
    uint32_t* out_days_since_transfer)
{
    if (last_transfer_date == 0u) {
        return 0;
    }

    uint32_t today_serial = kbo_independent_acquisition_date_serial(today);
    uint32_t last_serial = kbo_independent_acquisition_date_serial(last_transfer_date);
    if (today_serial == 0u || last_serial == 0u || today_serial < last_serial) {
        return 0;
    }

    uint32_t days_since = today_serial - last_serial;
    if (out_days_since_transfer != NULL) {
        *out_days_since_transfer = days_since;
    }
    return days_since < KBO_INDEPENDENT_ACQUISITION_SELLER_TRANSFER_COOLDOWN_DAYS;
}

int kbo_independent_acquisition_seller_strategy_deferred(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int seller_transfers,
    int seller_transfer_limit,
    int market_offer_count,
    int64_t selected_score,
    int64_t second_best_score,
    int32_t player_value_score,
    int64_t* out_reservation_score,
    int64_t* out_hold_value,
    uint32_t* out_window_age_days,
    uint32_t* out_request_age_days,
    uint32_t* out_days_remaining)
{
    if (request == NULL || seller_transfer_limit <= 0 || seller_transfers < 0
            || selected_score == INT64_MIN) {
        return 0;
    }

    uint32_t today_serial = kbo_independent_acquisition_date_serial(today);
    uint32_t request_serial = kbo_independent_acquisition_date_serial(request->date);
    if (today_serial == 0u) {
        return 0;
    }

    uint32_t window_age_days = kbo_independent_team_acquisition_window_elapsed_days(today);
    uint32_t request_age_days =
        request_serial != 0u && today_serial >= request_serial ? today_serial - request_serial : 0u;
    uint32_t window_days = kbo_independent_team_acquisition_window_planning_days();
    uint32_t days_remaining = window_age_days < window_days ? window_days - window_age_days : 0u;
    int remaining_transfers = seller_transfer_limit - seller_transfers;
    if (remaining_transfers <= 0) {
        return 0;
    }

    int64_t reservation_score = 205000ll;
    if (player_value_score > 0) {
        reservation_score += (int64_t)player_value_score / 4ll;
    }
    if (remaining_transfers <= 1) {
        reservation_score += 18000ll;
    } else if (remaining_transfers == 2) {
        reservation_score += 9000ll;
    }

    int competition = market_offer_count > 0 ? market_offer_count - 1 : 0;
    if (competition > 8) {
        competition = 8;
    }
    reservation_score -= (int64_t)competition * 6500ll;

    uint32_t aged_days = request_age_days > 30u ? 30u : request_age_days;
    reservation_score -= (int64_t)aged_days * 850ll;

    uint32_t market_days = window_age_days > 35u ? 35u : window_age_days;
    reservation_score -= (int64_t)market_days * 250ll;

    if (days_remaining <= 7u) {
        reservation_score -= (int64_t)(8u - days_remaining) * 2500ll;
    }
    if (market_offer_count <= 1 && request_age_days < 10u && days_remaining > 14u) {
        reservation_score += 18000ll;
    }
    if (market_offer_count >= 4 && request_age_days >= 18u) {
        reservation_score -= 12000ll;
    }
    if (reservation_score < 165000ll) {
        reservation_score = 165000ll;
    }

    int64_t hold_value = reservation_score;
    if (days_remaining > KBO_INDEPENDENT_ACQUISITION_SELLER_TRANSFER_COOLDOWN_DAYS) {
        int64_t option_value = 0ll;
        int competition_pressure = market_offer_count > 1 ? market_offer_count - 1 : 0;
        if (competition_pressure > 6) {
            competition_pressure = 6;
        }
        option_value += (int64_t)competition_pressure * 4500ll;

        uint32_t runway_days = days_remaining > 24u ? 24u : days_remaining;
        option_value += (int64_t)runway_days * 450ll;

        if (second_best_score > 0 && selected_score > second_best_score) {
            int64_t bid_gap = selected_score - second_best_score;
            if (bid_gap < 6000ll) {
                option_value += 16000ll;
            } else if (bid_gap < 14000ll) {
                option_value += 9000ll;
            } else if (bid_gap > 32000ll) {
                option_value -= 7000ll;
            }
        } else if (market_offer_count <= 1) {
            option_value += 18000ll;
        }

        if (remaining_transfers <= 2) {
            option_value += 7000ll;
        }
        if (option_value > 36000ll) {
            option_value = 36000ll;
        }
        if (option_value < -12000ll) {
            option_value = -12000ll;
        }
        hold_value += option_value;
    } else if (days_remaining <= 2u) {
        hold_value -= 12000ll;
    }

    if (out_reservation_score != NULL) {
        *out_reservation_score = reservation_score;
    }
    if (out_hold_value != NULL) {
        *out_hold_value = hold_value;
    }
    if (out_window_age_days != NULL) {
        *out_window_age_days = window_age_days;
    }
    if (out_request_age_days != NULL) {
        *out_request_age_days = request_age_days;
    }
    if (out_days_remaining != NULL) {
        *out_days_remaining = days_remaining;
    }
    return selected_score < hold_value;
}

int kbo_independent_acquisition_seller_abort_if_save(
    const char* source,
    const char* stage,
    uint32_t today)
{
    if (!kbo_runtime_save_in_progress()) {
        return 0;
    }

    kbo_log_runtimef(
        "independent acquisition seller AI aborted source=%s reason=save_in_progress stage=%s today=%u",
        source != NULL ? source : "",
        stage != NULL ? stage : "",
        today);
    return 1;
}

uint32_t kbo_independent_acquisition_seller_effective_season(uint32_t today)
{
    uint32_t open_date = kbo_independent_team_acquisition_window_open_date();
    if (open_date != 0u && today >= open_date) {
        return open_date / 10000u;
    }
    return today / 10000u;
}

