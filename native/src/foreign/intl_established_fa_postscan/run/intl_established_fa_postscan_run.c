#include "../internal/intl_established_fa_postscan_internal.h"
#include "../../intl_established_fa/intl_established_fa_policy.h"

void kbo_intl_established_fa_postscan_run(const KboIntlEstablishedFaPostscanState* batch)
{
    if (batch == NULL || !kbo_fix_enabled()) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_log_runtimef(
            "international established FA postscan failed batch=%ld reason=no_player_vector expected=%d before_count=%d",
            batch->batch_id,
            batch->expected_count,
            batch->before_count);
        return;
    }

    char date[16] = {0};
    if (!kbo_current_history_date(date, sizeof(date), 2000, "intl_established_fa_postscan")) {
        strcpy_s(date, sizeof(date), "00000000");
    }

    char csv_path[MAX_PATH] = {0};
    HANDLE csv_file = INVALID_HANDLE_VALUE;
    int csv_ok = kbo_intl_established_fa_postscan_open_csv(&csv_file, csv_path, sizeof(csv_path));

    int matched = 0;
    int valid = 0;
    int foreign = 0;
    int asian = 0;
    int non_asian = 0;
    int teamless = 0;
    int league_match = 0;
    int market_candidate = 0;
    int market_block_team = 0;
    int market_block_retired = 0;
    int market_block_age = 0;
    int market_block_draft_pool = 0;
    int market_block_contract = 0;
    int market_block_demand = 0;
    int market_block_context = 0;
    int draft_eligible_cleared = 0;
    int market_normalized = 0;
    int original_league_seeded = 0;
    int draft_league_cleared = 0;
    int draft_fields_cleared = 0;
    int contract_level_cleared = 0;
    int demand_initialized = 0;
    int logged = 0;
    int quality_shaping_enabled = kbo_intl_established_fa_quality_shaping_enabled();
    int pitcher_count = 0;
    int asian_pitcher_count = 0;
    int non_asian_pitcher_count = 0;
    int asian_starter_pitcher_count = 0;
    int asian_bullpen_pitcher_count = 0;
    int non_asian_starter_pitcher_count = 0;
    int non_asian_bullpen_pitcher_count = 0;
    int quality_adjusted_count = 0;
    int asian_pitcher_adjusted_count = 0;
    int non_asian_pitcher_adjusted_count = 0;
    int starter_pitcher_adjusted_count = 0;
    int bullpen_pitcher_adjusted_count = 0;
    const KboIntlEstablishedFaPolicy* policy = kbo_intl_established_fa_policy();
    int64_t score_sum = 0;
    int64_t asian_score_sum = 0;
    int64_t non_asian_score_sum = 0;
    int32_t max_score = 0;
    int32_t asian_max_score = 0;
    int32_t non_asian_max_score = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_intl_established_fa_postscan_candidate_matches(batch, i, player_count, player)) {
            continue;
        }

        matched++;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        valid++;

        if (!kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }
        foreign++;

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint32_t original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
        uint32_t loan_league_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET);
        uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
        int is_asian = kbo_nation_is_asian_quota_candidate(nation_id);
        int32_t score = kbo_foreign_waiver_value_score(player);
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
        int32_t original_score = score;
        int32_t quality_cap = 0;
        int16_t quality_field_cap = 0;
        int was_quality_adjusted = 0;
        const char* quality_policy = "none";

        if (position_group == 1u) {
            pitcher_count++;
            if (is_asian) {
                asian_pitcher_count++;
                if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                    asian_starter_pitcher_count++;
                } else if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                    asian_bullpen_pitcher_count++;
                }
            } else {
                non_asian_pitcher_count++;
                if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                    non_asian_starter_pitcher_count++;
                } else if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                    non_asian_bullpen_pitcher_count++;
                }
            }

        }

        quality_policy = kbo_intl_established_fa_quality_policy_label(is_asian, position_group, position_role);
        quality_cap = kbo_intl_established_fa_quality_score_cap(is_asian, position_group, position_role);
        quality_field_cap = kbo_intl_established_fa_quality_field_cap(is_asian, position_group, position_role);
        if (quality_shaping_enabled && quality_cap > 0) {
            int32_t adjusted_score = score;
            was_quality_adjusted = kbo_intl_established_fa_apply_quality_cap(
                player,
                quality_cap,
                quality_field_cap,
                score,
                &adjusted_score);
            if (was_quality_adjusted) {
                score = adjusted_score;
                quality_adjusted_count++;
                if (is_asian) {
                    asian_pitcher_adjusted_count++;
                } else {
                    non_asian_pitcher_adjusted_count++;
                }
                if (position_group == 1u) {
                    if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                        starter_pitcher_adjusted_count++;
                    } else if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                        bullpen_pitcher_adjusted_count++;
                    }
                }
            }
        }

        uint8_t generation_flags = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_FLAGS_OFFSET);
        uint8_t generation_context = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET);
        uint8_t generation_grade = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_GRADE_OFFSET);
        uint8_t generation_special = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_SPECIAL_OFFSET);
        uint8_t draft_class = player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET];
        uint8_t draft_subtype = player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET];
        uint8_t draft_eligible = player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET];
        uint8_t draft_extra = player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET];
        uint8_t original_draft_eligible = draft_eligible;
        int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
        int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
        int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
        int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
        KboIntlEstablishedFaMarketNormalization market_norm = {0};
        kbo_intl_established_fa_normalize_market_state(
            player,
            batch->primary_league_id,
            batch->fallback_league_id,
            is_asian,
            score,
            &market_norm);
        if (market_norm.changed) {
            market_normalized++;
        }
        if (market_norm.original_league_seeded) {
            original_league_seeded++;
        }
        if (market_norm.draft_league_cleared) {
            draft_league_cleared++;
        }
        if (market_norm.draft_fields_cleared) {
            draft_fields_cleared++;
            if (market_norm.before_draft_eligible != 0u) {
                draft_eligible_cleared++;
            }
        }
        if (market_norm.contract_level_cleared) {
            contract_level_cleared++;
        }
        if (market_norm.demand_initialized) {
            demand_initialized++;
        }

        current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
        current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
        loan_league_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET);
        draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
        status_flags = player[OOTP27_PLAYER_STATUS_FLAGS_OFFSET];
        restricted_flag = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        secondary_restricted_flag = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        loan_active_flag = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
        dfa_flag = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        injury_active_flag = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
        contract_status = *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET);
        contract_start_year = *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
        contract_salary_y1 = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
        fa_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
        draft_class = player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET];
        draft_subtype = player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET];
        draft_eligible = player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET];
        draft_extra = player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET];
        overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
        talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
        ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
        career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
        score = kbo_foreign_waiver_value_score(player);
        int has_context = (batch->primary_league_id != 0u && current_league_id == batch->primary_league_id)
            || (batch->fallback_league_id != 0u && current_league_id == batch->fallback_league_id)
            || (batch->primary_league_id != 0u && draft_league_id == batch->primary_league_id)
            || (batch->fallback_league_id != 0u && draft_league_id == batch->fallback_league_id)
            || (batch->primary_league_id != 0u && original_league_id == batch->primary_league_id)
            || (batch->fallback_league_id != 0u && original_league_id == batch->fallback_league_id);
        int candidate_ok = current_team_id == 0u
            && active_team_id == 0u
            && *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == 0u
            && retired_flag == 0u
            && age >= policy->market_age_min
            && age <= policy->market_age_max
            && draft_eligible == 0u
            && fa_demand > 0
            && has_context;
        if (candidate_ok) {
            market_candidate++;
        } else {
            if (current_team_id != 0u) { market_block_team++; }
            if (retired_flag != 0u) { market_block_retired++; }
            if (age < policy->market_age_min || age > policy->market_age_max) { market_block_age++; }
            if (draft_eligible != 0u) { market_block_draft_pool++; }
            if (fa_demand <= 0) { market_block_demand++; }
            if (!has_context) { market_block_context++; }
        }

        if (current_team_id == 0u && active_team_id == 0u) {
            teamless++;
        }
        if (has_context) {
            league_match++;
        }

        score_sum += score;
        if (score > max_score) {
            max_score = score;
        }
        if (is_asian) {
            asian++;
            asian_score_sum += score;
            if (score > asian_max_score) {
                asian_max_score = score;
            }
        } else {
            non_asian++;
            non_asian_score_sum += score;
            if (score > non_asian_max_score) {
                non_asian_max_score = score;
            }
        }

        if (csv_ok) {
            kbo_intl_established_fa_postscan_write_csv_row(
                csv_file,
                date,
                batch->batch_id,
                batch->before_count,
                player_count,
                batch->expected_count,
                i,
                player);
        }

        if (logged < KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_DETAIL_LOGS) {
            kbo_log_runtimef(
                "international established FA postscan player batch=%ld index=%d player=%u nation=%u asian_quota=%d age=%d pos=%u/%u market_candidate=%d normalized=%d market_ready=%d block=team:%d retired:%d age:%d draft:%d contract:%d demand:%d context:%d team=%u active=%u original_team=%u league=%u original_league=%u loan_league=%u draft_league=%u before_draft_league=%u status=retired:%u flags:%u restricted:%u secondary:%u loan:%u dfa:%u injury:%u contract=level:%u before_level:%u status:%u start:%u salary:%d demand:%d before_demand:%d draft=class:%u subtype:%u eligible:%u original_eligible:%u extra:%u before_class:%u before_subtype:%u before_extra:%u gen=flags:%u context:%u grade:%u special:%u quality=policy:%s cap:%d field_cap:%d original:%d adjusted:%d changed:%d value=overall:%d talent:%d ratings:%d career:%d score=%d",
                batch->batch_id,
                i,
                player_id,
                nation_id,
                is_asian,
                (int)age,
                position_group,
                position_role,
                candidate_ok,
                market_norm.changed,
                market_norm.market_ready,
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
                market_norm.before_draft_league_id,
                retired_flag,
                status_flags,
                restricted_flag,
                secondary_restricted_flag,
                loan_active_flag,
                dfa_flag,
                injury_active_flag,
                contract_level,
                market_norm.before_contract_level,
                contract_status,
                contract_start_year,
                contract_salary_y1,
                fa_demand,
                market_norm.before_fa_demand,
                draft_class,
                draft_subtype,
                draft_eligible,
                original_draft_eligible,
                draft_extra,
                market_norm.before_draft_class,
                market_norm.before_draft_subtype,
                market_norm.before_draft_extra,
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
            logged++;
        }
    }

    if (csv_file != INVALID_HANDLE_VALUE) {
        CloseHandle(csv_file);
    }

    int avg = foreign > 0 ? (int)(score_sum / foreign) : 0;
    int asian_avg = asian > 0 ? (int)(asian_score_sum / asian) : 0;
    int non_asian_avg = non_asian > 0 ? (int)(non_asian_score_sum / non_asian) : 0;

    kbo_log_runtimef(
        "international established FA postscan summary batch=%ld date=%s before_count=%d after_count=%d before_max_player=%u original=%d expected=%d matched=%d valid=%d foreign=%d asian=%d non_asian=%d teamless=%d league_match=%d market_candidate=%d market_normalized=%d original_league_seeded=%d draft_league_cleared=%d draft_fields_cleared=%d contract_level_cleared=%d demand_initialized=%d market_block=team:%d retired:%d age:%d draft:%d contract:%d demand:%d context:%d draft_eligible_cleared=%d avg_score=%d asian_avg=%d non_asian_avg=%d max_score=%d asian_max=%d non_asian_max=%d quality_shaping=%d pitchers=%d asian_pitchers=%d non_asian_pitchers=%d asian_starters=%d asian_bullpen=%d non_asian_starters=%d non_asian_bullpen=%d adjusted=%d asian_adjusted=%d non_asian_adjusted=%d starter_adjusted=%d bullpen_adjusted=%d csv=%s",
        batch->batch_id,
        date,
        batch->before_count,
        player_count,
        batch->before_max_player_id,
        batch->original_count,
        batch->expected_count,
        matched,
        valid,
        foreign,
        asian,
        non_asian,
        teamless,
        league_match,
        market_candidate,
        market_normalized,
        original_league_seeded,
        draft_league_cleared,
        draft_fields_cleared,
        contract_level_cleared,
        demand_initialized,
        market_block_team,
        market_block_retired,
        market_block_age,
        market_block_draft_pool,
        market_block_contract,
        market_block_demand,
        market_block_context,
        draft_eligible_cleared,
        avg,
        asian_avg,
        non_asian_avg,
        max_score,
        asian_max_score,
        non_asian_max_score,
        quality_shaping_enabled ? 1 : 0,
        pitcher_count,
        asian_pitcher_count,
        non_asian_pitcher_count,
        asian_starter_pitcher_count,
        asian_bullpen_pitcher_count,
        non_asian_starter_pitcher_count,
        non_asian_bullpen_pitcher_count,
        quality_adjusted_count,
        asian_pitcher_adjusted_count,
        non_asian_pitcher_adjusted_count,
        starter_pitcher_adjusted_count,
        bullpen_pitcher_adjusted_count,
        csv_ok ? csv_path : "");
}

