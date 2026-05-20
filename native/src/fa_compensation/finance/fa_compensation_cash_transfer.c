#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_compensation_cash_transfer.h"

#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "sql/fa_compensation_cash_transfer_sql_store.h"

#define KBO_FA_COMP_TEAM_FINANCIALS_BLOCK_OFFSET 0x2510u
#define KBO_FA_COMP_TEAM_FINANCIALS_BUDGET_OFFSET 0x78u
#define KBO_FA_COMP_TEAM_FINANCIALS_SCOUTING_BUDGET_OFFSET 0x90u
#define KBO_FA_COMP_TEAM_FINANCIALS_DEVELOPMENT_BUDGET_OFFSET 0x94u
#define KBO_FA_COMP_TEAM_FINANCIALS_DRAFT_BUDGET_OFFSET 0x98u
#define KBO_FA_COMP_TEAM_FINANCIALS_DRAFT_EXPENSES_OFFSET 0x9cu
#define KBO_FA_COMP_TEAM_FINANCIALS_CASH_OFFSET 0xc0u
#define KBO_FA_COMP_TEAM_FINANCIALS_READABLE_BYTES (KBO_FA_COMP_TEAM_FINANCIALS_CASH_OFFSET + sizeof(int32_t))
#define KBO_FA_COMP_FINANCIAL_FIELD_ABS_LIMIT 2000000000

static int kbo_fa_comp_abs_i32_plausible(int32_t value)
{
    return value > -KBO_FA_COMP_FINANCIAL_FIELD_ABS_LIMIT
        && value < KBO_FA_COMP_FINANCIAL_FIELD_ABS_LIMIT;
}

static int kbo_fa_comp_financial_block_plausible(uint8_t* financials)
{
    if (financials == NULL
            || !memory_range_readable(financials, KBO_FA_COMP_TEAM_FINANCIALS_READABLE_BYTES)) {
        return 0;
    }

    int32_t budget = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_BUDGET_OFFSET);
    int32_t scouting_budget = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_SCOUTING_BUDGET_OFFSET);
    int32_t development_budget = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_DEVELOPMENT_BUDGET_OFFSET);
    int32_t draft_budget = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_DRAFT_BUDGET_OFFSET);
    int32_t draft_expenses = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_DRAFT_EXPENSES_OFFSET);
    int32_t cash = *(int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_CASH_OFFSET);

    return kbo_fa_comp_abs_i32_plausible(budget)
        && kbo_fa_comp_abs_i32_plausible(scouting_budget)
        && kbo_fa_comp_abs_i32_plausible(development_budget)
        && kbo_fa_comp_abs_i32_plausible(draft_budget)
        && kbo_fa_comp_abs_i32_plausible(draft_expenses)
        && kbo_fa_comp_abs_i32_plausible(cash);
}

static int32_t* kbo_fa_comp_team_cash_ptr(uint32_t team_id)
{
    if (team_id == 0u) {
        return NULL;
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL
            || !memory_range_readable(
                team + KBO_FA_COMP_TEAM_FINANCIALS_BLOCK_OFFSET,
                KBO_FA_COMP_TEAM_FINANCIALS_READABLE_BYTES)) {
        return NULL;
    }
    uint8_t* financials = team + KBO_FA_COMP_TEAM_FINANCIALS_BLOCK_OFFSET;
    if (!kbo_fa_comp_financial_block_plausible(financials)) {
        return NULL;
    }
    return (int32_t*)(financials + KBO_FA_COMP_TEAM_FINANCIALS_CASH_OFFSET);
}

static int kbo_fa_comp_cash_transfer_already_applied(
    uint32_t season,
    uint32_t player_id,
    uint32_t signing_team_id,
    uint32_t original_team_id,
    const char* action)
{
    if (season == 0u || player_id == 0u || signing_team_id == 0u || original_team_id == 0u) {
        return 0;
    }

    return kbo_fa_compensation_cash_transfer_sql_already_applied(
        season,
        player_id,
        signing_team_id,
        original_team_id,
        action);
}

