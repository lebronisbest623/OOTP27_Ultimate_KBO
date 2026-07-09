#ifndef KBOFIX_SRC_PATCH_INSTALLERS_ARBITRATION_NO_WITHDRAW_ARBITRATION_NO_WITHDRAW_POLICY_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_ARBITRATION_NO_WITHDRAW_ARBITRATION_NO_WITHDRAW_POLICY_H_

static inline int kbo_salary_arbitration_should_block_non_tender_transition(
    int direct_block_candidate,
    int missing_declaration_transition,
    int official_zero_offer_transition)
{
    (void)missing_declaration_transition;
    return direct_block_candidate || official_zero_offer_transition;
}

#endif
