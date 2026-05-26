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

typedef struct KboForeignAiOfferDemandSalaryResult {
    int eligible;
    int changed;
    int generated_established_fa;
    int reserve_right;
    int32_t demand_salary;
    int32_t before_primary;
    int32_t before_first_year;
    int32_t after_primary;
    int32_t after_first_year;
} KboForeignAiOfferDemandSalaryResult;

typedef struct KboForeignAiOfferFinalGateSalaryResult {
    int eligible;
    int changed;
    int generated_established_fa;
    int reserve_right;
    int32_t demand_salary;
    int32_t before_salary_arg;
    int32_t after_salary_arg;
} KboForeignAiOfferFinalGateSalaryResult;

int kbo_foreign_ai_offer_contract_type_apply_bytes(
    uint8_t* offer,
    size_t offer_size,
    KboForeignAiOfferContractTypeResult* out_result);
int kbo_foreign_ai_offer_demand_salary_apply_bytes(
    uint8_t* offer,
    size_t offer_size,
    int32_t demand_salary,
    int generated_established_fa,
    int reserve_right,
    KboForeignAiOfferDemandSalaryResult* out_result);
int kbo_foreign_ai_offer_force_major_contract(
    uintptr_t player_ptr,
    uintptr_t offer_ptr,
    int32_t team_id_hint,
    const char* source);
int kbo_foreign_ai_offer_match_demand_salary(
    uintptr_t player_ptr,
    uintptr_t offer_ptr,
    int32_t team_id_hint,
    int reserve_right,
    const char* source);
int kbo_foreign_ai_offer_final_gate_salary_apply(
    int32_t salary_arg,
    int32_t demand_salary,
    int generated_established_fa,
    int reserve_right,
    KboForeignAiOfferFinalGateSalaryResult* out_result);
int32_t kbo_foreign_ai_offer_adjust_final_gate_salary_arg(
    uintptr_t player_ptr,
    uintptr_t offer_ptr,
    int32_t team_id_hint,
    int reserve_right,
    int32_t salary_arg,
    const char* source);

#endif