static int kbo_fa_comp_append_cash_transfer_ledger(
    const KboFaCompensationRecord* rec,
    uint32_t amount,
    uint32_t applied_yyyymmdd,
    const char* action,
    const char* source,
    int32_t signing_old_cash,
    int32_t signing_new_cash,
    int32_t original_old_cash,
    int32_t original_new_cash)
{
    return kbo_fa_compensation_cash_transfer_sql_append(
        rec,
        amount,
        applied_yyyymmdd,
        action,
        source,
        signing_old_cash,
        signing_new_cash,
        original_old_cash,
        original_new_cash);
}

static int32_t kbo_fa_comp_add_cash_clamped(int32_t old_cash, int64_t delta)
{
    int64_t next = (int64_t)old_cash + delta;
    if (next < (int64_t)INT32_MIN) {
        return INT32_MIN;
    }
    if (next > (int64_t)INT32_MAX) {
        return INT32_MAX;
    }
    return (int32_t)next;
}

int kbo_apply_fa_compensation_cash_transfer(
    const KboFaCompensationRecord* rec,
    uint32_t amount,
    uint32_t applied_yyyymmdd,
    const char* action,
    const char* source)
{
    if (rec == NULL
            || rec->player_id == 0u
            || rec->season == 0u
            || rec->signing_team_id == 0u
            || rec->original_team_id == 0u
            || rec->signing_team_id == rec->original_team_id
            || amount == 0u
            || applied_yyyymmdd == 0u) {
        return 0;
    }
    if (kbo_fa_comp_cash_transfer_already_applied(
            rec->season,
            rec->player_id,
            rec->signing_team_id,
            rec->original_team_id,
            action)) {
        kbo_log_runtimef(
            "KBO FA compensation cash transfer skipped source=%s reason=already_applied player=%u season=%u action=%s",
            source != NULL ? source : "",
            rec->player_id,
            rec->season,
            action != NULL ? action : "");
        return 0;
    }

    int32_t* signing_cash = kbo_fa_comp_team_cash_ptr(rec->signing_team_id);
    int32_t* original_cash = kbo_fa_comp_team_cash_ptr(rec->original_team_id);
    if (signing_cash == NULL || original_cash == NULL) {
        kbo_log_runtimef(
            "KBO FA compensation cash transfer skipped source=%s reason=team_cash_unavailable player=%u signing_team=%u original_team=%u amount=%u",
            source != NULL ? source : "",
            rec->player_id,
            rec->signing_team_id,
            rec->original_team_id,
            amount);
        return 0;
    }

    int32_t signing_old_cash = *signing_cash;
    int32_t original_old_cash = *original_cash;
    int32_t signing_new_cash = kbo_fa_comp_add_cash_clamped(signing_old_cash, -(int64_t)amount);
    int32_t original_new_cash = kbo_fa_comp_add_cash_clamped(original_old_cash, (int64_t)amount);
    *signing_cash = signing_new_cash;
    *original_cash = original_new_cash;

    if (!kbo_fa_comp_append_cash_transfer_ledger(
            rec,
            amount,
            applied_yyyymmdd,
            action,
            source,
            signing_old_cash,
            signing_new_cash,
            original_old_cash,
            original_new_cash)) {
        kbo_log_runtimef(
            "KBO FA compensation cash transfer ledger failed source=%s player=%u amount=%u action=%s",
            source != NULL ? source : "",
            rec->player_id,
            amount,
            action != NULL ? action : "");
    }

    kbo_log_runtimef(
        "KBO FA compensation cash transfer applied source=%s player=%u action=%s amount=%u signing_team=%u cash=%d->%d original_team=%u cash=%d->%d date=%u",
        source != NULL ? source : "",
        rec->player_id,
        action != NULL ? action : "",
        amount,
        rec->signing_team_id,
        signing_old_cash,
        signing_new_cash,
        rec->original_team_id,
        original_old_cash,
        original_new_cash,
        applied_yyyymmdd);
    return 1;
}
