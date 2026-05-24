#ifndef KBOFIX_SRC_HOTKEY_WINDOW_UI_FUTURES_OFFER_SNAPSHOT_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_UI_FUTURES_OFFER_SNAPSHOT_H_

#include <stdint.h>

#include "../../../../team/independent_acquisition/ui/independent_acquisition_ui.h"

typedef struct KboFuturesOfferUiSnapshotRow {
    uint32_t player_id;
    uint32_t seller_team_id;
    uint32_t nation_id;
    uint16_t age;
    uint8_t offer_blocked;
    uint8_t already_requested;
    uint8_t already_decided;
    int32_t cash_cost;
    char player_name[96];
    char seller_name[96];
    char position_label[24];
    char nation_label[64];
    char nation_abbrev[16];
    char slot_label[32];
    char status_label[32];
    char cash_text[32];
} KboFuturesOfferUiSnapshotRow;

typedef struct KboFuturesOfferUiSnapshot {
    uint32_t buyer_team_id;
    int count;
    KboIndependentAcquisitionUiContext context;
    KboFuturesOfferUiSnapshotRow rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_OFFERS];
} KboFuturesOfferUiSnapshot;

int kbo_futures_offer_ui_snapshot_get(
    uint32_t buyer_team_id,
    KboFuturesOfferUiSnapshot* out_snapshot,
    int* out_updating);
void kbo_futures_offer_ui_snapshot_invalidate(void);

#endif
