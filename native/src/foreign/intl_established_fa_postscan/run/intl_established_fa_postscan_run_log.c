#include "intl_established_fa_postscan_run_log.h"
#include "../../intl_established_fa/intl_established_fa_policy.h"

void kbo_intl_established_fa_postscan_log_player_detail(
    const KboIntlEstablishedFaPostscanState* batch,
    int32_t index,
    uint8_t* player,
    uint32_t player_id,
    uint32_t nation_id,
    int is_asian,
    int candidate_ok,
    int has_context,
    const KboIntlEstablishedFaMarketNormalization* market_norm,
    uint8_t original_draft_eligible,
    const char* quality_policy,
    int32_t quality_cap,
    int16_t quality_field_cap,
    int32_t original_score,
    int32_t score,
    int was_quality_adjusted)
{
    if (batch == NULL || player == NULL || market_norm == NULL) {
        return;
    }

    const KboIntlEstablishedFaPolicy* policy = kbo_intl_established_fa_policy();
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    uint32_t loan_league_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
    uint8_t status_flags = player[OOTP27_PLAYER_STATUS_FLAGS_OFFSET];
    uint8_t restricted_flag = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t secondary_restricted_flag = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t loan_active_flag = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
    uint8_t dfa_flag = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    uint8_t injury_active_flag = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    uint8_t contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    uint32_t contract_status = *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET);
    uint32_t contract_start_year = *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
    int32_t contract_salary_y1 = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t fa_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    uint8_t position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    uint8_t position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);
    uint8_t generation_flags = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_FLAGS_OFFSET);
    uint8_t generation_context = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET);
    uint8_t generation_grade = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_GRADE_OFFSET);
    uint8_t generation_special = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_SPECIAL_OFFSET);
    uint8_t draft_class = player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET];
    uint8_t draft_subtype = player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET];
    uint8_t draft_eligible = player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET];
    uint8_t draft_extra = player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET];
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);

    kbo_log_runtimef(
        "international established FA postscan player batch=%ld index=%d player=%u nation=%u asian_quota=%d age=%d pos=%u/%u market_candidate=%d normalized=%d market_ready=%d block=team:%d retired:%d age:%d draft:%d contract:%d demand:%d context:%d team=%u active=%u original_team=%u league=%u original_league=%u loan_league=%u draft_league=%u before_draft_league=%u status=retired:%u flags:%u restricted:%u secondary:%u loan:%u dfa:%u injury:%u contract=level:%u before_level:%u status:%u start:%u salary:%d demand:%d before_demand:%d draft=class:%u subtype:%u eligible:%u original_eligible:%u extra:%u before_class:%u before_subtype:%u before_extra:%u gen=flags:%u context:%u grade:%u special:%u quality=policy:%s cap:%d field_cap:%d original:%d adjusted:%d changed:%d value=overall:%d talent:%d ratings:%d career:%d score=%d",
        batch->batch_id,
        index,
        player_id,
        nation_id,
        is_asian,
        (int)age,
        position_group,
        position_role,
        candidate_ok,
        market_norm->changed,
        market_norm->market_ready,
        current_team_id != 0u,
        retired_flag != 0u,
        age < policy->market_age_min || age > policy->market_age_max,
        draft_eligible != 0u,
        0,
        fa_demand <= 0,
        !has_context,
        current_team_id,
        active_team_id,
        original_team_id,
        current_league_id,
        original_league_id,
        loan_league_id,
        draft_league_id,
        market_norm->before_draft_league_id,
        retired_flag,
        status_flags,
        restricted_flag,
        secondary_restricted_flag,
        loan_active_flag,
        dfa_flag,
        injury_active_flag,
        contract_level,
        market_norm->before_contract_level,
        contract_status,
        contract_start_year,
        contract_salary_y1,
        fa_demand,
        market_norm->before_fa_demand,
        draft_class,
        draft_subtype,
        draft_eligible,
        original_draft_eligible,
        draft_extra,
        market_norm->before_draft_class,
        market_norm->before_draft_subtype,
        market_norm->before_draft_extra,
        generation_flags,
        generation_context,
        generation_grade,
        generation_special,
        quality_policy,
        quality_cap,
        (int)quality_field_cap,
        original_score,
        score,
        was_quality_adjusted,
        overall,
        talent,
        ratings,
        career,
        score);
}
