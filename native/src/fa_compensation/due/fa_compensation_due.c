#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../core/core_flags/api/flags_api.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/logging/core_log.h"
#include "../../fa_rules/fa_rules.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../decisions/fa_compensation_decisions.h"
#include "../finance/fa_compensation_cash_transfer.h"
#include "fa_compensation_due.h"
#include "fa_compensation_due_ortools.h"
#include "../news/fa_compensation_news_transfer.h"
#include "../protection/fa_compensation_protected_lists.h"
#include "../protection/fa_compensation_protection_score.h"
#include "../records/fa_compensation_records.h"
#include "../selection/fa_compensation_selection.h"
#include "../state/fa_compensation_state.h"

static volatile LONG g_kbo_fa_compensation_due_processing = 0;

static int kbo_submit_fa_compensation_protected_list_record(
    KboFaCompensationRecord* rec,
    uint32_t today,
    const KboFaRules* rules,
    KboFaProtectedCandidate* candidates,
    KboFaProtectedCandidate* auto_protected,
    const char* source)
{
    if (rec == NULL || rules == NULL || candidates == NULL || auto_protected == NULL
            || !rec->requires_player_compensation
            || rec->protect_count == 0u
            || rec->signed_on_yyyymmdd == 0u) {
        return 0;
    }
    if (rec->status != KBO_FA_COMPENSATION_STATUS_PENDING
            && rec->status != KBO_FA_COMPENSATION_STATUS_RECORDED) {
        return 0;
    }

    uint32_t due = kbo_add_days_yyyymmdd(rec->signed_on_yyyymmdd, rules->protected_list_due_days);
    if (due == 0u) {
        return 0;
    }

    int candidate_count = kbo_build_fa_compensation_protected_candidates(
        rec,
        candidates,
        KBO_FA_COMPENSATION_PROTECTED_LIST_MAX,
        auto_protected,
        KBO_FA_COMPENSATION_PROTECTED_LIST_MAX);
    if (candidate_count <= 0) {
        kbo_log_runtimef(
            "KBO FA protected list deferred reason=no_candidates fa_player=%u signing_team=%u due=%u",
            rec->player_id,
            rec->signing_team_id,
            due);
        return 0;
    }

    int auto_protected_count = 0;
    while (auto_protected_count < KBO_FA_COMPENSATION_PROTECTED_LIST_MAX
            && auto_protected[auto_protected_count].player_id != 0u) {
        auto_protected_count++;
    }
    kbo_fa_compensation_apply_ortools_order(rec, candidates, candidate_count, source);
    kbo_persist_fa_compensation_protection_debug(
        rec,
        due,
        today,
        candidates,
        candidate_count,
        auto_protected,
        auto_protected_count,
        source);
    int first_list_submission = rec->status == KBO_FA_COMPENSATION_STATUS_PENDING;
    if (!kbo_persist_fa_compensation_protected_list(rec, due, today, candidates, candidate_count, source)) {
        return 0;
    }

    int protected_count = candidate_count > (int)rec->protect_count ? (int)rec->protect_count : candidate_count;
    int unprotected_count_for_news = candidate_count - protected_count;
    if (first_list_submission) {
        kbo_emit_fa_compensation_protected_list_submitted_news(
            rec,
            today,
            rules->protected_list_due_days,
            protected_count,
            unprotected_count_for_news);
    }

    KboFaProtectedCandidate selected;
    int unprotected_count = 0;
    if (kbo_select_fa_compensation_player_from_candidates(
            rec,
            candidates,
            candidate_count,
            &selected,
            &unprotected_count)) {
        if (kbo_fa_compensation_ai_prefers_cash_only(rec, &selected, unprotected_count)
                && kbo_persist_fa_compensation_cash_only_decision(
                    rec,
                    due,
                    today,
                    source != NULL ? source : "fa_compensation_ai_cash_only")) {
            rec->status = KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED;
        } else if (kbo_persist_fa_compensation_player_decision(
                rec,
                due,
                today,
                &selected,
                unprotected_count,
                source != NULL ? source : "fa_compensation_ai_default")) {
            rec->status = KBO_FA_COMPENSATION_STATUS_PLAYER_SELECTED;
        } else {
            rec->status = KBO_FA_COMPENSATION_STATUS_RECORDED;
        }
    } else {
        if (kbo_persist_fa_compensation_cash_only_decision(
                rec,
                due,
                today,
                source != NULL ? source : "fa_compensation_ai_cash_only_no_player")) {
            rec->status = KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED;
        } else {
            rec->status = KBO_FA_COMPENSATION_STATUS_RECORDED;
        }
    }
    return 1;
}

