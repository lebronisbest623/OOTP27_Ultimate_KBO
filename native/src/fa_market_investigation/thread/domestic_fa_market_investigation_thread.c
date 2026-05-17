#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../api/fa_market_investigation.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../fa_market_classification/api/fa_market_classification.h"
#include "../../fa_market_classification/policy/fa_market_policy.h"
#include "../rescue/domestic_fa_orphan_rescue.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "domestic_fa_market_investigation_scan.h"

static volatile LONG g_kbo_domestic_fa_market_investigation_started = 0;

static int kbo_domestic_fa_market_investigation_enabled(void)
{
    return read_kbo_localappdata_flag_file("enable_kbo_domestic_fa_market_investigation.txt")
        || kbo_domestic_fa_orphan_rescue_enabled();
}

static int kbo_domestic_fa_run_investigation_once(uint32_t today, const char* source)
{
    KboFaMarketClassification* rows = (KboFaMarketClassification*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_CLASSIFICATION_MAX * sizeof(KboFaMarketClassification));
    KboDomesticFaInvestigationCandidate* candidates = (KboDomesticFaInvestigationCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_DOMESTIC_FA_INVESTIGATION_MAX * sizeof(KboDomesticFaInvestigationCandidate));
    if (rows == NULL || candidates == NULL) {
        if (rows != NULL) { HeapFree(GetProcessHeap(), 0, rows); }
        if (candidates != NULL) { HeapFree(GetProcessHeap(), 0, candidates); }
        kbo_log_runtime_line("domestic FA market investigation: allocation failed");
        return 0;
    }

    KboFaMarketScanSummary summary;
    memset(&summary, 0, sizeof(summary));
    int row_count = kbo_collect_fa_market_classifications(
        0u,
        rows,
        KBO_FA_MARKET_CLASSIFICATION_MAX,
        &summary,
        0,
        source);

    int domestic_rows = 0;
    int relevant_rows = 0;
    int quality_rows = 0;
    int grade_a_or_b = 0;
    int snapshot_missing = 0;
    int high_demand = 0;
    int long_market = 0;
    int unofficial_or_pending = 0;
    int candidate_count = 0;
    for (int i = 0; i < row_count; i++) {
        KboFaMarketClassification* row = &rows[i];
        if (row->nation_id != OOTP27_KBO_KOREA_NATION_ID || row->foreign_player) {
            continue;
        }
        domestic_rows++;
        if (!kbo_domestic_fa_case_is_market_relevant(row->case_label)) {
            continue;
        }
        relevant_rows++;

        int32_t value_score = 0;
        int32_t overall = 0;
        int32_t talent = 0;
        int32_t ratings = 0;
        int32_t career = 0;
        uint8_t* player = kbo_find_player_by_id(row->player_id, NULL, NULL);
        if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            value_score = kbo_foreign_waiver_value_score(player);
            overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
            talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
            ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
            career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
        }

        if (!kbo_domestic_fa_row_is_quality_candidate(row, value_score)) {
            continue;
        }
        quality_rows++;
        if (strcmp(row->grade, "A") == 0 || strcmp(row->grade, "B") == 0) {
            grade_a_or_b++;
        }
        if (strcmp(row->fa_grade_flag, "SNAPSHOT_MISSING") == 0) {
            snapshot_missing++;
        }
        const KboFaMarketPolicy* policy = kbo_fa_market_policy();
        if (row->fa_demand >= policy->investigation_high_demand_min) {
            high_demand++;
        }
        if (!kbo_domestic_fa_case_is_official_or_probable(row->case_label)) {
            unofficial_or_pending++;
        }

        uint32_t history_date = kbo_domestic_fa_history_date_from_reason(row->reason);
        uint32_t market_days = kbo_domestic_fa_date_gap_days(history_date, today);
        if (market_days >= (uint32_t)policy->investigation_market_days_long_min) {
            long_market++;
        }

        if (candidate_count >= KBO_DOMESTIC_FA_INVESTIGATION_MAX) {
            continue;
        }

        KboDomesticFaInvestigationCandidate* candidate = &candidates[candidate_count++];
        candidate->row = *row;
        candidate->value_score = value_score;
        candidate->overall = overall;
        candidate->talent = talent;
        candidate->ratings = ratings;
        candidate->career = career;
        candidate->market_days = market_days;
        kbo_domestic_fa_describe_blockers(
            row,
            value_score,
            market_days,
            candidate->blockers,
            sizeof(candidate->blockers));
    }

    if (candidate_count > 1) {
        qsort(
            candidates,
            (size_t)candidate_count,
            sizeof(candidates[0]),
            kbo_domestic_fa_compare_candidates_desc);
    }

    kbo_domestic_fa_orphan_rescue_update_cache(today, candidates, candidate_count);

    char csv_path[MAX_PATH] = {0};
    int csv_written = kbo_domestic_fa_write_investigation_csv(
        candidates,
        candidate_count,
        &summary,
        csv_path,
        sizeof(csv_path));

    kbo_log_runtimef(
        "domestic FA market investigation summary date=%u league=%u rows=%d scanned=%d domestic=%d relevant=%d quality=%d grade_ab=%d high_demand=%d snapshot_missing=%d long_market=%d unofficial_or_pending=%d truncated=%d csv_written=%d csv=%s",
        today,
        summary.league_id,
        row_count,
        summary.scanned,
        domestic_rows,
        relevant_rows,
        quality_rows,
        grade_a_or_b,
        high_demand,
        snapshot_missing,
        long_market,
        unofficial_or_pending,
        summary.truncated,
        csv_written,
        csv_path);

    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    int top_count = candidate_count < policy->investigation_top_log_count
        ? candidate_count
        : policy->investigation_top_log_count;
    for (int i = 0; i < top_count; i++) {
        const KboDomesticFaInvestigationCandidate* candidate = &candidates[i];
        const KboFaMarketClassification* row = &candidate->row;
        kbo_log_runtimef(
            "domestic FA orphan watch rank=%d player=%u name=\"%s\" age=%u case=%s grade=%s score=%d ovr=%d tal=%d rat=%d car=%d demand=%d grade_salary=%d overall_rank=%u team_rank=%u original_team=%u market_days=%u blockers=%s reason=%s",
            i + 1,
            row->player_id,
            row->player_name,
            (uint32_t)row->age,
            row->case_label,
            row->grade,
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
            candidate->market_days,
            candidate->blockers,
            row->reason);
    }

    HeapFree(GetProcessHeap(), 0, rows);
    HeapFree(GetProcessHeap(), 0, candidates);
    return candidate_count;
}

