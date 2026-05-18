#include "../internal/fa_market_policy_internal.h"

int kbo_load_fa_market_history_cases(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories)
{
    if (rows == NULL || row_count <= 0 || histories == NULL || max_histories < row_count) {
        return 0;
    }
    memset(histories, 0, (SIZE_T)max_histories * sizeof(histories[0]));
    for (int i = 0; i < row_count; i++) {
        histories[i].player_id = rows[i].player_id;
    }

    char save_path[MAX_PATH] = {0};
    KboFaMarketFileSignature db_sig = {0};
    KboFaMarketFileSignature wal_sig = {0};
    KboFaMarketFileSignature shm_sig = {0};
    int have_signature = 0;
    if (kbo_get_current_save_path(save_path, sizeof(save_path))) {
        have_signature = kbo_fa_market_get_text_data_signatures(save_path, &db_sig, &wal_sig, &shm_sig);
        if (have_signature && kbo_fa_market_history_cache_matches(save_path, &db_sig, &wal_sig, &shm_sig)) {
            int cached_found = kbo_fa_market_copy_history_cache_for_rows(rows, row_count, histories, max_histories);
            kbo_log_runtimef("FA market history sqlite cache hit found=%d rows=%d save=%s", cached_found, row_count, save_path);
            return cached_found;
        }
    }

    KboFaMarketSqliteApi* api = kbo_fa_market_get_sqlite_api();
    if (api == NULL) {
        return 0;
    }

    char db_path[MAX_PATH] = {0};
    if (!kbo_fa_market_copy_text_data_sqlite(db_path, sizeof(db_path))) {
        return 0;
    }

    KboFaMarketSqlite3* db = NULL;
    int open_result = api->open_v2(db_path, &db, KBO_SQLITE_OPEN_READONLY, NULL);
    if (open_result != KBO_SQLITE_OK || db == NULL) {
        kbo_log_runtimef(
            "FA market history sqlite open failed result=%d path=%s msg=%s",
            open_result,
            db_path,
            (db != NULL && api->errmsg != NULL) ? api->errmsg(db) : "");
        if (db != NULL) {
            api->close(db);
        }
        return 0;
    }

    const char* sql =
        "SELECT history_date, history_text "
        "FROM player_history "
        "WHERE player_id=?1 AND ("
        "history_text='[G]Became a free agent.' "
        "OR history_text LIKE '[G]Was not drafted and became a free agent%' "
        "OR history_text LIKE '[G]Released by %') "
        "ORDER BY history_date DESC, history_id DESC LIMIT 1;";

    KboFaMarketSqlite3Stmt* stmt = NULL;
    int prepare_result = api->prepare_v2(db, sql, -1, &stmt, NULL);
    if (prepare_result != KBO_SQLITE_OK || stmt == NULL) {
        kbo_log_runtimef(
            "FA market history sqlite prepare failed result=%d msg=%s",
            prepare_result,
            api->errmsg != NULL ? api->errmsg(db) : "");
        api->close(db);
        return 0;
    }

    int found = 0;
    for (int i = 0; i < row_count; i++) {
        api->reset(stmt);
        api->clear_bindings(stmt);
        api->bind_int(stmt, 1, (int)rows[i].player_id);
        int step_result = api->step(stmt);
        if (step_result == KBO_SQLITE_ROW) {
            histories[i].found = 1;
            kbo_fa_market_copy_sqlite_text_column(api, stmt, 0, histories[i].history_date, sizeof(histories[i].history_date));
            kbo_fa_market_copy_sqlite_text_column(api, stmt, 1, histories[i].history_text, sizeof(histories[i].history_text));
            kbo_fa_market_mark_history_case(&histories[i]);
            found++;
        } else if (step_result != KBO_SQLITE_DONE) {
            kbo_log_runtimef(
                "FA market history sqlite step failed player=%u result=%d msg=%s",
                rows[i].player_id,
                step_result,
                api->errmsg != NULL ? api->errmsg(db) : "");
        }
    }

    api->finalize(stmt);
    api->close(db);
    kbo_log_runtimef("FA market history sqlite loaded found=%d rows=%d db=%s", found, row_count, db_path);
    if (have_signature) {
        kbo_fa_market_store_history_cache(save_path, &db_sig, &wal_sig, &shm_sig, histories, row_count);
    }
    return found;
}

