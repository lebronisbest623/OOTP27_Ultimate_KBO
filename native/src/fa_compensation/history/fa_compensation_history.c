#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/logging/core_log.h"
#include "../../core/logging/rule_audit.h"
#include "../../core/sql/history_transactions/core_sql_history_transactions.h"
#include "../../fa_filing/fa_filing.h"
#include "../../fa_market_classification/api/fa_market_classification.h"
#include "../../fa_rules/fa_rules.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../team/lookup/team_lookup.h"
#include "fa_compensation_history.h"
#include "../finance/fa_compensation_cash_transfer.h"
#include "../market/fa_compensation_market.h"
#include "../news/fa_compensation_news_transfer.h"
#include "../records/fa_compensation_records.h"
#include "../state/fa_compensation_state.h"

static void kbo_fa_compensation_record_history(const KboFaCompensationRecord* rec)
{
    if (rec == NULL || rec->player_id == 0u || rec->signed_on_yyyymmdd == 0u) {
        return;
    }

    uint32_t year = rec->signed_on_yyyymmdd / 10000u;
    uint32_t month = (rec->signed_on_yyyymmdd / 100u) % 100u;
    uint32_t day = rec->signed_on_yyyymmdd % 100u;
    if (year < 1982u || month == 0u || day == 0u) {
        return;
    }

    char salary_text[32] = "-";
    char cash_with_player_text[32] = "-";
    char cash_only_text[32] = "-";
    kbo_fa_market_format_salary(rec->previous_salary, salary_text, sizeof(salary_text));
    kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_with_player_text, sizeof(cash_with_player_text));
    kbo_fa_market_format_salary((int32_t)rec->cash_only, cash_only_text, sizeof(cash_only_text));

    char history_text[512] = {0};
    if (rec->requires_player_compensation) {
        snprintf(
            history_text,
            sizeof(history_text),
            "[G]KBO FA compensation recorded: Grade %s, previous salary %s, %u protected players; compensation is %s plus one player or %s cash.",
            rec->grade,
            salary_text,
            rec->protect_count,
            cash_with_player_text,
            cash_only_text);
    } else {
        snprintf(
            history_text,
            sizeof(history_text),
            "[G]KBO FA compensation recorded: Grade %s, previous salary %s; compensation is %s cash.",
            rec->grade,
            salary_text,
            cash_only_text);
    }
    insert_kbo_player_history_sql(rec->player_id, year, month, day, history_text, "fa_compensation");
}

