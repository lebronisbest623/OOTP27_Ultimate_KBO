#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cbt_cash_charge.h"
#include "sql/cbt_cash_charge_sql_store.h"
#include "../records/cbt_records.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

/*
 * The team object embeds its current financials block. These field offsets were
 * verified against OOTP 27's financial export sites: cash is financials + 0xc0.
 */
#define KBO_CBT_TEAM_FINANCIALS_BLOCK_OFFSET 0x2510u
#define KBO_CBT_TEAM_FINANCIALS_BUDGET_OFFSET 0x78u
#define KBO_CBT_TEAM_FINANCIALS_SCOUTING_BUDGET_OFFSET 0x90u
#define KBO_CBT_TEAM_FINANCIALS_DEVELOPMENT_BUDGET_OFFSET 0x94u
#define KBO_CBT_TEAM_FINANCIALS_DRAFT_BUDGET_OFFSET 0x98u
#define KBO_CBT_TEAM_FINANCIALS_DRAFT_EXPENSES_OFFSET 0x9cu
#define KBO_CBT_TEAM_FINANCIALS_CASH_OFFSET 0xc0u
#define KBO_CBT_TEAM_FINANCIALS_READABLE_BYTES (KBO_CBT_TEAM_FINANCIALS_CASH_OFFSET + sizeof(int32_t))
#define KBO_CBT_FINANCIAL_FIELD_ABS_LIMIT 2000000000

static volatile LONG g_kbo_cbt_cash_charge_busy = 0;

static int kbo_cbt_abs_i32_plausible(int32_t value)
{
    return value > -KBO_CBT_FINANCIAL_FIELD_ABS_LIMIT
        && value < KBO_CBT_FINANCIAL_FIELD_ABS_LIMIT;
}

static int kbo_cbt_financial_block_plausible(uint8_t* financials)
{
    if (financials == NULL
            || !memory_range_readable(financials, KBO_CBT_TEAM_FINANCIALS_READABLE_BYTES)) {
        return 0;
    }

    int32_t budget = *(int32_t*)(financials + KBO_CBT_TEAM_FINANCIALS_BUDGET_OFFSET);
    int32_t scouting_budget = *(int32_t*)(financials + KBO_CBT_TEAM_FINANCIALS_SCOUTING_BUDGET_OFFSET);
    int32_t development_budget = *(int32_t*)(financials + KBO_CBT_TEAM_FINANCIALS_DEVELOPMENT_BUDGET_OFFSET);
    int32_t draft_budget = *(int32_t*)(financials + KBO_CBT_TEAM_FINANCIALS_DRAFT_BUDGET_OFFSET);
    int32_t draft_expenses = *(int32_t*)(financials + KBO_CBT_TEAM_FINANCIALS_DRAFT_EXPENSES_OFFSET);
    int32_t cash = *(int32_t*)(financials + KBO_CBT_TEAM_FINANCIALS_CASH_OFFSET);

    return kbo_cbt_abs_i32_plausible(budget)
        && kbo_cbt_abs_i32_plausible(scouting_budget)
        && kbo_cbt_abs_i32_plausible(development_budget)
        && kbo_cbt_abs_i32_plausible(draft_budget)
        && kbo_cbt_abs_i32_plausible(draft_expenses)
        && kbo_cbt_abs_i32_plausible(cash);
}

static int kbo_cbt_record_season_has_charge(
    const KboCbtRecord* records,
    int count,
    uint32_t season)
{
    if (records == NULL || count <= 0 || season == 0u) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (records[i].season == season && records[i].tax_amount > 0) {
            return 1;
        }
    }
    return 0;
}

static uint32_t kbo_cbt_resolve_cash_charge_season(
    const KboCbtRecord* records,
    int count,
    uint32_t season,
    uint32_t applied_yyyymmdd)
{
    uint32_t date_year = applied_yyyymmdd / 10000u;
    uint32_t candidates[3] = {
        season,
        date_year,
        season > 1900u ? season - 1u : 0u
    };

    for (int i = 0; i < 3; i++) {
        uint32_t candidate = candidates[i];
        if (candidate != 0u && kbo_cbt_record_season_has_charge(records, count, candidate)) {
            return candidate;
        }
    }
    return 0u;
}

static int kbo_cbt_cash_charge_already_applied(uint32_t season, uint32_t team_id)
{
    if (season == 0u || team_id == 0u) {
        return 0;
    }
    return kbo_cbt_cash_charge_sql_already_applied(season, team_id);
}

static int kbo_cbt_cash_charge_append_ledger(
    const KboCbtRecord* rec,
    int32_t old_cash,
    int32_t new_cash,
    uint32_t applied_yyyymmdd,
    const char* source)
{
    if (rec == NULL || rec->season == 0u || rec->team_id == 0u) {
        return 0;
    }
    return kbo_cbt_cash_charge_sql_append_ledger(rec, old_cash, new_cash, applied_yyyymmdd, source);
}

static int32_t kbo_cbt_cash_after_charge(int32_t old_cash, int32_t tax_amount)
{
    int64_t next = (int64_t)old_cash - (int64_t)tax_amount;
    if (next < (int64_t)INT32_MIN) {
        return INT32_MIN;
    }
    if (next > (int64_t)INT32_MAX) {
        return INT32_MAX;
    }
    return (int32_t)next;
}