int kbo_manual_submit_fa_compensation_protected_list(uint32_t fa_player_id, const char* source)
{
    if (fa_player_id == 0u) {
        return 0;
    }
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        return 0;
    }
    KboFaRules rules;
    kbo_fa_rules_load(&rules);

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    KboFaProtectedCandidate* candidates = (KboFaProtectedCandidate*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaProtectedCandidate));
    KboFaProtectedCandidate* auto_protected = (KboFaProtectedCandidate*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaProtectedCandidate));
    if (records == NULL || candidates == NULL || auto_protected == NULL) {
        if (records != NULL) { HeapFree(GetProcessHeap(), 0, records); }
        if (candidates != NULL) { HeapFree(GetProcessHeap(), 0, candidates); }
        if (auto_protected != NULL) { HeapFree(GetProcessHeap(), 0, auto_protected); }
        return 0;
    }

    kbo_fa_compensation_lock_ledger(source != NULL ? source : "manual_protected_list_submit");
    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    int ok = 0;
    for (int i = 0; i < record_count; i++) {
        if (records[i].player_id == fa_player_id) {
            ok = kbo_submit_fa_compensation_protected_list_record(
                &records[i],
                today,
                &rules,
                candidates,
                auto_protected,
                source != NULL ? source : "hub_protected_list_submit");
            break;
        }
    }
    if (ok) {
        kbo_persist_fa_compensation_records(records, record_count);
    }

    kbo_fa_compensation_unlock_ledger();
    HeapFree(GetProcessHeap(), 0, auto_protected);
    HeapFree(GetProcessHeap(), 0, candidates);
    HeapFree(GetProcessHeap(), 0, records);
    return ok;
}

