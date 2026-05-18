#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "domestic_fa_market_investigation_scan.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/files/atomic/core_atomic_file.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../fa_market_classification/policy/fa_market_policy.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"

uint32_t kbo_domestic_fa_date_gap_days(uint32_t older_yyyymmdd, uint32_t newer_yyyymmdd)
{
    if (older_yyyymmdd == 0u || newer_yyyymmdd == 0u || older_yyyymmdd > newer_yyyymmdd) {
        return 0u;
    }

    uint32_t older_serial = kbo_date_serial(
        older_yyyymmdd / 10000u,
        (older_yyyymmdd / 100u) % 100u,
        older_yyyymmdd % 100u);
    uint32_t newer_serial = kbo_date_serial(
        newer_yyyymmdd / 10000u,
        (newer_yyyymmdd / 100u) % 100u,
        newer_yyyymmdd % 100u);
    if (older_serial == 0u || newer_serial == 0u || older_serial > newer_serial) {
        return 0u;
    }
    return newer_serial - older_serial;
}

uint32_t kbo_domestic_fa_history_date_from_reason(const char* reason)
{
    if (reason == NULL) {
        return 0u;
    }

    const char* marker = strstr(reason, "history=");
    if (marker == NULL) {
        return 0u;
    }
    marker += 8;

    char date_text[16] = {0};
    size_t pos = 0u;
    while (marker[pos] != '\0'
            && marker[pos] != ' '
            && marker[pos] != ';'
            && pos + 1u < sizeof(date_text)) {
        date_text[pos] = marker[pos];
        pos++;
    }
    date_text[pos] = '\0';

    uint32_t date = 0u;
    if (kbo_parse_yyyymmdd(date_text, &date)) {
        return date;
    }
    return 0u;
}

int kbo_domestic_fa_case_is_market_relevant(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    return strcmp(case_label, "KBO_FA_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_DEFERRED") == 0
        || strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
        || strcmp(case_label, "KBO_FA_CARRYOVER_UNSIGNED") == 0;
}

int kbo_domestic_fa_case_is_official_or_probable(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    return strcmp(case_label, "KBO_FA_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
        || strcmp(case_label, "KBO_FA_CARRYOVER_UNSIGNED") == 0;
}

static int kbo_domestic_fa_grade_is_quality(const char* grade)
{
    return grade != NULL
        && (strcmp(grade, "A") == 0
            || strcmp(grade, "B") == 0
            || strcmp(grade, "C") == 0);
}

int kbo_domestic_fa_row_is_quality_candidate(
    const KboFaMarketClassification* row,
    int32_t value_score)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u
            || !kbo_domestic_fa_case_is_market_relevant(row->case_label)) {
        return 0;
    }

    if (kbo_domestic_fa_grade_is_quality(row->grade)) {
        return 1;
    }
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    if (row->fa_grade_salary >= policy->investigation_quality_salary_min) {
        return 1;
    }
    if (row->fa_demand >= policy->investigation_quality_demand_min) {
        return 1;
    }
    return value_score >= policy->investigation_quality_value_score_min;
}

static void kbo_domestic_fa_append_blocker(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0 || text == NULL || text[0] == '\0') {
        return;
    }
    size_t len = strlen(out);
    if (len > 0 && len + 1 < out_size) {
        out[len++] = '|';
        out[len] = '\0';
    }
    if (len + 1 < out_size) {
        snprintf(out + len, out_size - len, "%s", text);
    }
}

void kbo_domestic_fa_describe_blockers(
    const KboFaMarketClassification* row,
    int32_t value_score,
    uint32_t market_days,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (row == NULL) {
        return;
    }

    if (!kbo_domestic_fa_case_is_official_or_probable(row->case_label)) {
        kbo_domestic_fa_append_blocker(out, out_size, "not_official_or_probable_fa");
    }
    if (row->fa_grade_team_changed_review) {
        kbo_domestic_fa_append_blocker(out, out_size, "team_changed_grade_review");
    }
    if (strcmp(row->fa_grade_flag, "SNAPSHOT_MISSING") == 0) {
        kbo_domestic_fa_append_blocker(out, out_size, "salary_snapshot_missing");
    }
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    if (row->fa_demand >= policy->investigation_very_high_demand_min) {
        kbo_domestic_fa_append_blocker(out, out_size, "very_high_demand");
    } else if (row->fa_demand >= policy->investigation_high_demand_min) {
        kbo_domestic_fa_append_blocker(out, out_size, "high_demand");
    }
    if (strcmp(row->grade, "A") == 0 || strcmp(row->grade, "B") == 0) {
        kbo_domestic_fa_append_blocker(out, out_size, "compensation_burden");
    }
    if (row->age >= (uint16_t)policy->investigation_age_old_min) {
        kbo_domestic_fa_append_blocker(out, out_size, "age_36_plus");
    } else if (row->age >= (uint16_t)policy->investigation_age_veteran_min) {
        kbo_domestic_fa_append_blocker(out, out_size, "age_34_plus");
    }
    if (market_days >= (uint32_t)policy->investigation_market_days_very_long_min) {
        kbo_domestic_fa_append_blocker(out, out_size, "market_90d_plus");
    } else if (market_days >= (uint32_t)policy->investigation_market_days_long_min) {
        kbo_domestic_fa_append_blocker(out, out_size, "market_45d_plus");
    }
    if (value_score >= policy->investigation_unexplained_value_score_min && out[0] == '\0') {
        kbo_domestic_fa_append_blocker(out, out_size, "high_quality_unexplained");
    }
    if (out[0] == '\0') {
        snprintf(out, out_size, "watch");
    }
}

