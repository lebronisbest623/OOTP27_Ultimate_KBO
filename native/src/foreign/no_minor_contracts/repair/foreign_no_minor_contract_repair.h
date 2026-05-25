#ifndef KBOFIX_SRC_FOREIGN_NO_MINOR_CONTRACTS_REPAIR_FOREIGN_NO_MINOR_CONTRACT_REPAIR_H_
#define KBOFIX_SRC_FOREIGN_NO_MINOR_CONTRACTS_REPAIR_FOREIGN_NO_MINOR_CONTRACT_REPAIR_H_

#include <stdint.h>

typedef struct KboForeignNoMinorContractRepairResult {
    uint32_t player_id;
    uint32_t affiliate_team_id;
    uint32_t parent_team_id;
    uint32_t before_current_team_id;
    uint32_t before_active_team_id;
    uint32_t before_original_team_id;
    uint32_t before_default_team_id;
    uint32_t before_current_league_id;
    uint32_t before_draft_league_id;
    uint32_t before_original_league_id;
    uint8_t before_contract_level;
    uint8_t before_restricted;
    uint8_t before_secondary_restricted;
    uint8_t before_dfa;
    uint32_t after_current_team_id;
    uint32_t after_active_team_id;
    uint32_t after_original_team_id;
    uint32_t after_default_team_id;
    uint32_t after_current_league_id;
    uint32_t after_draft_league_id;
    uint32_t after_original_league_id;
    uint8_t after_contract_level;
    uint8_t after_restricted;
    uint8_t after_secondary_restricted;
    uint8_t after_dfa;
    int removed_affiliate_arrays;
    int removed_parent_restricted;
    int added_parent_assignment_arrays;
    int changed;
} KboForeignNoMinorContractRepairResult;

int kbo_foreign_no_minor_contract_repair_affiliate_assignment(
    uint8_t* player,
    uint8_t* affiliate_team,
    uint8_t* parent_team,
    KboForeignNoMinorContractRepairResult* out_result);

int kbo_foreign_no_minor_contract_repair_all(const char* source);

#endif