static int kbo_cbt_apply_cash_charge_to_team(
    const KboCbtRecord* rec,
    uint32_t applied_yyyymmdd,
    const char* source)
{
    if (rec == NULL || rec->team_id == 0u || rec->tax_amount <= 0) {
        return 0;
    }
    if (kbo_cbt_cash_charge_already_applied(rec->season, rec->team_id)) {
        kbo_log_runtimef(
            "KBO CBT cash charge skipped source=%s reason=already_applied season=%u team=%u",
            source != NULL ? source : "",
            rec->season,
            rec->team_id);
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(rec->team_id, 0);
    if (team == NULL
            || !memory_range_readable(
                team + KBO_CBT_TEAM_FINANCIALS_BLOCK_OFFSET,
                KBO_CBT_TEAM_FINANCIALS_READABLE_BYTES)) {
        kbo_log_runtimef(
            "KBO CBT cash charge skipped source=%s reason=team_financials_unavailable season=%u team=%u tax=%d",
            source != NULL ? source : "",
            rec->season,
            rec->team_id,
            rec->tax_amount);
        return 0;
    }

    uint8_t* financials = team + KBO_CBT_TEAM_FINANCIALS_BLOCK_OFFSET;
    if (!kbo_cbt_financial_block_plausible(financials)) {
        kbo_log_runtimef(
            "KBO CBT cash charge skipped source=%s reason=financials_guard_failed season=%u team=%u tax=%d team_ptr=%p",
            source != NULL ? source : "",
            rec->season,
            rec->team_id,
            rec->tax_amount,
            team);
        return 0;
    }

    int32_t* cash_ptr = (int32_t*)(financials + KBO_CBT_TEAM_FINANCIALS_CASH_OFFSET);
    int32_t old_cash = *cash_ptr;
    int32_t new_cash = kbo_cbt_cash_after_charge(old_cash, rec->tax_amount);
    *cash_ptr = new_cash;

    if (!kbo_cbt_cash_charge_append_ledger(rec, old_cash, new_cash, applied_yyyymmdd, source)) {
        kbo_log_runtimef(
            "KBO CBT cash charge ledger failed source=%s season=%u team=%u tax=%d old_cash=%d new_cash=%d",
            source != NULL ? source : "",
            rec->season,
            rec->team_id,
            rec->tax_amount,
            old_cash,
            new_cash);
    }

    kbo_log_runtimef(
        "KBO CBT cash charge applied source=%s season=%u team=%u tax=%d old_cash=%d new_cash=%d date=%u",
        source != NULL ? source : "",
        rec->season,
        rec->team_id,
        rec->tax_amount,
        old_cash,
        new_cash,
        applied_yyyymmdd);
    return 1;
}

int kbo_cbt_apply_offseason_cash_charges(uint32_t season, uint32_t applied_yyyymmdd, const char* source)
{
    if (season == 0u || applied_yyyymmdd == 0u || !kbo_fix_enabled()) {
        return 0;
    }
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_COMPETITIVE_BALANCE_TAX_FILE)) {
        kbo_log_runtimef(
            "KBO CBT cash charge skipped source=%s season=%u date=%u reason=cbt_disabled",
            source != NULL ? source : "",
            season,
            applied_yyyymmdd);
        return 0;
    }
    if (InterlockedCompareExchange(&g_kbo_cbt_cash_charge_busy, 1, 0) != 0) {
        return 0;
    }

    int applied_count = 0;
    KboCbtRecord records[KBO_CBT_RECORDS_MAX];
    int record_count = kbo_cbt_load_records(records, KBO_CBT_RECORDS_MAX, NULL, 0);
    if (record_count <= 0) {
        kbo_log_runtimef(
            "KBO CBT cash charge skipped source=%s season=%u date=%u reason=no_records",
            source != NULL ? source : "",
            season,
            applied_yyyymmdd);
        InterlockedExchange(&g_kbo_cbt_cash_charge_busy, 0);
        return 0;
    }

    uint32_t charge_season = kbo_cbt_resolve_cash_charge_season(
        records,
        record_count,
        season,
        applied_yyyymmdd);
    if (charge_season == 0u) {
        kbo_log_runtimef(
            "KBO CBT cash charge skipped source=%s season=%u date=%u records=%d reason=no_charge_for_transition_season",
            source != NULL ? source : "",
            season,
            applied_yyyymmdd,
            record_count);
        InterlockedExchange(&g_kbo_cbt_cash_charge_busy, 0);
        return 0;
    }

    for (int i = 0; i < record_count; i++) {
        if (records[i].season != charge_season || records[i].tax_amount <= 0) {
            continue;
        }
        applied_count += kbo_cbt_apply_cash_charge_to_team(
            &records[i],
            applied_yyyymmdd,
            source != NULL ? source : "offseason_transition");
    }

    kbo_log_runtimef(
        "KBO CBT cash charge pass source=%s requested_season=%u charge_season=%u date=%u records=%d applied=%d",
        source != NULL ? source : "",
        season,
        charge_season,
        applied_yyyymmdd,
        record_count,
        applied_count);
    InterlockedExchange(&g_kbo_cbt_cash_charge_busy, 0);
    return applied_count;
}
