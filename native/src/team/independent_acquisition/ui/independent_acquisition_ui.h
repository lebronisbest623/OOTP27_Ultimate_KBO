#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_UI_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_UI_H_

#include <stdint.h>

#define KBO_INDEPENDENT_ACQUISITION_UI_MAX_OFFERS 256
#define KBO_INDEPENDENT_ACQUISITION_UI_MAX_ROWS   256

#define KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_OK        1
#define KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_DUPLICATE 2
#define KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_CLOSED   -1
#define KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_INVALID  -2
#define KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_DECIDED  -3
#define KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_NO_CASH  -4
#define KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_BLOCKED  -5
#define KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_FAILED   -6

#define KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_OK        1
#define KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_NOT_FOUND 0
#define KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_INVALID  -2
#define KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_DECIDED  -3
#define KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_BLOCKED  -5
#define KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_FAILED   -6

typedef struct KboIndependentAcquisitionUiContext {
    uint32_t today;
    uint32_t season;
    uint32_t buyer_team_id;
    uint32_t open_date;
    uint32_t close_date;
    int window_open;
    int policy_enabled;
    int buyer_valid;
    int seller_count;
    int seed_rows;
    int unresolved_seed_rows;
    uint32_t buyer_active_count;
    uint32_t buyer_effective_foreign_count;
    int32_t buyer_cash;
} KboIndependentAcquisitionUiContext;

typedef struct KboIndependentAcquisitionUiOfferRow {
    uintptr_t player_ptr;
    uint32_t player_id;
    uint32_t seller_team_id;
    uint32_t seller_league_id;
    uint32_t nation_id;
    uint32_t effective_before;
    uint32_t effective_after;
    uint32_t effective_limit;
    uint32_t injured_player_id;
    uint16_t age;
    uint8_t pitcher;
    uint8_t foreign_player;
    uint8_t asian_quota;
    uint8_t slot_type;
    uint8_t offer_blocked;
    uint8_t already_requested;
    uint8_t already_decided;
    int32_t value_score;
    int32_t cash_cost;
    int64_t request_score;
    char slot_label[32];
    char status_label[32];
} KboIndependentAcquisitionUiOfferRow;

typedef struct KboIndependentAcquisitionUiRequestRow {
    uintptr_t player_ptr;
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
    uint16_t age;
    uint8_t pitcher;
    uint8_t asian_quota;
    int32_t value_score;
    int32_t cash_cost;
    int64_t request_score;
    char slot_label[32];
} KboIndependentAcquisitionUiRequestRow;

typedef struct KboIndependentAcquisitionUiResultRow {
    KboIndependentAcquisitionUiRequestRow request;
    uint32_t decision_date;
    uint32_t winning_buyer_team_id;
    int32_t old_cash;
    int32_t new_cash;
    uint8_t transferred;
} KboIndependentAcquisitionUiResultRow;

int kbo_independent_acquisition_ui_context(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiContext* out_context);
int kbo_independent_acquisition_ui_collect_offer_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiOfferRow* out_rows,
    int max_rows,
    KboIndependentAcquisitionUiContext* out_context);
void kbo_independent_acquisition_ui_invalidate_offer_cache(void);
long kbo_independent_acquisition_ui_offer_cache_generation(void);
int kbo_independent_acquisition_ui_load_pending_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiRequestRow* out_rows,
    int max_rows);
int kbo_independent_acquisition_ui_load_result_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiResultRow* out_rows,
    int max_rows);
int kbo_independent_acquisition_ui_submit_offer(
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id,
    const char* source);
int kbo_independent_acquisition_ui_cancel_offer(
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id,
    const char* source);

#endif
