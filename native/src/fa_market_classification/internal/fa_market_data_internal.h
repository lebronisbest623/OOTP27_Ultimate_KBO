#ifndef KBOFIX_SRC_FA_MARKET_CLASSIFICATION_INTERNAL_FA_MARKET_DATA_INTERNAL_H_
#define KBOFIX_SRC_FA_MARKET_CLASSIFICATION_INTERNAL_FA_MARKET_DATA_INTERNAL_H_

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/csv/core_csv.h"
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
#include "../../team/classification/team_classification.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"
#include "../api/fa_market_classification.h"
#include "../policy/fa_market_policy.h"
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

#define KBO_SQLITE_OK 0
#define KBO_SQLITE_ROW 100
#define KBO_SQLITE_DONE 101
#define KBO_SQLITE_OPEN_READONLY 0x00000001

extern KboFaMarketSqliteApi g_kbo_fa_market_sqlite_api;
extern KboFaMarketHistoryCase g_kbo_fa_market_history_cache[KBO_FA_MARKET_CLASSIFICATION_MAX];
extern int g_kbo_fa_market_history_cache_count;
extern int g_kbo_fa_market_history_cache_valid;
extern char g_kbo_fa_market_history_cache_save_path[KBO_UTF8_PATH_BYTES];
extern KboFaMarketFileSignature g_kbo_fa_market_history_cache_db_sig;
extern KboFaMarketFileSignature g_kbo_fa_market_history_cache_wal_sig;
extern KboFaMarketFileSignature g_kbo_fa_market_history_cache_shm_sig;

KboFaMarketSqliteApi* kbo_fa_market_get_sqlite_api(void);
uint32_t kbo_fa_market_get_player_original_team_id(uint8_t* player);
const KboFaMarketHistoryCase* kbo_find_fa_market_history_case(
    const KboFaMarketHistoryCase* histories,
    int history_count,
    uint32_t player_id);
int kbo_get_fa_market_classification_csv_path(char* out, size_t out_size);
void kbo_fa_market_text_data_source_paths(
    const char* save_path,
    char* source_db,
    size_t source_db_size,
    char* source_wal,
    size_t source_wal_size,
    char* source_shm,
    size_t source_shm_size);
int kbo_fa_market_get_file_signature(const char* path, KboFaMarketFileSignature* out);
int kbo_fa_market_file_signature_equal(
    const KboFaMarketFileSignature* lhs,
    const KboFaMarketFileSignature* rhs);
int kbo_fa_market_get_text_data_signatures(
    const char* save_path,
    KboFaMarketFileSignature* db_sig,
    KboFaMarketFileSignature* wal_sig,
    KboFaMarketFileSignature* shm_sig);
int kbo_fa_market_history_cache_matches(
    const char* save_path,
    const KboFaMarketFileSignature* db_sig,
    const KboFaMarketFileSignature* wal_sig,
    const KboFaMarketFileSignature* shm_sig);
int kbo_fa_market_copy_history_cache_for_rows(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories);
void kbo_fa_market_store_history_cache(
    const char* save_path,
    const KboFaMarketFileSignature* db_sig,
    const KboFaMarketFileSignature* wal_sig,
    const KboFaMarketFileSignature* shm_sig,
    const KboFaMarketHistoryCase* histories,
    int history_count);
int kbo_fa_market_copy_file_if_present(const char* source, const char* destination);
int kbo_fa_market_copy_text_data_sqlite(char* out_path, size_t out_path_size);
void kbo_fa_market_copy_sqlite_text_column(
    KboFaMarketSqliteApi* api,
    KboFaMarketSqlite3Stmt* stmt,
    int column,
    char* out,
    size_t out_size);
uint32_t kbo_fa_market_resolve_league_id(uint32_t requested_league_id);
uint32_t kbo_fa_market_get_team_league_id(uint32_t team_id);
int kbo_fa_market_team_belongs_to_league(uint32_t team_id, uint32_t league_id);
int kbo_fa_market_player_has_kbo_pro_context(uint8_t* player, uint32_t league_id);
int kbo_fa_market_player_is_candidate(uint8_t* player, uint32_t league_id);

#endif
