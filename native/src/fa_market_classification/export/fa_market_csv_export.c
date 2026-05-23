#include "../internal/fa_market_policy_internal.h"
#include "../../core/dates/constants/kbo_date_constants.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/logging/rule_audit.h"

static int kbo_fa_market_source_is_interactive_ui(const char* source)
{
    if (source == NULL) {
        return 0;
    }
    return strncmp(source, "f2_webview", 10) == 0
        || strncmp(source, "f2_text", 7) == 0;
}

static void kbo_fa_market_init_empty_histories(
    const KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories)
{
    if (rows == NULL || row_count <= 0 || histories == NULL || max_histories < row_count) {
        return;
    }
    memset(histories, 0, (SIZE_T)max_histories * sizeof(histories[0]));
    for (int i = 0; i < row_count; i++) {
        histories[i].player_id = rows[i].player_id;
    }
}

static int kbo_fa_market_try_load_cached_histories_for_ui(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories)
{
    if (rows == NULL || row_count <= 0 || histories == NULL || max_histories < row_count) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    KboFaMarketFileSignature db_sig = {0};
    KboFaMarketFileSignature wal_sig = {0};
    KboFaMarketFileSignature shm_sig = {0};
    if (kbo_get_current_save_path(save_path, sizeof(save_path))
            && kbo_fa_market_get_text_data_signatures(save_path, &db_sig, &wal_sig, &shm_sig)
            && kbo_fa_market_history_cache_matches(save_path, &db_sig, &wal_sig, &shm_sig)) {
        return kbo_fa_market_copy_history_cache_for_rows(rows, row_count, histories, max_histories);
    }

    kbo_fa_market_init_empty_histories(rows, row_count, histories, max_histories);
    return 0;
}