int kbo_fa_market_overlay_filing_history_cases(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int history_count)
{
    if (rows == NULL || row_count <= 0 || histories == NULL || history_count < row_count) {
        return 0;
    }

    KboFaFilingRecord* filings = (KboFaFilingRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_FILING_MAX * sizeof(KboFaFilingRecord));
    if (filings == NULL) {
        kbo_log_runtime_line("FA market filing ledger overlay skipped: allocation failed");
        return 0;
    }

    char path[MAX_PATH] = {0};
    int filing_count = kbo_load_fa_filing_records(filings, KBO_FA_FILING_MAX, path, sizeof(path));
    int applied = 0;
    for (int f = 0; f < filing_count; f++) {
        const KboFaFilingRecord* filing = &filings[f];
        if (filing->player_id == 0u || filing->filing_date == 0u) {
            continue;
        }
        for (int i = 0; i < row_count; i++) {
            if (rows[i].player_id != filing->player_id) {
                continue;
            }
            uint32_t existing_date = kbo_fa_market_history_date_u32(&histories[i]);
            if (histories[i].found && existing_date > filing->filing_date) {
                break;
            }

            histories[i].player_id = rows[i].player_id;
            histories[i].found = 1;
            histories[i].became_free_agent = 1;
            histories[i].undrafted_free_agent = 0;
            histories[i].released = 0;
            snprintf(histories[i].history_date, sizeof(histories[i].history_date), "%u", filing->filing_date);
            snprintf(histories[i].history_text, sizeof(histories[i].history_text), "[G]Became a free agent.");
            if (rows[i].original_team_id == 0u && filing->original_team_id != 0u) {
                rows[i].original_team_id = filing->original_team_id;
            }
            rows[i].fa_filing_date = filing->filing_date;
            rows[i].fa_filing_season = filing->season;
            applied++;
            break;
        }
    }

    if (filing_count > 0 || applied > 0) {
        static DWORD last_overlay_log_tick = 0u;
        DWORD now = GetTickCount();
        if (last_overlay_log_tick != 0u && now >= last_overlay_log_tick && now - last_overlay_log_tick < 60000u) {
            HeapFree(GetProcessHeap(), 0, filings);
            return applied;
        }
        last_overlay_log_tick = now;
        kbo_log_runtimef(
            "FA market filing ledger overlay applied=%d filings=%d rows=%d csv=%s",
            applied,
            filing_count,
            row_count,
            path);
    }
    HeapFree(GetProcessHeap(), 0, filings);
    return applied;
}

