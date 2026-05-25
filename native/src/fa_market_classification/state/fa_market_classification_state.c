#include "../internal/fa_market_policy_internal.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/files/save_paths/platform/core_path_io.h"
#include "../../fa_filing/fa_filing.h"
#include "../../fa_filing/fa_filing_parts/fa_filing_csv_parse.h"
#include "../../fa_rules/fa_rules.h"
#include "../../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/rights/query/foreign_waiver_rights_query.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"
#include "../api/fa_market_classification.h"
#include "../seeds/fa_market_seed_cases.h"


typedef struct KboFaMarketSqlite3 KboFaMarketSqlite3;
typedef struct KboFaMarketSqlite3Stmt KboFaMarketSqlite3Stmt;
typedef int (__cdecl *KboSqlite3OpenV2Fn)(const char*, KboFaMarketSqlite3**, int, const char*);
typedef int (__cdecl *KboSqlite3CloseFn)(KboFaMarketSqlite3*);
typedef int (__cdecl *KboSqlite3PrepareV2Fn)(KboFaMarketSqlite3*, const char*, int, KboFaMarketSqlite3Stmt**, const char**);
typedef int (__cdecl *KboSqlite3StepFn)(KboFaMarketSqlite3Stmt*);
typedef int (__cdecl *KboSqlite3FinalizeFn)(KboFaMarketSqlite3Stmt*);
typedef int (__cdecl *KboSqlite3BindIntFn)(KboFaMarketSqlite3Stmt*, int, int);
typedef int (__cdecl *KboSqlite3ResetFn)(KboFaMarketSqlite3Stmt*);
typedef int (__cdecl *KboSqlite3ClearBindingsFn)(KboFaMarketSqlite3Stmt*);
typedef const unsigned char* (__cdecl *KboSqlite3ColumnTextFn)(KboFaMarketSqlite3Stmt*, int);
typedef const char* (__cdecl *KboSqlite3ErrmsgFn)(KboFaMarketSqlite3*);

typedef struct KboFaMarketSqliteApi {
    HMODULE module;
    int attempted;
    int available;
    KboSqlite3OpenV2Fn open_v2;
    KboSqlite3CloseFn close;
    KboSqlite3PrepareV2Fn prepare_v2;
    KboSqlite3StepFn step;
    KboSqlite3FinalizeFn finalize;
    KboSqlite3BindIntFn bind_int;
    KboSqlite3ResetFn reset;
    KboSqlite3ClearBindingsFn clear_bindings;
    KboSqlite3ColumnTextFn column_text;
    KboSqlite3ErrmsgFn errmsg;
} KboFaMarketSqliteApi;

typedef struct KboFaMarketFileSignature {
    int exists;
    DWORD size_high;
    DWORD size_low;
    FILETIME last_write_time;
} KboFaMarketFileSignature;

KboFaMarketSqliteApi g_kbo_fa_market_sqlite_api = {0};
KboFaMarketHistoryCase g_kbo_fa_market_history_cache[KBO_FA_MARKET_CLASSIFICATION_MAX];
int g_kbo_fa_market_history_cache_count = 0;
int g_kbo_fa_market_history_cache_valid = 0;
char g_kbo_fa_market_history_cache_save_path[KBO_UTF8_PATH_BYTES] = {0};
KboFaMarketFileSignature g_kbo_fa_market_history_cache_db_sig = {0};
KboFaMarketFileSignature g_kbo_fa_market_history_cache_wal_sig = {0};
KboFaMarketFileSignature g_kbo_fa_market_history_cache_shm_sig = {0};

#define KBO_SQLITE_OK 0
#define KBO_SQLITE_ROW 100
#define KBO_SQLITE_DONE 101
#define KBO_SQLITE_OPEN_READONLY 0x00000001


