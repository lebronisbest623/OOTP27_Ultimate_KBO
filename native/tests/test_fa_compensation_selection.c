#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/fa_compensation/decisions/fa_compensation_decisions.h"
#include "../src/fa_compensation/protection/fa_compensation_protection_score.h"
#include "../src/fa_compensation/protection/policy/fa_compensation_protection_policy.h"
#include "../src/fa_compensation/records/fa_compensation_records.h"
#include "../src/fa_compensation/selection/fa_compensation_selection.h"
#include "../src/fa_rules/fa_rules.h"
#include "../src/foreign/common/dates/foreign_waiver_date.h"

static KboFaCompensationProtectionPolicy g_test_policy = {
    .cash_only_score_threshold = 65000,
    .cash_only_extra_cash_score_threshold = 80000,
    .cash_only_ab_grade_requires_no_player = 1
};

static KboFaCompensationRecord make_test_record(const char* grade)
{
    KboFaCompensationRecord rec;
    memset(&rec, 0, sizeof(rec));
    rec.player_id = 10u;
    snprintf(rec.grade, sizeof(rec.grade), "%s", grade);
    rec.previous_salary = 100000000;
    rec.cash_with_player = 100000000u;
    rec.cash_only = 200000000u;
    rec.requires_player_compensation = 1u;
    return rec;
}

static KboFaProtectedCandidate make_test_selected(int32_t score)
{
    KboFaProtectedCandidate selected;
    memset(&selected, 0, sizeof(selected));
    selected.player_id = 20u;
    selected.score = score;
    snprintf(selected.player_name, sizeof(selected.player_name), "Selected Player");
    return selected;
}

static void test_ab_grade_prefers_player_plus_cash_when_candidate_exists(void)
{
    KboFaProtectedCandidate low_score_selected = make_test_selected(1000);

    KboFaCompensationRecord a = make_test_record("A");
    assert(!kbo_fa_compensation_ai_prefers_cash_only(&a, &low_score_selected, 3));

    KboFaCompensationRecord b = make_test_record("b");
    assert(!kbo_fa_compensation_ai_prefers_cash_only(&b, &low_score_selected, 3));

    KboFaCompensationRecord c = make_test_record("C");
    assert(kbo_fa_compensation_ai_prefers_cash_only(&c, &low_score_selected, 3));

    assert(kbo_fa_compensation_ai_prefers_cash_only(&a, NULL, 0));

    g_test_policy.cash_only_ab_grade_requires_no_player = 0;
    assert(kbo_fa_compensation_ai_prefers_cash_only(&a, &low_score_selected, 3));
    g_test_policy.cash_only_ab_grade_requires_no_player = 1;

    printf("test_ab_grade_prefers_player_plus_cash_when_candidate_exists: PASS\n");
}

int main(void)
{
    test_ab_grade_prefers_player_plus_cash_when_candidate_exists();
    printf("All FA compensation selection tests passed.\n");
    return 0;
}

const KboFaCompensationProtectionPolicy* kbo_fa_compensation_protection_policy(void)
{
    return &g_test_policy;
}

int32_t kbo_fa_compensation_player_decision_score(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* candidate,
    char* reason,
    size_t reason_size)
{
    (void)rec;
    if (reason != NULL && reason_size > 0u) {
        snprintf(reason, reason_size, "test_score");
    }
    return candidate != NULL ? candidate->score : -2147483647;
}

void kbo_fa_compensation_lock_ledger(const char* source)
{
    (void)source;
}

void kbo_fa_compensation_unlock_ledger(void)
{
}

int kbo_load_fa_compensation_records(
    KboFaCompensationRecord* records,
    int max_records,
    char* out_path,
    size_t out_path_size)
{
    (void)records;
    (void)max_records;
    if (out_path != NULL && out_path_size > 0u) {
        out_path[0] = '\0';
    }
    return 0;
}

int kbo_persist_fa_compensation_records(const KboFaCompensationRecord* records, int record_count)
{
    (void)records;
    (void)record_count;
    return 0;
}

int kbo_load_fa_compensation_protection_debug_rows(
    KboFaCompensationProtectionDebugRow* rows,
    int max_rows)
{
    (void)rows;
    (void)max_rows;
    return 0;
}

int kbo_persist_fa_compensation_player_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const KboFaProtectedCandidate* selected,
    int unprotected_candidate_count,
    const char* source)
{
    (void)rec;
    (void)due_yyyymmdd;
    (void)decided_yyyymmdd;
    (void)selected;
    (void)unprotected_candidate_count;
    (void)source;
    return 0;
}

int kbo_persist_fa_compensation_cash_only_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const char* source)
{
    (void)rec;
    (void)due_yyyymmdd;
    (void)decided_yyyymmdd;
    (void)source;
    return 0;
}

int kbo_current_date_tick_latest_published_date(uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 20260301u;
    }
    return 1;
}

int kbo_fa_rules_load(KboFaRules* rules)
{
    if (rules != NULL) {
        memset(rules, 0, sizeof(*rules));
    }
    return 1;
}

uint32_t kbo_add_days_yyyymmdd(uint32_t yyyymmdd, uint32_t add_days)
{
    return yyyymmdd + add_days;
}

void kbo_log_runtimef_at(const char* file, int line, const char* format, ...)
{
    (void)file;
    (void)line;
    (void)format;
}
