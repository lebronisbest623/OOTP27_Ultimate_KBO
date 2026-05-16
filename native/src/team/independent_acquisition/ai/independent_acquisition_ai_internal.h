#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_INTERNAL_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_INTERNAL_H_

#include <stdint.h>

#include "../window/independent_acquisition_window.h"
#include "../../classification/team_classification.h"

#define KBO_INDEPENDENT_ACQUISITION_REQUEST_FILE "independent_acquisition_requests.jsonl"
#define KBO_INDEPENDENT_ACQUISITION_DECISION_FILE "independent_acquisition_decisions.jsonl"
#define KBO_INDEPENDENT_ACQUISITION_MAX_BUYERS 32
#define KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS 8
#define KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE 256
#define KBO_INDEPENDENT_ACQUISITION_BUYER_PENDING_LIMIT 2
#define KBO_INDEPENDENT_ACQUISITION_SELLER_DAILY_TRANSFER_LIMIT 1
#define KBO_INDEPENDENT_ACQUISITION_SELLER_TRANSFER_COOLDOWN_DAYS 5
#define KBO_INDEPENDENT_ACQUISITION_TEAM_FINANCIALS_BLOCK_OFFSET 0x2510u
#define KBO_INDEPENDENT_ACQUISITION_TEAM_FINANCIALS_CASH_OFFSET 0xc0u
#define KBO_INDEPENDENT_ACQUISITION_TEAM_FINANCIALS_READABLE_BYTES \
    (KBO_INDEPENDENT_ACQUISITION_TEAM_FINANCIALS_CASH_OFFSET + sizeof(int32_t))
#define KBO_INDEPENDENT_ACQUISITION_FINANCIAL_FIELD_ABS_LIMIT 2000000000

typedef struct KboIndependentAcquisitionBuyerState {
    uint32_t team_id;
    uint32_t league_id;
    uint32_t active_count;
    uint32_t asian_hitters;
    uint32_t asian_pitchers;
    uint32_t non_asian_hitters;
    uint32_t non_asian_pitchers;
    uint32_t effective_foreign_count;
    int32_t cash_available;
} KboIndependentAcquisitionBuyerState;

typedef struct KboIndependentAcquisitionCandidate {
    uintptr_t player_ptr;
    uint32_t player_id;
    uint32_t seller_team_id;
    uint32_t seller_league_id;
    uint32_t nation_id;
    uint8_t pitcher;
    uint8_t asian_quota;
    int32_t value_score;
    int64_t request_score;
    uint32_t effective_before;
    uint32_t effective_after;
    uint32_t effective_limit;
    uint8_t slot_type;
    uint32_t injured_player_id;
} KboIndependentAcquisitionCandidate;

typedef struct KboIndependentAcquisitionQueuedRequest {
    uint32_t date;
    uint32_t season;
    uint32_t buyer_team_id;
    uint32_t seller_team_id;
    uint32_t player_id;
    int64_t request_score;
    int32_t value_score;
    int32_t cash_cost;
} KboIndependentAcquisitionQueuedRequest;

int32_t* kbo_independent_acquisition_team_cash_ptr(uint8_t* team);
int32_t kbo_independent_acquisition_cash_cost_for_player(uint8_t* player);
int kbo_independent_acquisition_team_has_cash(uint8_t* team, int32_t cash_cost);
int kbo_independent_acquisition_charge_team_cash(
    uint8_t* team,
    int32_t cash_cost,
    int32_t* out_old_cash,
    int32_t* out_new_cash);
void kbo_independent_acquisition_read_buyer_state(
    uint8_t* team,
    KboIndependentAcquisitionBuyerState* out);
int kbo_independent_acquisition_player_status_ok(uint8_t* player);
int64_t kbo_independent_acquisition_score_candidate_for_buyer(
    const KboIndependentAcquisitionBuyerState* buyer,
    uint8_t* player,
    uint32_t effective_before,
    uint32_t effective_limit);
uintptr_t kbo_independent_acquisition_find_player_snapshot(
    const uintptr_t* player_snapshot,
    int32_t player_count,
    uint32_t player_id);
int kbo_independent_acquisition_choose_candidate_for_buyer(
    const uintptr_t* player_snapshot,
    int32_t player_count,
    const KboIndependentFuturesTeamLeague* sellers,
    int seller_count,
    const KboIndependentAcquisitionQueuedRequest* market_requests,
    int market_request_count,
    const KboIndependentAcquisitionBuyerState* buyer,
    KboIndependentAcquisitionCandidate* out_candidate);
int kbo_independent_acquisition_request_exists(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id);
int kbo_independent_acquisition_append_request(
    uint32_t today,
    const KboIndependentAcquisitionCandidate* candidate,
    const KboIndependentAcquisitionBuyerState* buyer,
    const KboIndependentFuturesTeamLeague* seller,
    const char* source);
int kbo_independent_acquisition_cancel_request(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id,
    const char* source);
int kbo_independent_acquisition_load_requests(
    uint32_t season,
    KboIndependentAcquisitionQueuedRequest* out,
    int max_count);
int kbo_run_independent_team_acquisition_seller_ai(
    uint32_t today,
    const uintptr_t* player_snapshot,
    int32_t player_count,
    const char* source);
int kbo_independent_acquisition_decision_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id);
int kbo_independent_acquisition_transferred_count(
    uint32_t season,
    uint32_t seller_team_id);
uint32_t kbo_independent_acquisition_last_transfer_date(
    uint32_t season,
    uint32_t seller_team_id);
int kbo_independent_acquisition_buyer_transferred_count(
    uint32_t season,
    uint32_t buyer_team_id);
int kbo_independent_acquisition_append_decision(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int transferred,
    int32_t old_cash,
    int32_t new_cash,
    const char* source);
int kbo_emit_independent_acquisition_transfer_news(
    uint32_t today,
    uint8_t* player,
    uint8_t* buyer_team,
    uint8_t* seller_team,
    uint32_t player_id,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    int32_t cash_cost,
    const char* source);

#endif