KboFaMarketSqliteApi* kbo_fa_market_get_sqlite_api(void)
{
    if (g_kbo_fa_market_sqlite_api.attempted) {
        return g_kbo_fa_market_sqlite_api.available ? &g_kbo_fa_market_sqlite_api : NULL;
    }

    g_kbo_fa_market_sqlite_api.attempted = 1;
    HMODULE module = LoadLibraryA("winsqlite3.dll");
    if (module == NULL) {
        kbo_log_runtimef("FA market history sqlite unavailable reason=load_winsqlite3_failed gle=%lu", GetLastError());
        return NULL;
    }

    g_kbo_fa_market_sqlite_api.module = module;
    g_kbo_fa_market_sqlite_api.open_v2 =
        (KboSqlite3OpenV2Fn)GetProcAddress(module, "sqlite3_open_v2");
    g_kbo_fa_market_sqlite_api.close =
        (KboSqlite3CloseFn)GetProcAddress(module, "sqlite3_close");
    g_kbo_fa_market_sqlite_api.prepare_v2 =
        (KboSqlite3PrepareV2Fn)GetProcAddress(module, "sqlite3_prepare_v2");
    g_kbo_fa_market_sqlite_api.step =
        (KboSqlite3StepFn)GetProcAddress(module, "sqlite3_step");
    g_kbo_fa_market_sqlite_api.finalize =
        (KboSqlite3FinalizeFn)GetProcAddress(module, "sqlite3_finalize");
    g_kbo_fa_market_sqlite_api.bind_int =
        (KboSqlite3BindIntFn)GetProcAddress(module, "sqlite3_bind_int");
    g_kbo_fa_market_sqlite_api.reset =
        (KboSqlite3ResetFn)GetProcAddress(module, "sqlite3_reset");
    g_kbo_fa_market_sqlite_api.clear_bindings =
        (KboSqlite3ClearBindingsFn)GetProcAddress(module, "sqlite3_clear_bindings");
    g_kbo_fa_market_sqlite_api.column_text =
        (KboSqlite3ColumnTextFn)GetProcAddress(module, "sqlite3_column_text");
    g_kbo_fa_market_sqlite_api.errmsg =
        (KboSqlite3ErrmsgFn)GetProcAddress(module, "sqlite3_errmsg");

    g_kbo_fa_market_sqlite_api.available =
        g_kbo_fa_market_sqlite_api.open_v2 != NULL
        && g_kbo_fa_market_sqlite_api.close != NULL
        && g_kbo_fa_market_sqlite_api.prepare_v2 != NULL
        && g_kbo_fa_market_sqlite_api.step != NULL
        && g_kbo_fa_market_sqlite_api.finalize != NULL
        && g_kbo_fa_market_sqlite_api.bind_int != NULL
        && g_kbo_fa_market_sqlite_api.reset != NULL
        && g_kbo_fa_market_sqlite_api.clear_bindings != NULL
        && g_kbo_fa_market_sqlite_api.column_text != NULL
        && g_kbo_fa_market_sqlite_api.errmsg != NULL;
    if (!g_kbo_fa_market_sqlite_api.available) {
        kbo_log_runtime_line("FA market history sqlite unavailable reason=missing_winsqlite3_exports");
        return NULL;
    }
    return &g_kbo_fa_market_sqlite_api;
}





















const char* kbo_fa_market_display_grade(const char* grade)
{
    return kbo_fa_market_grade_is_unknown(grade) ? "-" : grade;
}




const char* kbo_fa_market_display_case_label(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return "-";
    }
    if (strcmp(case_label, "KBO_FA_APPROVED") == 0) { return "Official FA"; }
    if (strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0) { return "Eligible FA"; }
    if (strcmp(case_label, "KBO_FA_DEFERRED") == 0) { return "Deferred FA"; }
    if (strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0) { return "FA"; }
    if (strcmp(case_label, "KBO_FA_CARRYOVER_UNSIGNED") == 0) { return "Carryover FA"; }
    if (strcmp(case_label, "DOMESTIC_RELEASED_NON_FA") == 0) { return "Released"; }
    if (strcmp(case_label, "DOMESTIC_UNDRAFTED_FREE_AGENT") == 0) { return "Undrafted"; }
    if (strcmp(case_label, "DOMESTIC_INDEPENDENT_FUTURES_FA") == 0) { return "Futures Indy FA"; }
    if (strcmp(case_label, "DOMESTIC_INDEPENDENT_LEAGUE_FA") == 0) { return "Indy League FA"; }
    if (strcmp(case_label, "DOMESTIC_TEAMLESS_UNVERIFIED") == 0) { return "Teamless"; }
    if (strcmp(case_label, "DOMESTIC_MARKET_UNVERIFIED") == 0) { return "Teamless"; }
    if (strcmp(case_label, "FOREIGN_RESERVED_RIGHT") == 0) { return "Foreign Rights"; }
    if (strcmp(case_label, "FOREIGN_FREE") == 0) { return "Foreign FA"; }
    return case_label;
}





const KboFaMarketHistoryCase* kbo_find_fa_market_history_case(
    const KboFaMarketHistoryCase* histories,
    int history_count,
    uint32_t player_id)
{
    if (histories == NULL || history_count <= 0 || player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < history_count; i++) {
        if (histories[i].player_id == player_id && histories[i].found) {
            return &histories[i];
        }
    }
    return NULL;
}