static int kbo_collect_fa_market_classifications_internal(
    uint32_t requested_league_id,
    KboFaMarketClassification* rows,
    int max_rows,
    int row_offset,
    KboFaMarketScanSummary* summary,
    int write_csv,
    const char* source)
{
    if (rows == NULL || max_rows <= 0) {
        return 0;
    }
    if (row_offset < 0) {
        row_offset = 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));
    if (summary != NULL) {
        memset(summary, 0, sizeof(*summary));
    }

    uint32_t league_id = kbo_fa_market_resolve_league_id(requested_league_id);
    uint32_t today = 0u;
    kbo_current_date_tick_latest_published_date(&today);
    uint32_t current_year = 0u;
    if (today != 0u) {
        current_year = today / 10000u;
    }

    if (summary != NULL) {
        summary->league_id = league_id;
        summary->today_yyyymmdd = today;
        summary->current_year = current_year;
        kbo_get_fa_market_classification_csv_path(summary->csv_path, sizeof(summary->csv_path));
    }

    ULONGLONG started_ms = GetTickCount64();

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "requested_league_id", requested_league_id);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_u32(&audit_fields, "today", today);
            kbo_rule_audit_emit_fields(
                "fa_market.classification.scan",
                "skip",
                "player_vector_unavailable",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtime_line("FA market classification: no player vector");
        return 0;
    }

    ULONGLONG load_started_ms = GetTickCount64();
    KboFaMarketSeedCase* seeds = (KboFaMarketSeedCase*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_SEED_MAX * sizeof(KboFaMarketSeedCase));
    KboFaSalarySnapshotGrade* salary_grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    KboFaSalarySnapshotGrade* carryover_salary_grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (seeds == NULL || salary_grades == NULL || carryover_salary_grades == NULL) {
        if (seeds != NULL) {
            HeapFree(GetProcessHeap(), 0, seeds);
        }
        if (salary_grades != NULL) {
            HeapFree(GetProcessHeap(), 0, salary_grades);
        }
        if (carryover_salary_grades != NULL) {
            HeapFree(GetProcessHeap(), 0, carryover_salary_grades);
        }
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "requested_league_id", requested_league_id);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_u32(&audit_fields, "today", today);
            kbo_rule_audit_emit_fields(
                "fa_market.classification.scan",
                "fail",
                "allocation_failed",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtime_line("FA market classification: allocation failed");
        return 0;
    }

    char seed_path[MAX_PATH] = {0};
    char salary_snapshot_path[MAX_PATH] = {0};
    int seed_count = kbo_load_fa_market_seed_cases(seeds, KBO_FA_MARKET_SEED_MAX, seed_path, sizeof(seed_path));
    uint32_t salary_grade_season = 0u;
    int salary_grade_count = kbo_fa_salary_snapshot_load_fa_market_grade_rows(
        current_year,
        salary_grades,
        KBO_FA_SALARY_SNAPSHOT_GRADE_MAX,
        salary_snapshot_path,
        sizeof(salary_snapshot_path),
        &salary_grade_season);
    int carryover_salary_grade_count = 0;
    if (current_year > KBO_SEASON_YEAR_MIN && salary_grade_season == current_year) {
        carryover_salary_grade_count = kbo_fa_salary_snapshot_load_grade_rows(
            current_year - 1u,
            carryover_salary_grades,
            KBO_FA_SALARY_SNAPSHOT_GRADE_MAX,
            NULL,
            0);
    }
    KboFaRules fa_rules;
    kbo_fa_rules_load(&fa_rules);
    if (summary != NULL) {
        summary->seed_count = seed_count;
        summary->salary_snapshot_count = salary_grade_count;
        snprintf(summary->seed_path, sizeof(summary->seed_path), "%s", seed_path);
        snprintf(summary->salary_snapshot_path, sizeof(summary->salary_snapshot_path), "%s", salary_snapshot_path);
    }
    ULONGLONG load_ms = GetTickCount64() - load_started_ms;

    ULONGLONG scan_started_ms = GetTickCount64();
    int row_count = 0;
    int candidate_count = 0;
    int scanned = 0;
    int truncated = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        scanned++;
        if (!kbo_fa_market_player_is_candidate(player, league_id)) {
            continue;
        }
        candidate_count++;
        if (candidate_count <= row_offset) {
            continue;
        }
        if (row_count >= max_rows) {
            truncated = 1;
            continue;
        }

        KboFaMarketClassification* row = &rows[row_count];
        row->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        row->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        row->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        row->original_team_id = kbo_fa_market_get_player_original_team_id(player);
        row->original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
        row->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        row->draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
        row->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        row->retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
        row->contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
        row->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        row->position_group = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
        row->position_role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
        row->fa_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
        row->foreign_player = kbo_player_is_foreign_for_kbo_rights(player) ? 1u : 0u;
        row->draft_class = player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET];
        row->draft_subtype = player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET];
        row->draft_eligible = player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET];
        row->draft_extra = player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET];
        row->generation_flags = player[OOTP27_PLAYER_GENERATION_FLAGS_OFFSET];
        row->generation_context = player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET];
        row->generation_grade = player[OOTP27_PLAYER_GENERATION_GRADE_OFFSET];
        row->generation_special = player[OOTP27_PLAYER_GENERATION_SPECIAL_OFFSET];
        kbo_copy_player_display_name(player, row->player_name, sizeof(row->player_name));
        if (row->player_name[0] == '\0' || strcmp(row->player_name, "Unknown player") == 0) {
            snprintf(row->player_name, sizeof(row->player_name), "Player #%u", row->player_id);
        }
        row_count++;
    }
    ULONGLONG scan_ms = GetTickCount64() - scan_started_ms;

    KboFaMarketHistoryCase* histories = NULL;
    int history_count = 0;
    int interactive_ui = kbo_fa_market_source_is_interactive_ui(source);
    ULONGLONG history_started_ms = GetTickCount64();
    if (row_count > 0) {
        histories = (KboFaMarketHistoryCase*)HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            (SIZE_T)row_count * sizeof(KboFaMarketHistoryCase));
        if (histories != NULL) {
            if (interactive_ui) {
                kbo_fa_market_try_load_cached_histories_for_ui(rows, row_count, histories, row_count);
            } else {
                kbo_load_fa_market_history_cases(rows, row_count, histories, row_count);
            }
            kbo_fa_market_overlay_filing_history_cases(rows, row_count, histories, row_count);
            history_count = row_count;
        } else {
            kbo_log_runtime_line("FA market classification: history allocation failed");
        }
    }
    ULONGLONG history_ms = GetTickCount64() - history_started_ms;

    ULONGLONG classify_started_ms = GetTickCount64();
    for (int i = 0; i < row_count; i++) {
        const KboFaMarketHistoryCase* history_case =
            kbo_find_fa_market_history_case(histories, history_count, rows[i].player_id);
        kbo_classify_fa_market_row(
            &rows[i],
            seeds,
            seed_count,
            history_case,
            today);
        kbo_fa_market_apply_salary_snapshot_grade(
            &rows[i],
            salary_grades,
            salary_grade_count,
            &fa_rules);
        kbo_fa_market_apply_carryover_salary_snapshot_grade(
            &rows[i],
            carryover_salary_grades,
            carryover_salary_grade_count,
            &fa_rules,
            current_year);
    }
    ULONGLONG classify_ms = GetTickCount64() - classify_started_ms;

    if (row_count > 1) {
        qsort(rows, (size_t)row_count, sizeof(rows[0]), kbo_compare_fa_market_classification_rows);
    }

    if (summary != NULL) {
        summary->scanned = scanned;
        summary->candidates = candidate_count;
        summary->rows = row_count;
        summary->truncated = truncated;
    }

    if (write_csv) {
        kbo_write_fa_market_classification_csv(rows, row_count, summary, source);
        if (summary != NULL) {
            summary->csv_written = 1;
        }
    }

    ULONGLONG total_ms = GetTickCount64() - started_ms;
    if (total_ms >= 100u) {
        kbo_log_runtimef(
            "FA market classification timing source=%s rows=%d candidates=%d scanned=%d offset=%d interactive=%d load_ms=%llu scan_ms=%llu history_ms=%llu classify_ms=%llu total_ms=%llu",
            source != NULL ? source : "",
            row_count,
            candidate_count,
            scanned,
            row_offset,
            interactive_ui,
            (unsigned long long)load_ms,
            (unsigned long long)scan_ms,
            (unsigned long long)history_ms,
            (unsigned long long)classify_ms,
            (unsigned long long)total_ms);
    }
    if (!interactive_ui) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "requested_league_id", requested_league_id);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_u32(&audit_fields, "today", today);
            kbo_log_field_u32(&audit_fields, "current_year", current_year);
            kbo_log_field_u32(&audit_fields, "salary_grade_season", salary_grade_season);
            kbo_log_field_i32(&audit_fields, "rows", row_count);
            kbo_log_field_i32(&audit_fields, "candidates", candidate_count);
            kbo_log_field_i32(&audit_fields, "scanned", scanned);
            kbo_log_field_i32(&audit_fields, "truncated", truncated);
            kbo_log_field_i32(&audit_fields, "seed_count", seed_count);
            kbo_log_field_i32(&audit_fields, "salary_snapshot", salary_grade_count);
            kbo_log_field_i32(&audit_fields, "write_csv", write_csv);
            kbo_log_field_i32(&audit_fields, "csv_written", summary != NULL ? summary->csv_written : 0);
            kbo_log_field_u64(&audit_fields, "total_ms", (unsigned long long)total_ms);
            kbo_rule_audit_emit_fields(
                "fa_market.classification.scan",
                row_count > 0 ? "classify" : "scan_empty",
                row_count > 0 ? "candidates_classified" : "no_candidates",
                source,
                &audit_fields);
        } while (0);
    }

    HeapFree(GetProcessHeap(), 0, seeds);
    HeapFree(GetProcessHeap(), 0, salary_grades);
    HeapFree(GetProcessHeap(), 0, carryover_salary_grades);
    if (histories != NULL) {
        HeapFree(GetProcessHeap(), 0, histories);
    }
    return row_count;
}

int kbo_collect_fa_market_classifications(
    uint32_t requested_league_id,
    KboFaMarketClassification* rows,
    int max_rows,
    KboFaMarketScanSummary* summary,
    int write_csv,
    const char* source)
{
    return kbo_collect_fa_market_classifications_internal(
        requested_league_id,
        rows,
        max_rows,
        0,
        summary,
        write_csv,
        source);
}

int kbo_collect_fa_market_classifications_page(
    uint32_t requested_league_id,
    KboFaMarketClassification* rows,
    int max_rows,
    int row_offset,
    KboFaMarketScanSummary* summary,
    int write_csv,
    const char* source)
{
    return kbo_collect_fa_market_classifications_internal(
        requested_league_id,
        rows,
        max_rows,
        row_offset,
        summary,
        write_csv,
        source);
}