int kbo_domestic_fa_compare_candidates_desc(const void* lhs, const void* rhs)
{
    const KboDomesticFaInvestigationCandidate* a = (const KboDomesticFaInvestigationCandidate*)lhs;
    const KboDomesticFaInvestigationCandidate* b = (const KboDomesticFaInvestigationCandidate*)rhs;
    uint32_t grade_a = kbo_fa_market_display_grade_sort_rank(a->row.grade);
    uint32_t grade_b = kbo_fa_market_display_grade_sort_rank(b->row.grade);
    if (grade_a != grade_b) {
        return grade_a < grade_b ? -1 : 1;
    }
    if (a->value_score != b->value_score) {
        return a->value_score > b->value_score ? -1 : 1;
    }
    if (a->market_days != b->market_days) {
        return a->market_days > b->market_days ? -1 : 1;
    }
    if (a->row.age != b->row.age) {
        return a->row.age < b->row.age ? -1 : 1;
    }
    if (a->row.player_id == b->row.player_id) {
        return 0;
    }
    return a->row.player_id < b->row.player_id ? -1 : 1;
}

static void kbo_domestic_fa_csv_text(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, "\"", 1, &written, NULL);
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            if (*p == '"') {
                WriteFile(file, "\"\"", 2, &written, NULL);
            } else {
                WriteFile(file, p, 1, &written, NULL);
            }
        }
    }
    WriteFile(file, "\"", 1, &written, NULL);
}

static void kbo_domestic_fa_csv_raw(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE || text == NULL) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
}

int kbo_domestic_fa_write_investigation_csv(
    const KboDomesticFaInvestigationCandidate* candidates,
    int candidate_count,
    const KboFaMarketScanSummary* summary,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (candidates == NULL || candidate_count < 0 || summary == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_save_scoped_data_file("domestic_fa_market_investigation.csv", path, sizeof(path))) {
        kbo_log_runtime_line("domestic FA market investigation: unable to resolve CSV path");
        return 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("domestic FA market investigation: failed to open CSV path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    kbo_domestic_fa_csv_raw(
        file,
        "date,league_id,rank,player_id,name,age,case,grade,value_score,overall,talent,ratings,career,fa_demand,fa_grade_salary,fa_grade_overall_rank,fa_grade_team_rank,original_team_id,rights_team_id,market_days,fa_grade_flag,blockers,reason\r\n");

    char line[512] = {0};
    for (int i = 0; i < candidate_count; i++) {
        const KboDomesticFaInvestigationCandidate* candidate = &candidates[i];
        const KboFaMarketClassification* row = &candidate->row;
        int len = snprintf(
            line,
            sizeof(line),
            "%08u,%u,%d,%u,",
            summary->today_yyyymmdd,
            summary->league_id,
            i + 1,
            row->player_id);
        if (len <= 0 || len >= (int)sizeof(line)) {
            continue;
        }
        kbo_domestic_fa_csv_raw(file, line);
        kbo_domestic_fa_csv_text(file, row->player_name);
        len = snprintf(
            line,
            sizeof(line),
            ",%u,",
            (uint32_t)row->age);
        if (len <= 0 || len >= (int)sizeof(line)) {
            continue;
        }
        kbo_domestic_fa_csv_raw(file, line);
        kbo_domestic_fa_csv_text(file, row->case_label);
        kbo_domestic_fa_csv_raw(file, ",");
        kbo_domestic_fa_csv_text(file, row->grade);
        len = snprintf(
            line,
            sizeof(line),
            ",%d,%d,%d,%d,%d,%d,%d,%u,%u,%u,%u,%u,",
            candidate->value_score,
            candidate->overall,
            candidate->talent,
            candidate->ratings,
            candidate->career,
            row->fa_demand,
            row->fa_grade_salary,
            row->fa_grade_overall_rank,
            row->fa_grade_team_rank,
            row->original_team_id,
            row->rights_team_id,
            candidate->market_days);
        if (len <= 0 || len >= (int)sizeof(line)) {
            continue;
        }
        kbo_domestic_fa_csv_raw(file, line);
        kbo_domestic_fa_csv_text(file, row->fa_grade_flag);
        kbo_domestic_fa_csv_raw(file, ",");
        kbo_domestic_fa_csv_text(file, candidate->blockers);
        kbo_domestic_fa_csv_raw(file, ",");
        kbo_domestic_fa_csv_text(file, row->reason);
        kbo_domestic_fa_csv_raw(file, "\r\n");
    }

    if (!kbo_atomic_commit(file, tmp_path, path)) {
        kbo_log_runtimef("domestic FA market investigation: atomic commit failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return 1;
}
