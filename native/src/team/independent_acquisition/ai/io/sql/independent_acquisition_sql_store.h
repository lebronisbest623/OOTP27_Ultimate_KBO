#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_IO_SQL_INDEPENDENT_ACQUISITION_SQL_STORE_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_IO_SQL_INDEPENDENT_ACQUISITION_SQL_STORE_H_

#include <stdint.h>

#include "../../independent_acquisition_ai_internal.h"

typedef struct KboIndependentAcquisitionSqlRequestRow {
    uint32_t date;
    uint32_t season;
    uint32_t buyer_team_id;
    uint32_t seller_team_id;
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t effective_before;
    uint32_t effective_after;
    uint32_t effective_limit;
    uint32_t injured_player_id;
    uint8_t pitcher;
    uint8_t asian_quota;
    int32_t cash_cost;
    int32_t value_score;
    int64_t request_score;
    char slot_type[32];
} KboIndependentAcquisitionSqlRequestRow;

typedef struct KboIndependentAcquisitionSqlDecisionRow {
    uint32_t date;
    uint32_t season;
    uint32_t seller_team_id;
    uint32_t player_id;
    uint32_t buyer_team_id;
    uint8_t transferred;
    int32_t value_score;
    int32_t cash_cost;
    int32_t old_cash;
    int32_t new_cash;
    int32_t seller_transfer_fee;
    int32_t seller_old_cash;
    int32_t seller_new_cash;
    int64_t request_score;
} KboIndependentAcquisitionSqlDecisionRow;

int kbo_independent_acquisition_sql_request_exists(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id);
int kbo_independent_acquisition_sql_cancel_request(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id);
int kbo_independent_acquisition_sql_append_request(
    uint32_t today,
    const KboIndependentAcquisitionCandidate* candidate,
    const KboIndependentAcquisitionBuyerState* buyer,
    const KboIndependentFuturesTeamLeague* seller,
    int32_t cash_cost,
    const char* slot_label,
    const char* source);
int kbo_independent_acquisition_sql_load_pending_requests(
    uint32_t season,
    KboIndependentAcquisitionQueuedRequest* out,
    int max_count);
int kbo_independent_acquisition_sql_load_request_rows(
    uint32_t season,
    uint32_t buyer_team_id,
    int pending_only,
    KboIndependentAcquisitionSqlRequestRow* out,
    int max_count);

int kbo_independent_acquisition_sql_decision_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id);
int kbo_independent_acquisition_sql_load_decision_keys(
    uint32_t season,
    KboIndependentAcquisitionDecisionKey* out,
    int max_count);
int kbo_independent_acquisition_sql_load_seller_transfer_summaries(
    uint32_t season,
    KboIndependentAcquisitionTransferSummary* out,
    int max_count);
int kbo_independent_acquisition_sql_load_buyer_transfer_summaries(
    uint32_t season,
    KboIndependentAcquisitionTransferSummary* out,
    int max_count);
int kbo_independent_acquisition_sql_transferred_count(
    uint32_t season,
    uint32_t team_id,
    int seller_side);
uint32_t kbo_independent_acquisition_sql_last_transfer_date(
    uint32_t season,
    uint32_t seller_team_id);
int kbo_independent_acquisition_sql_append_decision(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int transferred,
    int32_t old_cash,
    int32_t new_cash,
    int32_t seller_transfer_fee,
    int32_t seller_old_cash,
    int32_t seller_new_cash,
    const char* source);
int kbo_independent_acquisition_sql_load_decision_rows(
    uint32_t season,
    KboIndependentAcquisitionSqlDecisionRow* out,
    int max_count);

#endif