static DWORD WINAPI kbo_domestic_fa_market_investigation_thread(LPVOID parameter)
{
    (void)parameter;

    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "domestic_fa_market_investigation",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER
            | KBO_CURRENT_DATE_TICK_CONSUMER_GAP_CATCHUP
            | KBO_CURRENT_DATE_TICK_CONSUMER_OBSERVE_CURRENT_WHEN_IDLE);

    int last_enabled = 0;
    while (kbo_runtime_threads_should_continue()) {
        const KboFaMarketPolicy* policy = kbo_fa_market_policy();
        if (!kbo_runtime_sleep_should_continue((uint32_t)policy->investigation_thread_sleep_ms)) {
            break;
        }

        int enabled = kbo_domestic_fa_market_investigation_enabled();
        if (!enabled) {
            last_enabled = 0;
            kbo_current_date_tick_consumer_skip_to_latest(&consumer);
            continue;
        }
        if (!last_enabled) {
            kbo_log_runtime_line("domestic FA market investigation enabled: observe-only mode active");
            kbo_current_date_tick_consumer_init(
                &consumer,
                "domestic_fa_market_investigation",
                KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER
                    | KBO_CURRENT_DATE_TICK_CONSUMER_GAP_CATCHUP
                    | KBO_CURRENT_DATE_TICK_CONSUMER_OBSERVE_CURRENT_WHEN_IDLE);
            last_enabled = 1;
        }

        if (!kbo_runtime_pause_for_save_if_needed("domestic_fa_market_investigation")) {
            break;
        }

        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
            const char* source = work.gap
                ? "domestic_fa_market_investigation_date_gap"
                : "domestic_fa_market_investigation";
            kbo_domestic_fa_run_investigation_once(work.date, source);
            if (kbo_runtime_save_in_progress()) {
                break;
            }
            kbo_current_date_tick_consumer_mark_processed(&consumer);
        }
    }

    InterlockedExchange(&g_kbo_domestic_fa_market_investigation_started, 0);
    kbo_log_runtime_line("domestic FA market investigation thread stopped");
    return 0;
}

void start_kbo_domestic_fa_market_investigation_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_domestic_fa_market_investigation_started, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(
            kbo_domestic_fa_market_investigation_thread,
            NULL,
            "domestic FA market investigation")) {
        InterlockedExchange(&g_kbo_domestic_fa_market_investigation_started, 0);
    }
}
