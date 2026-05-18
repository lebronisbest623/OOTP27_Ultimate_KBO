#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/logging/core_log.h"
#include "../../fa_rules/fa_rules.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../protection/policy/fa_compensation_protection_policy.h"
#include "../decisions/fa_compensation_decisions.h"
#include "../protection/fa_compensation_protection_score.h"
#include "../records/fa_compensation_records.h"
#include "fa_compensation_selection.h"

static int kbo_fa_compensation_grade_is_ab(const char* grade)
{
    return grade != NULL && (_stricmp(grade, "A") == 0 || _stricmp(grade, "B") == 0);
}

int kbo_select_fa_compensation_player_from_candidates(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    KboFaProtectedCandidate* out_selected,
    int* out_unprotected_count)
{
    if (out_selected != NULL) {
        memset(out_selected, 0, sizeof(*out_selected));
    }
    if (out_unprotected_count != NULL) {
        *out_unprotected_count = 0;
    }
    if (rec == NULL || candidates == NULL || out_selected == NULL || candidate_count <= 0) {
        return 0;
    }

    int protected_count = (int)rec->protect_count;
    if (protected_count < 0) {
        protected_count = 0;
    }
    if (protected_count > candidate_count) {
        protected_count = candidate_count;
    }
    int unprotected_count = candidate_count - protected_count;
    if (out_unprotected_count != NULL) {
        *out_unprotected_count = unprotected_count;
    }
    if (unprotected_count <= 0) {
        return 0;
    }

    int best_index = -1;
    int32_t best_score = -2147483647;
    char best_reason[96] = {0};
    for (int i = protected_count; i < candidate_count; i++) {
        char reason[96] = {0};
        int32_t score = kbo_fa_compensation_player_decision_score(rec, &candidates[i], reason, sizeof(reason));
        if (best_index < 0
                || score > best_score
                || (score == best_score && candidates[i].age < candidates[best_index].age)
                || (score == best_score && candidates[i].age == candidates[best_index].age
                    && candidates[i].player_id < candidates[best_index].player_id)) {
            best_index = i;
            best_score = score;
            snprintf(best_reason, sizeof(best_reason), "%s", reason);
        }
    }
    if (best_index < 0) {
        return 0;
    }

    *out_selected = candidates[best_index];
    out_selected->score = best_score;
    snprintf(out_selected->reason, sizeof(out_selected->reason), "%s", best_reason[0] != '\0' ? best_reason : "decision_score");
    return out_selected->player_id != 0u;
}

int kbo_fa_compensation_ai_prefers_cash_only(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    int unprotected_count)
{
    /* TODO: Feed FA compensation cost into the FA signing AI itself.
       A compensated FA should carry an acquisition penalty before the contract is offered,
       so AI clubs do not sign marginal players when the true price includes cash and a
       protected-list loss. This belongs at the FA target/offer valuation hook, not here:
       this function only decides player-plus-cash versus cash-only after a signing exists. */
    if (rec == NULL || rec->cash_only == 0u) {
        return 0;
    }
    if (selected == NULL || selected->player_id == 0u || unprotected_count <= 0) {
        return 1;
    }

    uint32_t extra_cash = rec->cash_only > rec->cash_with_player
        ? rec->cash_only - rec->cash_with_player : 0u;
    const KboFaCompensationProtectionPolicy* policy = kbo_fa_compensation_protection_policy();
    if (policy->cash_only_ab_grade_requires_no_player
            && kbo_fa_compensation_grade_is_ab(rec->grade)) {
        kbo_log_runtimef(
            "KBO FA compensation cash-only blocked reason=ab_grade_player_priority fa_player=%u grade=%s selected=%u score=%d unprotected=%d",
            rec->player_id,
            rec->grade,
            selected->player_id,
            selected->score,
            unprotected_count);
        return 0;
    }
    if (selected->score < policy->cash_only_score_threshold) {
        return 1;
    }
    if (selected->score < policy->cash_only_extra_cash_score_threshold
            && rec->previous_salary > 0
            && extra_cash >= (uint32_t)rec->previous_salary) {
        return 1;
    }
    return 0;
}