int kbo_record_fa_compensation_signing(
    uintptr_t player_ptr,
    uint32_t signing_team_id,
    uint32_t league_id,
    const char* source)
{
    KBO_PROFILE_BEGIN(profile_fa_comp_record_signing);
    if (!kbo_fix_enabled()) {
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.disabled");
        return 0;
    }
    if (read_kbo_localappdata_flag_file("disable_kbo_fa_compensation.txt")) {
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.flag_disabled");
        return 0;
    }
    if (!kbo_player_pointer_plausible(player_ptr) || signing_team_id == 0u || league_id == 0u) {
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.bad_input");
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_fa_comp_rules_load);
    KboFaRules fa_rules;
    kbo_fa_rules_load(&fa_rules);
    KBO_PROFILE_END(profile_fa_comp_rules_load, "fa_comp.record_signing.rules_load");

    uint8_t* player = (uint8_t*)player_ptr;
    if (fa_rules.exclude_foreign_players && kbo_player_is_foreign_for_kbo_rights(player)) {
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.foreign_excluded");
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today)) {
        today = 0u;
    }

    uint32_t signing_year = 0u;
    uint32_t filing_original_team_id = 0u;
    uint32_t filing_league_id = 0u;
    uint32_t filing_season = 0u;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (kbo_fa_filing_find_latest_player(player_id, &filing_original_team_id, &filing_league_id, &filing_season)
            && filing_season >= 1982u
            && filing_season <= 2300u) {
        signing_year = filing_season;
        if (filing_league_id != 0u) {
            league_id = filing_league_id;
        }
    }
    if (signing_year == 0u) {
        signing_year = kbo_find_league_year_from_id_no_scan(league_id);
    }
    if (signing_year < 1982u || signing_year > 2300u) {
        signing_year = today >= 19820000u ? today / 10000u : 0u;
    }
    if (signing_year < 1982u || signing_year > 2300u) {
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.no_year");
        return 0;
    }
    if (today == 0u) {
        today = signing_year * 10000u + 101u;
    }

    KboFaMarketClassification row;
    KBO_PROFILE_BEGIN(profile_fa_comp_market_row);
    if (!kbo_fa_compensation_build_market_row(player, &row, league_id, signing_year, today, &fa_rules)) {
        KBO_PROFILE_END(profile_fa_comp_market_row, "fa_comp.record_signing.market_row_failed");
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.market_row_failed");
        return 0;
    }
    KBO_PROFILE_END(profile_fa_comp_market_row, "fa_comp.record_signing.market_row");

    if (filing_original_team_id != 0u && filing_original_team_id != row.original_team_id) {
        uint8_t* filing_original_team = find_kbo_team_by_numeric_id_any_league(filing_original_team_id, 0);
        if (filing_original_team != NULL) {
            kbo_log_runtimef(
                "KBO FA compensation original team override player=%u memory_original=%u filing_original=%u signing_team=%u filing_league=%u filing_season=%u",
                row.player_id,
                row.original_team_id,
                filing_original_team_id,
                signing_team_id,
                filing_league_id,
                filing_season);
            row.original_team_id = filing_original_team_id;
        }
    }

    if (!kbo_fa_rules_case_is_compensable(&fa_rules, row.case_label)
            || (fa_rules.exclude_foreign_players && row.foreign_player)
            || row.original_team_id == 0u
            || row.original_team_id == signing_team_id
            || !kbo_fa_rules_grade_is_compensable(&fa_rules, row.grade)
            || row.fa_grade_salary <= 0) {
        static LONG skip_log_count = 0;
        LONG slot = InterlockedIncrement(&skip_log_count);
        if (slot <= 80) {
            kbo_log_runtimef(
                "KBO FA compensation skipped player=%u signing_team=%u original_team=%u case=%s grade=%s salary=%d foreign=%u reason=not_compensable",
                row.player_id,
                signing_team_id,
                row.original_team_id,
                row.case_label,
                row.grade,
                row.fa_grade_salary,
                (uint32_t)row.foreign_player);
        }
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.not_compensable");
        return 0;
    }

    uint32_t cash_with_player = 0u;
    uint32_t cash_only = 0u;
    uint32_t protect_count = 0u;
    uint8_t requires_player = 0u;
    kbo_fa_rules_calculate_compensation(
        &fa_rules,
        row.grade,
        row.fa_grade_salary,
        &cash_with_player,
        &cash_only,
        &protect_count,
        &requires_player);
    if (cash_only == 0u) {
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.no_cash_only");
        return 0;
    }

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    if (records == NULL) {
        kbo_log_runtime_line("KBO FA compensation skipped reason=allocation_failed");
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.alloc_failed");
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_fa_comp_ledger);
    kbo_fa_compensation_lock_ledger(source != NULL ? source : "record_signing");
    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    if (kbo_fa_compensation_find_existing(
            records,
            record_count,
            row.player_id,
            signing_year,
            row.original_team_id,
            signing_team_id) >= 0) {
        kbo_fa_compensation_unlock_ledger();
        HeapFree(GetProcessHeap(), 0, records);
        KBO_PROFILE_END(profile_fa_comp_ledger, "fa_comp.record_signing.ledger_existing");
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.existing");
        return 0;
    }

    if (record_count >= KBO_FA_COMPENSATION_MAX) {
        kbo_log_runtimef("KBO FA compensation skipped reason=ledger_full player=%u path=%s", row.player_id, path);
        kbo_fa_compensation_unlock_ledger();
        HeapFree(GetProcessHeap(), 0, records);
        KBO_PROFILE_END(profile_fa_comp_ledger, "fa_comp.record_signing.ledger_full");
        KBO_PROFILE_END(profile_fa_comp_record_signing, "fa_comp.record_signing.ledger_full");
        return 0;
    }

    KboFaCompensationRecord* rec = &records[record_count++];
    memset(rec, 0, sizeof(*rec));
    rec->player_id = row.player_id;
    rec->signed_on_yyyymmdd = today;
    rec->season = signing_year;
    rec->league_id = league_id;
    rec->original_team_id = row.original_team_id;
    rec->signing_team_id = signing_team_id;
    snprintf(rec->grade, sizeof(rec->grade), "%s", row.grade);
    rec->previous_salary = row.fa_grade_salary;
    rec->cash_with_player = cash_with_player;
    rec->cash_only = cash_only;
    rec->protect_count = protect_count;
    rec->requires_player_compensation = requires_player;
    rec->status = KBO_FA_COMPENSATION_STATUS_PENDING;
    snprintf(rec->case_label, sizeof(rec->case_label), "%s", row.case_label);
    snprintf(rec->player_name, sizeof(rec->player_name), "%s", row.player_name);
    snprintf(rec->source, sizeof(rec->source), "%s", source != NULL ? source : "fa_signing");

    KBO_PROFILE_BEGIN(profile_fa_comp_append);
    int persisted = kbo_append_fa_compensation_record(rec);
    KBO_PROFILE_END(profile_fa_comp_append, persisted ? "fa_comp.record_signing.appended" : "fa_comp.record_signing.append_failed");
    KboFaCompensationRecord recorded = *rec;

    kbo_fa_compensation_unlock_ledger();
    KBO_PROFILE_END(profile_fa_comp_ledger, persisted ? "fa_comp.record_signing.ledger_persisted" : "fa_comp.record_signing.ledger_not_persisted");
    HeapFree(GetProcessHeap(), 0, records);
    if (persisted) {
        KBO_PROFILE_BEGIN(profile_fa_comp_history);
        kbo_fa_compensation_record_history(&recorded);
        KBO_PROFILE_END(profile_fa_comp_history, "fa_comp.record_signing.history");
        if (!recorded.requires_player_compensation) {
            kbo_apply_fa_compensation_cash_transfer(
                &recorded,
                recorded.cash_only,
                recorded.signed_on_yyyymmdd,
                "CASH_ONLY_SIGNING",
                source != NULL ? source : "fa_signing");
            kbo_emit_fa_compensation_cash_only_news(&recorded, recorded.signed_on_yyyymmdd);
        }
        kbo_log_runtimef(
            "KBO FA compensation recorded player=%u name=%s grade=%s signing_team=%u original_team=%u salary=%d cash_with_player=%u cash_only=%u protect=%u ledger=%s",
            recorded.player_id,
            recorded.player_name,
            recorded.grade,
            recorded.signing_team_id,
            recorded.original_team_id,
            recorded.previous_salary,
            recorded.cash_with_player,
            recorded.cash_only,
            recorded.protect_count,
            path);
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", recorded.signed_on_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "season", recorded.season);
            kbo_log_field_u32(&audit_fields, "league_id", recorded.league_id);
            kbo_log_field_u32(&audit_fields, "player_id", recorded.player_id);
            kbo_log_field_u32(&audit_fields, "signing_team_id", recorded.signing_team_id);
            kbo_log_field_u32(&audit_fields, "original_team_id", recorded.original_team_id);
            kbo_log_field_i32(&audit_fields, "previous_salary", recorded.previous_salary);
            kbo_log_field_u32(&audit_fields, "cash_with_player", recorded.cash_with_player);
            kbo_log_field_u32(&audit_fields, "cash_only", recorded.cash_only);
            kbo_log_field_u32(&audit_fields, "protect_count", recorded.protect_count);
            kbo_log_field_u32(&audit_fields, "requires_player", (uint32_t)recorded.requires_player_compensation);
            kbo_rule_audit_emit_fields(
                "fa.compensation.signing",
                "record_compensation",
                "compensable_fa_signed",
                source != NULL ? source : "fa_signing",
                &audit_fields);
        } while (0);
    } else {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", recorded.signed_on_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "season", recorded.season);
            kbo_log_field_u32(&audit_fields, "league_id", recorded.league_id);
            kbo_log_field_u32(&audit_fields, "player_id", recorded.player_id);
            kbo_log_field_u32(&audit_fields, "signing_team_id", recorded.signing_team_id);
            kbo_log_field_u32(&audit_fields, "original_team_id", recorded.original_team_id);
            kbo_log_field_i32(&audit_fields, "previous_salary", recorded.previous_salary);
            kbo_log_field_u32(&audit_fields, "cash_only", recorded.cash_only);
            kbo_rule_audit_emit_fields(
                "fa.compensation.signing",
                "fail",
                "ledger_append_failed",
                source != NULL ? source : "fa_signing",
                &audit_fields);
        } while (0);
    }
    KBO_PROFILE_END(profile_fa_comp_record_signing, persisted ? "fa_comp.record_signing.persisted" : "fa_comp.record_signing.not_persisted");
    return persisted;
}

const char* kbo_fa_compensation_status_label(uint8_t status)
{
    switch (status) {
    case KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED:
        return "Cash only recorded";
    case KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED:
        return "Cash only selected";
    case KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED:
        return "Player transferred";
    case KBO_FA_COMPENSATION_STATUS_PLAYER_SELECTED:
        return "Player selected";
    case KBO_FA_COMPENSATION_STATUS_RECORDED:
        return "List submitted";
    case KBO_FA_COMPENSATION_STATUS_PENDING:
    default:
        return "Pending";
    }
}