int kbo_process_due_fa_compensation_protected_lists_for_date(uint32_t today, const char* source)
{
    if (!kbo_fix_enabled() || read_kbo_localappdata_flag_file("disable_kbo_fa_compensation.txt")) {
        return 0;
    }

    if (today == 0u) {
        return 0;
    }
    if (InterlockedCompareExchange(&g_kbo_fa_compensation_due_processing, 1, 0) != 0) {
        return 0;
    }

    KboFaRules rules;
    kbo_fa_rules_load(&rules);

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    KboFaProtectedCandidate* candidates = (KboFaProtectedCandidate*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaProtectedCandidate));
    KboFaProtectedCandidate* auto_protected = (KboFaProtectedCandidate*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaProtectedCandidate));
    KboFaCompensationDueTask* tasks = (KboFaCompensationDueTask*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationDueTask));
    if (records == NULL || candidates == NULL || auto_protected == NULL || tasks == NULL) {
        if (records != NULL) { HeapFree(GetProcessHeap(), 0, records); }
        if (candidates != NULL) { HeapFree(GetProcessHeap(), 0, candidates); }
        if (auto_protected != NULL) { HeapFree(GetProcessHeap(), 0, auto_protected); }
        if (tasks != NULL) { HeapFree(GetProcessHeap(), 0, tasks); }
        InterlockedExchange(&g_kbo_fa_compensation_due_processing, 0);
        return 0;
    }

    kbo_fa_compensation_lock_ledger(source != NULL ? source : "process_due");
    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    int task_count = 0;
    for (int i = 0; i < record_count; i++) {
        KboFaCompensationRecord* rec = &records[i];
        if (rec->status == KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED) {
            tasks[task_count].rec = *rec;
            tasks[task_count].action = KBO_FA_COMPENSATION_DUE_TASK_RECORD_CASH;
            task_count++;
            continue;
        }
        if (rec->status == KBO_FA_COMPENSATION_STATUS_PLAYER_SELECTED) {
            tasks[task_count].rec = *rec;
            tasks[task_count].action = KBO_FA_COMPENSATION_DUE_TASK_TRANSFER_PLAYER;
            task_count++;
            continue;
        }
        if (rec->status == KBO_FA_COMPENSATION_STATUS_PENDING
                && !rec->requires_player_compensation
                && rec->cash_only > 0u) {
            tasks[task_count].rec = *rec;
            tasks[task_count].action = KBO_FA_COMPENSATION_DUE_TASK_RECORD_CASH;
            task_count++;
            continue;
        }
        if (rec->status != KBO_FA_COMPENSATION_STATUS_PENDING
                || !rec->requires_player_compensation
                || rec->protect_count == 0u
                || rec->signed_on_yyyymmdd == 0u) {
            continue;
        }
        uint32_t due = kbo_add_days_yyyymmdd(rec->signed_on_yyyymmdd, rules.protected_list_due_days);
        if (due == 0u || today < due) {
            continue;
        }

        tasks[task_count].rec = *rec;
        tasks[task_count].action = KBO_FA_COMPENSATION_DUE_TASK_SUBMIT_LIST;
        task_count++;
    }
    kbo_fa_compensation_unlock_ledger();

    int generated = 0;
    for (int t = 0; t < task_count; t++) {
        KboFaCompensationRecord task_rec = tasks[t].rec;
        uint8_t new_status = task_rec.status;
        int changed = 0;

        if (tasks[t].action == KBO_FA_COMPENSATION_DUE_TASK_RECORD_CASH) {
            KboFaCompensationDecisionRow decision;
            int cash_recorded = 0;
            if (!task_rec.requires_player_compensation) {
                cash_recorded = task_rec.cash_only > 0u;
            } else if (kbo_load_latest_fa_compensation_decision(task_rec.player_id, &decision)
                    && strcmp(decision.action, "CASH_ONLY") == 0) {
                cash_recorded = 1;
            }
            if (cash_recorded) {
                kbo_log_runtimef(
                    "KBO FA compensation cash-only recorded fa_player=%u original_team=%u signing_team=%u cash_only=%u source=%s",
                    task_rec.player_id,
                    task_rec.original_team_id,
                    task_rec.signing_team_id,
                    task_rec.cash_only,
                    source != NULL ? source : "");
                kbo_apply_fa_compensation_cash_transfer(
                    &task_rec,
                    task_rec.cash_only,
                    today,
                    task_rec.requires_player_compensation ? "CASH_ONLY_DECISION" : "CASH_ONLY_SIGNING",
                    source != NULL ? source : "fa_compensation_due");
                new_status = KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED;
                changed = 1;
            }
        } else if (tasks[t].action == KBO_FA_COMPENSATION_DUE_TASK_TRANSFER_PLAYER) {
            KboFaCompensationDecisionRow decision;
            if (kbo_load_latest_fa_compensation_decision(task_rec.player_id, &decision)
                    && strcmp(decision.action, "PLAYER") == 0) {
                KboFaProtectedCandidate selected;
                memset(&selected, 0, sizeof(selected));
                selected.player_id = decision.selected_player_id;
                selected.team_id = task_rec.signing_team_id;
                selected.score = decision.selected_player_score;
                snprintf(selected.player_name, sizeof(selected.player_name), "%s", decision.selected_player_name);
                if (kbo_transfer_fa_compensation_player_to_original_team(&task_rec, &selected, today, source)) {
                    kbo_apply_fa_compensation_cash_transfer(
                        &task_rec,
                        task_rec.cash_with_player,
                        today,
                        "PLAYER_PLUS_CASH",
                        source != NULL ? source : "fa_compensation_due");
                    kbo_emit_fa_compensation_player_selected_news(&task_rec, &selected, today);
                    new_status = KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED;
                    changed = 1;
                }
            }
        } else if (tasks[t].action == KBO_FA_COMPENSATION_DUE_TASK_SUBMIT_LIST) {
            if (kbo_submit_fa_compensation_protected_list_record(
                    &task_rec,
                    today,
                    &rules,
                    candidates,
                    auto_protected,
                    source)) {
                new_status = task_rec.status;
                changed = 1;
            }
        }

        if (!changed) {
            continue;
        }

        kbo_fa_compensation_lock_ledger(source != NULL ? source : "process_due_update");
        memset(records, 0, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
        record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
        for (int i = 0; i < record_count; i++) {
            KboFaCompensationRecord* current = &records[i];
            if (current->player_id == task_rec.player_id
                    && current->season == task_rec.season
                    && current->original_team_id == task_rec.original_team_id
                    && current->signing_team_id == task_rec.signing_team_id) {
                current->status = new_status;
                kbo_persist_fa_compensation_records(records, record_count);
                generated++;
                break;
            }
        }
        kbo_fa_compensation_unlock_ledger();
    }

    HeapFree(GetProcessHeap(), 0, tasks);
    HeapFree(GetProcessHeap(), 0, auto_protected);
    HeapFree(GetProcessHeap(), 0, candidates);
    HeapFree(GetProcessHeap(), 0, records);
    InterlockedExchange(&g_kbo_fa_compensation_due_processing, 0);
    return generated;
}

int kbo_process_due_fa_compensation_protected_lists(const char* source)
{
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        return 0;
    }
    return kbo_process_due_fa_compensation_protected_lists_for_date(today, source);
}