int kbo_manual_select_fa_compensation_player(
    uint32_t fa_player_id,
    uint32_t selected_player_id,
    const char* source)
{
    if (fa_player_id == 0u || selected_player_id == 0u) {
        return 0;
    }

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    KboFaCompensationProtectionDebugRow* rows = (KboFaCompensationProtectionDebugRow*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaCompensationProtectionDebugRow));
    if (records == NULL || rows == NULL) {
        if (records != NULL) { HeapFree(GetProcessHeap(), 0, records); }
        if (rows != NULL) { HeapFree(GetProcessHeap(), 0, rows); }
        return 0;
    }

    kbo_fa_compensation_lock_ledger(source != NULL ? source : "manual_compensation_select");
    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    KboFaCompensationRecord* rec = NULL;
    for (int i = 0; i < record_count; i++) {
        if (records[i].player_id == fa_player_id) {
            rec = &records[i];
            break;
        }
    }
    if (rec == NULL || !rec->requires_player_compensation || rec->protect_count == 0u
            || rec->status == KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED) {
        kbo_log_runtimef(
            "KBO FA manual compensation select skipped reason=record fa_player=%u selected=%u status=%u source=%s",
            fa_player_id,
            selected_player_id,
            rec != NULL ? (uint32_t)rec->status : 999u,
            source != NULL ? source : "");
        kbo_fa_compensation_unlock_ledger();
        HeapFree(GetProcessHeap(), 0, rows);
        HeapFree(GetProcessHeap(), 0, records);
        return 0;
    }

    int row_count = kbo_load_fa_compensation_protection_debug_rows(rows, KBO_FA_COMPENSATION_PROTECTED_LIST_MAX);
    KboFaProtectedCandidate selected;
    memset(&selected, 0, sizeof(selected));
    int unprotected_count = 0;
    for (int i = 0; i < row_count; i++) {
        if (rows[i].fa_player_id != fa_player_id) {
            continue;
        }
        if (strcmp(rows[i].list_type, "unprotected") == 0) {
            unprotected_count++;
            if (rows[i].player_id == selected_player_id) {
                selected.player_id = rows[i].player_id;
                selected.team_id = rows[i].signing_team_id;
                selected.age = rows[i].age;
                selected.role = rows[i].role;
                selected.score = rows[i].score;
                snprintf(selected.reason, sizeof(selected.reason), "%s", rows[i].reason);
                snprintf(selected.player_name, sizeof(selected.player_name), "%s", rows[i].player_name);
            }
        }
    }
    if (selected.player_id == 0u) {
        kbo_log_runtimef(
            "KBO FA manual compensation select skipped reason=not_unprotected fa_player=%u selected=%u source=%s",
            fa_player_id,
            selected_player_id,
            source != NULL ? source : "");
        kbo_fa_compensation_unlock_ledger();
        HeapFree(GetProcessHeap(), 0, rows);
        HeapFree(GetProcessHeap(), 0, records);
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today)) {
        today = rec->signed_on_yyyymmdd;
    }
    KboFaRules rules;
    kbo_fa_rules_load(&rules);
    uint32_t due = kbo_add_days_yyyymmdd(rec->signed_on_yyyymmdd, rules.protected_list_due_days);
    int ok = kbo_persist_fa_compensation_player_decision(
        rec,
        due,
        today,
        &selected,
        unprotected_count,
        source != NULL ? source : "hub_manual_compensation_select");
    if (ok) {
        rec->status = KBO_FA_COMPENSATION_STATUS_PLAYER_SELECTED;
        kbo_persist_fa_compensation_records(records, record_count);
    }

    kbo_fa_compensation_unlock_ledger();
    HeapFree(GetProcessHeap(), 0, rows);
    HeapFree(GetProcessHeap(), 0, records);
    return ok;
}

int kbo_manual_select_fa_compensation_cash_only(uint32_t fa_player_id, const char* source)
{
    if (fa_player_id == 0u) {
        return 0;
    }
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today)) {
        return 0;
    }
    KboFaRules rules;
    kbo_fa_rules_load(&rules);

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    if (records == NULL) {
        return 0;
    }
    kbo_fa_compensation_lock_ledger(source != NULL ? source : "manual_cash_only_select");
    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    int ok = 0;
    for (int i = 0; i < record_count; i++) {
        KboFaCompensationRecord* rec = &records[i];
        if (rec->player_id != fa_player_id) {
            continue;
        }
        if (!rec->requires_player_compensation
                || rec->status == KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED
                || rec->status == KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED) {
            break;
        }
        uint32_t due = kbo_add_days_yyyymmdd(rec->signed_on_yyyymmdd, rules.protected_list_due_days);
        ok = kbo_persist_fa_compensation_cash_only_decision(
            rec,
            due,
            today,
            source != NULL ? source : "hub_cash_only_select");
        if (ok) {
            rec->status = KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED;
            kbo_persist_fa_compensation_records(records, record_count);
        }
        break;
    }

    kbo_fa_compensation_unlock_ledger();
    HeapFree(GetProcessHeap(), 0, records);
    return ok;
}
