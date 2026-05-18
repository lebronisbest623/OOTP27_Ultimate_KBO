#ifndef KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_NEWS_TRANSFER_H_
#define KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_NEWS_TRANSFER_H_

#include <stdint.h>

#include "../state/fa_compensation_state.h"

void kbo_emit_fa_compensation_obligation_news(
    const KboFaCompensationRecord* rec,
    uint32_t event_yyyymmdd,
    uint32_t protected_list_due_days);

void kbo_emit_fa_compensation_protected_list_submitted_news(
    const KboFaCompensationRecord* rec,
    uint32_t generated_yyyymmdd,
    uint32_t selection_due_days,
    int protected_count,
    int unprotected_count);

void kbo_emit_fa_compensation_player_selected_news(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    uint32_t decided_yyyymmdd);

void kbo_emit_fa_compensation_cash_only_news(
    const KboFaCompensationRecord* rec,
    uint32_t decided_yyyymmdd);

int kbo_transfer_fa_compensation_player_to_original_team(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    uint32_t transfer_yyyymmdd,
    const char* source);

#endif
