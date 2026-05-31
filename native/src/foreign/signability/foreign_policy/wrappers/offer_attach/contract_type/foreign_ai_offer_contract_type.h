#ifndef KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_AI_OFFER_CONTRACT_TYPE_H_
#define KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_AI_OFFER_CONTRACT_TYPE_H_

#include <stddef.h>
#include <stdint.h>

typedef struct KboForeignAiOfferContractTypeResult {
    int eligible;
    int changed;
    uint8_t before_major;
    uint8_t before_minor;
    uint8_t after_major;
    uint8_t after_minor;
} KboForeignAiOfferContractTypeResult;

int kbo_foreign_ai_offer_contract_type_apply_bytes(
    uint8_t* offer,
    size_t offer_size,
    KboForeignAiOfferContractTypeResult* out_result);
int kbo_foreign_ai_offer_force_major_contract(
    uintptr_t player_ptr,
    uintptr_t offer_ptr,
    int32_t team_id_hint,
    const char* source);

#endif
