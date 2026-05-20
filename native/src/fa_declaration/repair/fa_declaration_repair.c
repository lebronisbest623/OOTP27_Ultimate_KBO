#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../fa_declaration_internal.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/csv/core_csv.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/dates/constants/kbo_date_constants.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/league_roles/kbo_league_roles.h"
#include "../../core/logging/core_log.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/assignment/assignment/team_assignment.h"
#include "../../team/lookup/team_lookup.h"

static int32_t kbo_fa_declaration_parse_i32_text(const char* text)
{
    if (text == NULL) {
        return 0;
    }
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    if (*text == '\0') {
        return 0;
    }

    char* tail = NULL;
    long value = strtol(text, &tail, 10);
    if (tail == text) {
        return 0;
    }
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

int kbo_fa_declaration_parse_decision_fields(
    KboFaDeclarationDecision* decision,
    char fields[][128],
    int field_count)
{
    if (decision == NULL || fields == NULL || field_count < 7) {
        return 0;
    }

    memset(decision, 0, sizeof(*decision));
    decision->declaration_date = kbo_csv_parse_u32_text(fields[0], 10);
    decision->season = kbo_csv_parse_u32_text(fields[1], 10);
    decision->player_id = kbo_csv_parse_u32_text(fields[2], 10);
    decision->declared = kbo_csv_parse_u32_text(fields[4], 10) != 0u ? 1u : 0u;
    decision->team_id = kbo_csv_parse_u32_text(fields[5], 10);
    decision->league_id = kbo_csv_parse_u32_text(fields[6], 10);
    if (field_count > 9) {
        uint32_t level = kbo_csv_parse_u32_text(fields[9], 10);
        decision->contract_level = level > 255u ? 255u : (uint8_t)level;
    }
    if (field_count > 10) {
        decision->salary = kbo_fa_declaration_parse_i32_text(fields[10]);
    }
    if (field_count > 11) {
        decision->fa_demand = kbo_fa_declaration_parse_i32_text(fields[11]);
    }
    if (field_count > 12) {
        decision->score = kbo_fa_declaration_parse_i32_text(fields[12]);
    }
    return decision->player_id != 0u && decision->declaration_date != 0u;
}

int kbo_fa_declaration_repair_retained_contract_salary(
    uint8_t* player,
    uint32_t season,
    const KboFaDeclarationDecision* decision,
    int32_t minimum_salary,
    const char* source)
{
    if (player == NULL
            || season == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET, sizeof(int32_t))
            || !memory_range_readable(
                player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET,
                OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t))
            || !memory_range_readable(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET, sizeof(uint8_t))
            || !memory_range_readable(player + OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET, sizeof(int32_t))) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (decision != NULL && decision->player_id != 0u && decision->player_id != player_id) {
        return 0;
    }

    int32_t repair_salary = 0;
    if (decision != NULL && decision->salary > repair_salary) {
        repair_salary = decision->salary;
    }
    if (minimum_salary > repair_salary) {
        repair_salary = minimum_salary;
    }
    if (repair_salary <= 0 && decision != NULL && decision->fa_demand > 0) {
        repair_salary = decision->fa_demand;
    }
    if (repair_salary <= 0) {
        return 0;
    }

    int changed = 0;
    int32_t* salaries = (int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t* start_year_ptr = (int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
    int32_t before_start_year = *start_year_ptr;
    int32_t start_year = before_start_year;

    int season_valid = season >= KBO_SEASON_YEAR_MIN && season <= KBO_SIM_YEAR_MAX;
    int start_year_valid = start_year >= (int32_t)KBO_SEASON_YEAR_MIN && start_year <= (int32_t)KBO_SIM_YEAR_MAX;
    uint32_t original_season_index = 0u;
    int original_season_index_valid = 0;
    if (start_year_valid && season >= (uint32_t)start_year) {
        uint32_t index = season - (uint32_t)start_year;
        if (index < OOTP27_PLAYER_CONTRACT_SALARY_YEARS) {
            original_season_index = index;
            original_season_index_valid = 1;
        }
    }

    int32_t before_y1 = salaries[0];
    int32_t before_season_salary = original_season_index_valid
        ? salaries[original_season_index]
        : salaries[0];
    uint32_t season_index = original_season_index;
    int season_index_valid = original_season_index_valid;
    int normalized_contract = 0;
    int future_slots_cleared = 0;
    uint8_t* contract_level_ptr = player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET;
    uint8_t before_contract_level = *contract_level_ptr;
    uint8_t* contract_total_years_ptr = player + OOTP27_PLAYER_CONTRACT_TOTAL_YEARS_OFFSET;
    uint8_t* contract_current_year_ptr = player + OOTP27_PLAYER_CONTRACT_CURRENT_YEAR_OFFSET;
    uint8_t before_contract_total_years = *contract_total_years_ptr;
    uint8_t before_contract_current_year = *contract_current_year_ptr;
    uint8_t* arbitration_status_ptr = player + OOTP27_PLAYER_ARBITRATION_STATUS_OFFSET;
    uint8_t* arbitration_tender_ptr = player + OOTP27_PLAYER_ARBITRATION_TENDER_FLAG_OFFSET;
    uint8_t before_arbitration_status = *arbitration_status_ptr;
    uint8_t before_arbitration_tender = *arbitration_tender_ptr;
    if (*contract_level_ptr == 0u) {
        uint8_t retained_level = decision != NULL && decision->contract_level != 0u
            ? decision->contract_level
            : 1u;
        *contract_level_ptr = retained_level;
        changed = 1;
    }
    if (season_valid && (!start_year_valid || start_year == (int32_t)season || original_season_index_valid)) {
        if (*start_year_ptr != (int32_t)season) {
            *start_year_ptr = (int32_t)season;
            normalized_contract = 1;
            changed = 1;
        }
        season_index = 0u;
        season_index_valid = 1;
    }
    if (season_index_valid && *contract_total_years_ptr != 1u) {
        *contract_total_years_ptr = 1u;
        changed = 1;
    }
    if (season_index_valid && *contract_current_year_ptr != 0u) {
        *contract_current_year_ptr = 0u;
        changed = 1;
    }
    if (season_index_valid) {
        for (uint32_t i = 0u; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
            int32_t target_salary = i == season_index ? repair_salary : 0;
            if (salaries[i] != target_salary) {
                if (i != season_index) {
                    future_slots_cleared++;
                }
                salaries[i] = target_salary;
                changed = 1;
            }
        }
    } else if (*contract_total_years_ptr == 0u) {
        *contract_total_years_ptr = 1u;
        changed = 1;
    }
    if (!season_index_valid && *contract_current_year_ptr > *contract_total_years_ptr) {
        *contract_current_year_ptr = *contract_total_years_ptr;
        changed = 1;
    }

    int32_t* offer = (int32_t*)(player + OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET);
    int32_t before_offer = *offer;
    if (*offer < repair_salary) {
        *offer = repair_salary;
        changed = 1;
    }
    int32_t* request = (int32_t*)(player + OOTP27_PLAYER_ARBITRATION_REQUEST_OFFSET);
    int32_t before_request = *request;
    if (*request != repair_salary) {
        *request = repair_salary;
        changed = 1;
    }
    if (*arbitration_status_ptr == 0u) {
        *arbitration_status_ptr = 1u;
        changed = 1;
    }
    if (*arbitration_tender_ptr == 0u) {
        *arbitration_tender_ptr = 1u;
        changed = 1;
    }

    if (changed) {
        static LONG repair_log_count = 0;
        LONG slot = InterlockedIncrement(&repair_log_count);
        if (slot <= 160) {
            kbo_log_runtimef(
                "KBO FA declaration deferred arbitration state repaired source=%s player=%u season=%u team=%u decision_date=%u start_year=%d->%d slot=%u slot_valid=%d normalized=%d future_cleared=%d salary_slot=%d->%d y1=%d->%d contract_level=%u->%u years=%u->%u current_year=%u->%u offer=%d->%d request=%d->%d arb_status=%u->%u tender=%u->%u repair_salary=%d decision_salary=%d demand=%d minimum=%d",
                source != NULL ? source : "",
                player_id,
                season,
                *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                decision != NULL ? decision->declaration_date : 0u,
                before_start_year,
                *start_year_ptr,
                season_index,
                season_index_valid,
                normalized_contract,
                future_slots_cleared,
                before_season_salary,
                salaries[season_index],
                before_y1,
                salaries[0],
                (unsigned)before_contract_level,
                (unsigned)*contract_level_ptr,
                (unsigned)before_contract_total_years,
                (unsigned)*contract_total_years_ptr,
                (unsigned)before_contract_current_year,
                (unsigned)*contract_current_year_ptr,
                before_offer,
                *offer,
                before_request,
                *request,
                (unsigned)before_arbitration_status,
                (unsigned)*arbitration_status_ptr,
                (unsigned)before_arbitration_tender,
                (unsigned)*arbitration_tender_ptr,
                repair_salary,
                decision != NULL ? decision->salary : 0,
                decision != NULL ? decision->fa_demand : 0,
                minimum_salary);
        }
    }

    return changed;
}

int kbo_fa_declaration_repair_retained_contracts_for_season(
    uint32_t season,
    const char* source)
{
    if (!kbo_fix_enabled()) {
        return 0;
    }
    if (season == 0u) {
        uint32_t today = 0u;
        if (kbo_current_date_tick_latest_published_date(&today) && today != 0u) {
            season = today / 10000u;
        }
    }
    if (season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_declaration_csv_path(path, sizeof(path))) {
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    KboFaDeclarationDecision decisions[KBO_FA_DECLARATION_MAX];
    memset(decisions, 0, sizeof(decisions));
    int decision_count = 0;
    int rows = 0;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[13][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 13);
        if (field_count < 7 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }

        KboFaDeclarationDecision row;
        if (!kbo_fa_declaration_parse_decision_fields(&row, fields, field_count)
                || row.season != season
                || row.declared != 0u) {
            continue;
        }
        rows++;

        int existing = -1;
        for (int i = 0; i < decision_count; i++) {
            if (decisions[i].player_id == row.player_id) {
                existing = i;
                break;
            }
        }
        if (existing >= 0) {
            if (row.declaration_date >= decisions[existing].declaration_date) {
                decisions[existing] = row;
            }
            continue;
        }
        if (decision_count < KBO_FA_DECLARATION_MAX) {
            decisions[decision_count++] = row;
        }
    }
    kbo_csv_reader_close(reader);

    int found = 0;
    int repaired = 0;
    int skipped_team = 0;
    int restored_team = 0;
    for (int i = 0; i < decision_count; i++) {
        uint32_t current_team_id = 0u;
        uint32_t current_league_id = 0u;
        uint8_t* player = kbo_find_player_by_id(
            decisions[i].player_id,
            &current_team_id,
            &current_league_id);
        if (player == NULL) {
            continue;
        }
        found++;
        if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID
                || decisions[i].team_id == 0u) {
            skipped_team++;
            continue;
        }
        if (current_team_id != decisions[i].team_id) {
            if (current_team_id == 0u) {
                uint8_t* team = find_kbo_team_by_numeric_id_any_league(decisions[i].team_id, 1);
                if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
                    int pre = 0;
                    int reg = 0;
                    int attach = 0;
                    uint32_t fallback_league_id = decisions[i].league_id != 0u
                        ? decisions[i].league_id
                        : kbo_league_role_main_league_id();
                    kbo_assign_player_to_team_like_ootp(player, team, fallback_league_id, &pre, &reg, &attach);
                    current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                    current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
                    if (current_team_id == decisions[i].team_id) {
                        restored_team++;
                    }
                }
            }
            if (current_team_id != decisions[i].team_id) {
                skipped_team++;
                continue;
            }
        }
        uint32_t repair_season = kbo_fa_declaration_retained_contract_season(decisions[i].season);
        if (repair_season == 0u) {
            repair_season = season;
        }
        repaired += kbo_fa_declaration_repair_retained_contract_salary(
            player,
            repair_season,
            &decisions[i],
            0,
            source != NULL ? source : "fa_declaration_retained_repair");
    }

    if (rows > 0 || repaired > 0) {
        kbo_log_runtimef(
            "KBO FA declaration deferred arbitration offer repair scan source=%s season=%u rows=%d unique=%d found=%d repaired=%d restored_team=%d skipped_team=%d csv=%s",
            source != NULL ? source : "",
            season,
            rows,
            decision_count,
            found,
            repaired,
            restored_team,
            skipped_team,
            path);
    }
    return repaired;
}