void kbo_classify_fa_market_row(
    KboFaMarketClassification* row,
    const KboFaMarketSeedCase* seeds,
    int seed_count,
    const KboFaMarketHistoryCase* history_case,
    uint32_t today_yyyymmdd)
{
    if (row == NULL) {
        return;
    }

    snprintf(row->grade, sizeof(row->grade), "UNKNOWN");
    row->case_label[0] = '\0';
    row->reason[0] = '\0';

    const KboFaMarketSeedCase* seed = kbo_find_fa_market_seed_case(seeds, seed_count, row->player_id);
    if (seed != NULL) {
        const char* seed_case = kbo_fa_market_canonical_case_label(seed->case_label);
        snprintf(row->case_label, sizeof(row->case_label), "%s", seed_case != NULL ? seed_case : seed->case_label);
        snprintf(row->grade, sizeof(row->grade), "%s", seed->grade[0] != '\0' ? seed->grade : "UNKNOWN");
        if (seed->note[0] != '\0') {
            snprintf(row->reason, sizeof(row->reason), "%s", seed->note);
        } else if (kbo_fa_market_case_is_seeded_official(seed_case)) {
            snprintf(row->reason, sizeof(row->reason), "manual official FA seed");
        } else {
            snprintf(row->reason, sizeof(row->reason), "manual case seed");
        }
        return;
    }

    if (row->foreign_player) {
        uint32_t rights_team_id = 0u;
        if (today_yyyymmdd != 0u && kbo_find_active_foreign_waiver_holder(row->player_id, today_yyyymmdd, &rights_team_id)) {
            row->rights_team_id = rights_team_id;
            snprintf(row->case_label, sizeof(row->case_label), "FOREIGN_RESERVED_RIGHT");
            snprintf(row->reason, sizeof(row->reason), "active foreign reserve right held by team #%u", rights_team_id);
            return;
        }
        snprintf(row->case_label, sizeof(row->case_label), "FOREIGN_FREE");
        snprintf(row->reason, sizeof(row->reason), "foreign player with no active KBO reserve right");
        return;
    }

    if (history_case != NULL && history_case->found && history_case->became_free_agent) {
        kbo_fa_market_apply_history_filing_metadata(row, history_case);
        int independent_kind = kbo_fa_market_row_independent_source_kind(row);
        if (independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE
                && kbo_fa_market_history_is_carryover_unsigned(row, history_case, today_yyyymmdd)) {
            snprintf(row->case_label, sizeof(row->case_label), "KBO_FA_CARRYOVER_UNSIGNED");
            kbo_fa_market_set_history_reason(
                row,
                history_case,
                "player remains unsigned from prior FA filing; eligibility carried over");
            return;
        }
    }

    if (kbo_fa_market_apply_history_case(row, history_case)) {
        return;
    }

    int independent_kind = kbo_fa_market_row_independent_source_kind(row);
    if (independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES
            || independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE) {
        uint32_t original_team_league_id = kbo_fa_market_get_team_league_id(row->original_team_id);
        uint32_t active_team_league_id = kbo_fa_market_get_team_league_id(row->active_team_id);
        snprintf(
            row->case_label,
            sizeof(row->case_label),
            "%s",
            independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES
                ? "DOMESTIC_INDEPENDENT_FUTURES_FA"
                : "DOMESTIC_INDEPENDENT_LEAGUE_FA");
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s original_team_league=%u active_team_league=%u original_team=%u active_team=%u original_league=%u current_league=%u draft_league=%u",
            independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES
                ? "domestic independent futures-team free agent marker"
                : "domestic true independent-league free agent marker",
            original_team_league_id,
            active_team_league_id,
            row->original_team_id,
            row->active_team_id,
            row->original_league_id,
            row->current_league_id,
            row->draft_league_id);
        return;
    }

    if (kbo_fa_market_row_is_undrafted_domestic(row)) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_UNDRAFTED_FREE_AGENT");
        snprintf(
            row->reason,
            sizeof(row->reason),
            "domestic undrafted free agent marker draft_league=%u subtype=%u gen=%u/%u",
            row->draft_league_id,
            (uint32_t)row->draft_subtype,
            (uint32_t)row->generation_context,
            (uint32_t)row->generation_grade);
        return;
    }

    snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_TEAMLESS_UNVERIFIED");
    snprintf(row->reason, sizeof(row->reason), "domestic active teamless player; release/official FA origin not proven without seed");
}

int kbo_fa_market_case_rank(const char* case_label)
{
    if (case_label == NULL) {
        return 99;
    }
    if (strcmp(case_label, "KBO_FA_APPROVED") == 0) { return 10; }
    if (strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0) { return 11; }
    if (strcmp(case_label, "KBO_FA_DEFERRED") == 0) { return 12; }
    if (strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0) { return 13; }
    if (strcmp(case_label, "KBO_FA_CARRYOVER_UNSIGNED") == 0) { return 14; }
    if (strcmp(case_label, "DOMESTIC_RELEASED_NON_FA") == 0) { return 30; }
    if (strcmp(case_label, "DOMESTIC_UNDRAFTED_FREE_AGENT") == 0) { return 31; }
    if (strcmp(case_label, "DOMESTIC_INDEPENDENT_FUTURES_FA") == 0) { return 32; }
    if (strcmp(case_label, "DOMESTIC_INDEPENDENT_LEAGUE_FA") == 0) { return 33; }
    if (strcmp(case_label, "DOMESTIC_TEAMLESS_UNVERIFIED") == 0) { return 34; }
    if (strcmp(case_label, "DOMESTIC_MARKET_UNVERIFIED") == 0) { return 34; }
    if (strcmp(case_label, "FOREIGN_RESERVED_RIGHT") == 0) { return 40; }
    if (strcmp(case_label, "FOREIGN_FREE") == 0) { return 41; }
    return 99;
}

int kbo_compare_fa_market_classification_rows(const void* lhs, const void* rhs)
{
    const KboFaMarketClassification* a = (const KboFaMarketClassification*)lhs;
    const KboFaMarketClassification* b = (const KboFaMarketClassification*)rhs;
    int rank_a = kbo_fa_market_case_rank(a->case_label);
    int rank_b = kbo_fa_market_case_rank(b->case_label);
    if (rank_a != rank_b) {
        return rank_a - rank_b;
    }
    int name_cmp = _stricmp(a->player_name, b->player_name);
    if (name_cmp != 0) {
        return name_cmp;
    }
    if (a->player_id < b->player_id) { return -1; }
    if (a->player_id > b->player_id) { return 1; }
    return 0;
}

void kbo_fa_market_write_raw(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE || text == NULL) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
}

