#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../foreign_injury_scanner_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/sql/league_news/core_sql_league_news.h"
#include "../../../../runtime_memory/runtime_memory.h"

typedef struct KboForeignInjurySqlScan {
    int min_days;
    int found;
    int best_days;
    uint32_t best_date;
    int rows_seen;
} KboForeignInjurySqlScan;

typedef struct KboForeignInjurySqlDailyScan {
    uintptr_t database;
    int min_days;
    uint32_t game_date_yyyymmdd;
    int rows_seen;
    int found_count;
} KboForeignInjurySqlDailyScan;

typedef struct KboForeignInjurySqlDiscoveryScan {
    int min_days;
    uint32_t game_date_yyyymmdd;
    int rows_seen;
    int found_count;
    KboForeignInjurySqlDiscoveryRow* rows;
    int max_rows;
} KboForeignInjurySqlDiscoveryScan;

typedef struct KboForeignInjurySqliteDb KboForeignInjurySqliteDb;
typedef int (__cdecl *KboForeignInjurySqliteOpenV2Fn)(const char*, KboForeignInjurySqliteDb**, int, const char*);
typedef int (__cdecl *KboForeignInjurySqliteCloseFn)(KboForeignInjurySqliteDb*);

typedef struct KboForeignInjurySqliteFileApi {
    HMODULE module;
    int attempted;
    int available;
    KboForeignInjurySqliteOpenV2Fn open_v2;
    KboForeignInjurySqliteCloseFn close;
    KboSqlite3ExecFn exec;
} KboForeignInjurySqliteFileApi;

#define KBO_FOREIGN_INJURY_SQL_EVIDENCE_CACHE_SIZE 512
#define KBO_FOREIGN_INJURY_SQL_EVIDENCE_CACHE_TTL_MS 10000u
#define KBO_FOREIGN_INJURY_SQL_DAILY_CACHE_SIZE 2048
#define KBO_FOREIGN_INJURY_SQL_DAILY_SCANNED_SIZE 512

typedef struct KboForeignInjurySqlEvidenceCacheEntry {
    uintptr_t database;
    uint32_t player_id;
    uint32_t game_date_yyyymmdd;
    int min_days;
    DWORD tick;
    int found;
    int days;
    uint32_t evidence_date;
    uint8_t valid;
} KboForeignInjurySqlEvidenceCacheEntry;

typedef struct KboForeignInjurySqlDailyEvidenceEntry {
    uintptr_t database;
    uint32_t player_id;
    uint32_t game_date_yyyymmdd;
    int min_days;
    int days;
    uint32_t evidence_date;
    uint8_t valid;
} KboForeignInjurySqlDailyEvidenceEntry;

typedef struct KboForeignInjurySqlDailyScannedEntry {
    uintptr_t database;
    uint32_t game_date_yyyymmdd;
    int min_days;
    uint8_t valid;
} KboForeignInjurySqlDailyScannedEntry;

static KboForeignInjurySqlEvidenceCacheEntry
    g_kbo_foreign_injury_sql_evidence_cache[KBO_FOREIGN_INJURY_SQL_EVIDENCE_CACHE_SIZE];
static KboForeignInjurySqlDailyEvidenceEntry
    g_kbo_foreign_injury_sql_daily_cache[KBO_FOREIGN_INJURY_SQL_DAILY_CACHE_SIZE];
static KboForeignInjurySqlDailyScannedEntry
    g_kbo_foreign_injury_sql_daily_scanned[KBO_FOREIGN_INJURY_SQL_DAILY_SCANNED_SIZE];
static KboLock g_kbo_foreign_injury_sql_evidence_cache_lock = KBO_LOCK_INIT;
static KboForeignInjurySqliteFileApi g_kbo_foreign_injury_sqlite_file_api = {0};

#define KBO_SQLITE_OPEN_READONLY 0x00000001

static void kbo_foreign_injury_sql_cache_lock(void)
{
    kbo_lock_enter(&g_kbo_foreign_injury_sql_evidence_cache_lock);
}

static KboForeignInjurySqliteFileApi* kbo_foreign_injury_get_sqlite_file_api(void)
{
    if (g_kbo_foreign_injury_sqlite_file_api.attempted) {
        return g_kbo_foreign_injury_sqlite_file_api.available ? &g_kbo_foreign_injury_sqlite_file_api : NULL;
    }

    g_kbo_foreign_injury_sqlite_file_api.attempted = 1;
    HMODULE module = LoadLibraryA("winsqlite3.dll");
    if (module == NULL) {
        kbo_log_runtimef("foreign injury replacement: sqlite file fallback unavailable reason=load_winsqlite3_failed gle=%lu", GetLastError());
        return NULL;
    }

    g_kbo_foreign_injury_sqlite_file_api.module = module;
    g_kbo_foreign_injury_sqlite_file_api.open_v2 =
        (KboForeignInjurySqliteOpenV2Fn)GetProcAddress(module, "sqlite3_open_v2");
    g_kbo_foreign_injury_sqlite_file_api.close =
        (KboForeignInjurySqliteCloseFn)GetProcAddress(module, "sqlite3_close");
    g_kbo_foreign_injury_sqlite_file_api.exec =
        (KboSqlite3ExecFn)GetProcAddress(module, "sqlite3_exec");
    g_kbo_foreign_injury_sqlite_file_api.available =
        g_kbo_foreign_injury_sqlite_file_api.open_v2 != NULL
        && g_kbo_foreign_injury_sqlite_file_api.close != NULL
        && g_kbo_foreign_injury_sqlite_file_api.exec != NULL;
    if (!g_kbo_foreign_injury_sqlite_file_api.available) {
        kbo_log_runtime_line("foreign injury replacement: sqlite file fallback unavailable reason=missing_winsqlite3_exports");
        return NULL;
    }
    return &g_kbo_foreign_injury_sqlite_file_api;
}

static void kbo_foreign_injury_sql_cache_unlock(void)
{
    kbo_lock_leave(&g_kbo_foreign_injury_sql_evidence_cache_lock);
}

static uint32_t kbo_foreign_injury_sql_cache_slot(uint32_t player_id, int min_days)
{
    uint32_t h = player_id * 2654435761u;
    h ^= (uint32_t)min_days * 2246822519u;
    h ^= h >> 16;
    return h & (KBO_FOREIGN_INJURY_SQL_EVIDENCE_CACHE_SIZE - 1u);
}

static uint32_t kbo_foreign_injury_sql_daily_cache_slot(uint32_t player_id, int min_days)
{
    uint32_t h = player_id * 2654435761u;
    h ^= (uint32_t)min_days * 3266489917u;
    h ^= h >> 15;
    return h & (KBO_FOREIGN_INJURY_SQL_DAILY_CACHE_SIZE - 1u);
}

static uint32_t kbo_foreign_injury_sql_daily_scanned_slot(
    uintptr_t database,
    uint32_t game_date_yyyymmdd,
    int min_days)
{
    uint32_t h = (uint32_t)(database >> 4) ^ (uint32_t)database;
    h ^= game_date_yyyymmdd * 2654435761u;
    h ^= (uint32_t)min_days * 2246822519u;
    h ^= h >> 16;
    return h & (KBO_FOREIGN_INJURY_SQL_DAILY_SCANNED_SIZE - 1u);
}

static int kbo_foreign_injury_sql_daily_scanned_locked(
    uintptr_t database,
    uint32_t game_date_yyyymmdd,
    int min_days)
{
    uint32_t start = kbo_foreign_injury_sql_daily_scanned_slot(
        database,
        game_date_yyyymmdd,
        min_days);
    for (uint32_t probe = 0; probe < KBO_FOREIGN_INJURY_SQL_DAILY_SCANNED_SIZE; probe++) {
        uint32_t slot = (start + probe) & (KBO_FOREIGN_INJURY_SQL_DAILY_SCANNED_SIZE - 1u);
        KboForeignInjurySqlDailyScannedEntry* entry = &g_kbo_foreign_injury_sql_daily_scanned[slot];
        if (!entry->valid) {
            return 0;
        }
        if (entry->database == database
                && entry->game_date_yyyymmdd == game_date_yyyymmdd
                && entry->min_days == min_days) {
            return 1;
        }
    }
    return 0;
}

static void kbo_foreign_injury_sql_mark_daily_scanned_locked(
    uintptr_t database,
    uint32_t game_date_yyyymmdd,
    int min_days)
{
    uint32_t start = kbo_foreign_injury_sql_daily_scanned_slot(
        database,
        game_date_yyyymmdd,
        min_days);
    for (uint32_t probe = 0; probe < KBO_FOREIGN_INJURY_SQL_DAILY_SCANNED_SIZE; probe++) {
        uint32_t slot = (start + probe) & (KBO_FOREIGN_INJURY_SQL_DAILY_SCANNED_SIZE - 1u);
        KboForeignInjurySqlDailyScannedEntry* entry = &g_kbo_foreign_injury_sql_daily_scanned[slot];
        if (!entry->valid
                || (entry->database == database
                    && entry->game_date_yyyymmdd == game_date_yyyymmdd
                    && entry->min_days == min_days)) {
            entry->database = database;
            entry->game_date_yyyymmdd = game_date_yyyymmdd;
            entry->min_days = min_days;
            entry->valid = 1u;
            return;
        }
    }
    g_kbo_foreign_injury_sql_daily_scanned[start].database = database;
    g_kbo_foreign_injury_sql_daily_scanned[start].game_date_yyyymmdd = game_date_yyyymmdd;
    g_kbo_foreign_injury_sql_daily_scanned[start].min_days = min_days;
    g_kbo_foreign_injury_sql_daily_scanned[start].valid = 1u;
}

static int kbo_foreign_injury_sql_daily_cache_get(
    uintptr_t database,
    uint32_t player_id,
    uint32_t game_date_yyyymmdd,
    int min_days,
    int* out_days,
    uint32_t* out_evidence_date)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    if (out_evidence_date != NULL) {
        *out_evidence_date = 0u;
    }
    uint32_t start = kbo_foreign_injury_sql_daily_cache_slot(player_id, min_days);
    kbo_foreign_injury_sql_cache_lock();
    for (uint32_t probe = 0; probe < KBO_FOREIGN_INJURY_SQL_DAILY_CACHE_SIZE; probe++) {
        uint32_t slot = (start + probe) & (KBO_FOREIGN_INJURY_SQL_DAILY_CACHE_SIZE - 1u);
        KboForeignInjurySqlDailyEvidenceEntry entry = g_kbo_foreign_injury_sql_daily_cache[slot];
        if (!entry.valid) {
            break;
        }
        if (entry.database == database
                && entry.player_id == player_id
                && entry.game_date_yyyymmdd == game_date_yyyymmdd
                && entry.min_days == min_days) {
            kbo_foreign_injury_sql_cache_unlock();
            if (out_days != NULL) {
                *out_days = entry.days;
            }
            if (out_evidence_date != NULL) {
                *out_evidence_date = entry.evidence_date;
            }
            return 1;
        }
    }
    kbo_foreign_injury_sql_cache_unlock();
    return 0;
}

static void kbo_foreign_injury_sql_daily_cache_store(
    uintptr_t database,
    uint32_t player_id,
    uint32_t game_date_yyyymmdd,
    int min_days,
    int days,
    uint32_t evidence_date)
{
    if (database == 0u || player_id == 0u || game_date_yyyymmdd == 0u || min_days <= 0 || days <= 0) {
        return;
    }
    uint32_t start = kbo_foreign_injury_sql_daily_cache_slot(player_id, min_days);
    kbo_foreign_injury_sql_cache_lock();
    for (uint32_t probe = 0; probe < KBO_FOREIGN_INJURY_SQL_DAILY_CACHE_SIZE; probe++) {
        uint32_t slot = (start + probe) & (KBO_FOREIGN_INJURY_SQL_DAILY_CACHE_SIZE - 1u);
        KboForeignInjurySqlDailyEvidenceEntry* entry = &g_kbo_foreign_injury_sql_daily_cache[slot];
        if (!entry->valid
                || (entry->database == database
                    && entry->player_id == player_id
                    && entry->game_date_yyyymmdd == game_date_yyyymmdd
                    && entry->min_days == min_days)) {
            if (!entry->valid || days > entry->days || (days == entry->days && entry->evidence_date == 0u)) {
                entry->database = database;
                entry->player_id = player_id;
                entry->game_date_yyyymmdd = game_date_yyyymmdd;
                entry->min_days = min_days;
                entry->days = days;
                entry->evidence_date = evidence_date;
                entry->valid = 1u;
            }
            break;
        }
    }
    kbo_foreign_injury_sql_cache_unlock();
}

static int kbo_foreign_injury_sql_cache_get(
    uintptr_t database,
    uint32_t player_id,
    uint32_t game_date_yyyymmdd,
    int min_days,
    int* out_days,
    uint32_t* out_evidence_date)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    if (out_evidence_date != NULL) {
        *out_evidence_date = 0u;
    }

    DWORD now = GetTickCount();
    uint32_t slot = kbo_foreign_injury_sql_cache_slot(player_id, min_days);
    kbo_foreign_injury_sql_cache_lock();
    KboForeignInjurySqlEvidenceCacheEntry cached =
        g_kbo_foreign_injury_sql_evidence_cache[slot];
    kbo_foreign_injury_sql_cache_unlock();

    if (!cached.valid
            || cached.database != database
            || cached.player_id != player_id
            || cached.game_date_yyyymmdd != game_date_yyyymmdd
            || cached.min_days != min_days
            || now - cached.tick > KBO_FOREIGN_INJURY_SQL_EVIDENCE_CACHE_TTL_MS) {
        return 0;
    }

    if (out_days != NULL) {
        *out_days = cached.days;
    }
    if (out_evidence_date != NULL) {
        *out_evidence_date = cached.evidence_date;
    }
    return cached.found ? 1 : -1;
}

static void kbo_foreign_injury_sql_cache_store(
    uintptr_t database,
    uint32_t player_id,
    uint32_t game_date_yyyymmdd,
    int min_days,
    int found,
    int days,
    uint32_t evidence_date)
{
    uint32_t slot = kbo_foreign_injury_sql_cache_slot(player_id, min_days);
    kbo_foreign_injury_sql_cache_lock();
    KboForeignInjurySqlEvidenceCacheEntry* entry =
        &g_kbo_foreign_injury_sql_evidence_cache[slot];
    entry->valid = 0u;
    entry->database = database;
    entry->player_id = player_id;
    entry->game_date_yyyymmdd = game_date_yyyymmdd;
    entry->min_days = min_days;
    entry->tick = GetTickCount();
    entry->found = found ? 1 : 0;
    entry->days = days;
    entry->evidence_date = evidence_date;
    entry->valid = 1u;
    kbo_foreign_injury_sql_cache_unlock();
}

static uint32_t kbo_foreign_injury_parse_history_date(const char* text)
{
    if (text == NULL || strlen(text) != 8u) {
        return 0u;
    }
    for (int i = 0; i < 8; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0u;
        }
    }
    uint32_t value = (uint32_t)atoi(text);
    uint32_t month = (value / 100u) % 100u;
    uint32_t day = value % 100u;
    return month >= 1u && month <= 12u && day >= 1u && day <= 31u ? value : 0u;
}

static int __cdecl kbo_foreign_injury_sql_scan_callback(void* arg, int column_count, char** values, char** names)
{
    (void)names;
    KboForeignInjurySqlScan* scan = (KboForeignInjurySqlScan*)arg;
    if (scan == NULL || column_count <= 0 || values == NULL || values[0] == NULL) {
        return 0;
    }

    scan->rows_seen++;
    int evidence_days = 0;
    if (kbo_foreign_injury_duration_text_meets_minimum(values[0], scan->min_days, &evidence_days)) {
        uint32_t evidence_date = column_count > 1 && values[1] != NULL
            ? kbo_foreign_injury_parse_history_date(values[1])
            : 0u;
        scan->found = 1;
        if (evidence_days > scan->best_days) {
            scan->best_days = evidence_days;
            scan->best_date = evidence_date;
        } else if (evidence_days == scan->best_days && scan->best_date == 0u) {
            scan->best_date = evidence_date;
        }
    }
    return 0;
}

static int __cdecl kbo_foreign_injury_sql_daily_scan_callback(void* arg, int column_count, char** values, char** names)
{
    (void)names;
    KboForeignInjurySqlDailyScan* scan = (KboForeignInjurySqlDailyScan*)arg;
    if (scan == NULL || column_count < 3 || values == NULL || values[0] == NULL || values[1] == NULL) {
        return 0;
    }

    scan->rows_seen++;
    uint32_t player_id = (uint32_t)strtoul(values[0], NULL, 10);
    if (player_id == 0u) {
        return 0;
    }
    int evidence_days = 0;
    if (!kbo_foreign_injury_duration_text_meets_minimum(values[1], scan->min_days, &evidence_days)) {
        return 0;
    }
    uint32_t evidence_date = values[2] != NULL
        ? kbo_foreign_injury_parse_history_date(values[2])
        : 0u;
    kbo_foreign_injury_sql_daily_cache_store(
        scan->database,
        player_id,
        scan->game_date_yyyymmdd,
        scan->min_days,
        evidence_days,
        evidence_date);
    scan->found_count++;
    return 0;
}

static int __cdecl kbo_foreign_injury_sql_discovery_scan_callback(void* arg, int column_count, char** values, char** names)
{
    (void)names;
    KboForeignInjurySqlDiscoveryScan* scan = (KboForeignInjurySqlDiscoveryScan*)arg;
    if (scan == NULL || column_count < 3 || values == NULL || values[0] == NULL || values[1] == NULL) {
        return 0;
    }

    scan->rows_seen++;
    uint32_t player_id = (uint32_t)strtoul(values[0], NULL, 10);
    if (player_id == 0u) {
        return 0;
    }
    int evidence_days = 0;
    if (!kbo_foreign_injury_duration_text_meets_minimum(values[1], scan->min_days, &evidence_days)) {
        return 0;
    }
    uint32_t evidence_date = values[2] != NULL
        ? kbo_foreign_injury_parse_history_date(values[2])
        : 0u;
    if (evidence_date == 0u) {
        evidence_date = scan->game_date_yyyymmdd;
    }

    for (int i = 0; i < scan->found_count; i++) {
        KboForeignInjurySqlDiscoveryRow* row = &scan->rows[i];
        if (row->player_id == player_id) {
            if (evidence_days > row->days) {
                row->days = evidence_days;
                row->evidence_date = evidence_date;
            }
            return 0;
        }
    }
    if (scan->rows != NULL && scan->found_count < scan->max_rows) {
        KboForeignInjurySqlDiscoveryRow* row = &scan->rows[scan->found_count++];
        row->player_id = player_id;
        row->days = evidence_days;
        row->evidence_date = evidence_date;
    }
    return 0;
}

static void kbo_foreign_injury_sql_build_query(
    uint32_t player_id,
    uint32_t max_date_yyyymmdd,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    char player_href[64] = {0};
    snprintf(player_href, sizeof(player_href), "%%/player_%u.html%%", player_id);
    snprintf(
        out,
        out_size,
        "SELECT history_text, history_date FROM player_history WHERE player_id=%u AND (%u=0 OR history_date <= '%08u') "
        "UNION ALL SELECT injury_text, injury_date FROM league_injuries WHERE injury_text LIKE '%s' AND (%u=0 OR injury_date <= '%08u') "
        "UNION ALL SELECT injury_text, injury_date FROM team_injuries WHERE injury_text LIKE '%s' AND (%u=0 OR injury_date <= '%08u');",
        player_id,
        max_date_yyyymmdd,
        max_date_yyyymmdd,
        player_href,
        max_date_yyyymmdd,
        max_date_yyyymmdd,
        player_href,
        max_date_yyyymmdd,
        max_date_yyyymmdd);
}

static int kbo_foreign_injury_scan_sql_database(
    void* database,
    KboSqlite3ExecFn sqlite_exec,
    const char* sql,
    int min_days,
    KboForeignInjurySqlScan* out_scan)
{
    if (out_scan != NULL) {
        memset(out_scan, 0, sizeof(*out_scan));
        out_scan->min_days = min_days;
    }
    if (database == NULL || sqlite_exec == NULL || sql == NULL || out_scan == NULL || min_days <= 0) {
        return -1;
    }
    return sqlite_exec(
        database,
        sql,
        (void*)&kbo_foreign_injury_sql_scan_callback,
        out_scan,
        NULL);
}

static int kbo_foreign_injury_scan_text_data_sqlite(
    const char* sql,
    int min_days,
    KboForeignInjurySqlScan* out_scan)
{
    if (out_scan != NULL) {
        memset(out_scan, 0, sizeof(*out_scan));
        out_scan->min_days = min_days;
    }
    if (sql == NULL || min_days <= 0 || out_scan == NULL) {
        return -1;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return -1;
    }

    char db_path[MAX_PATH] = {0};
    snprintf(db_path, sizeof(db_path), "%s\\temp\\text_data.sqlite3", save_path);
    if (GetFileAttributesA(db_path) == INVALID_FILE_ATTRIBUTES) {
        return -1;
    }

    KboForeignInjurySqliteFileApi* api = kbo_foreign_injury_get_sqlite_file_api();
    if (api == NULL) {
        return -1;
    }

    KboForeignInjurySqliteDb* db = NULL;
    int open_result = api->open_v2(db_path, &db, KBO_SQLITE_OPEN_READONLY, NULL);
    if (open_result != 0 || db == NULL) {
        kbo_log_runtimef(
            "foreign injury replacement: text_data sqlite open failed result=%d path=%s",
            open_result,
            db_path);
        return open_result != 0 ? open_result : -1;
    }

    int result = kbo_foreign_injury_scan_sql_database(db, api->exec, sql, min_days, out_scan);
    api->close(db);
    return result;
}

static int kbo_foreign_injury_scan_text_data_sqlite_daily(
    const char* sql,
    uintptr_t cache_database,
    int min_days,
    uint32_t game_date_yyyymmdd,
    KboForeignInjurySqlDailyScan* out_scan)
{
    if (out_scan != NULL) {
        memset(out_scan, 0, sizeof(*out_scan));
        out_scan->database = cache_database;
        out_scan->min_days = min_days;
        out_scan->game_date_yyyymmdd = game_date_yyyymmdd;
    }
    if (sql == NULL || min_days <= 0 || game_date_yyyymmdd == 0u || out_scan == NULL) {
        return -1;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return -1;
    }

    char db_path[MAX_PATH] = {0};
    snprintf(db_path, sizeof(db_path), "%s\\temp\\text_data.sqlite3", save_path);
    if (GetFileAttributesA(db_path) == INVALID_FILE_ATTRIBUTES) {
        return -1;
    }

    KboForeignInjurySqliteFileApi* api = kbo_foreign_injury_get_sqlite_file_api();
    if (api == NULL) {
        return -1;
    }

    KboForeignInjurySqliteDb* db = NULL;
    int open_result = api->open_v2(db_path, &db, KBO_SQLITE_OPEN_READONLY, NULL);
    if (open_result != 0 || db == NULL) {
        kbo_log_runtimef(
            "foreign injury replacement: text_data sqlite daily open failed result=%d path=%s",
            open_result,
            db_path);
        return open_result != 0 ? open_result : -1;
    }

    int result = api->exec(
        db,
        sql,
        (void*)&kbo_foreign_injury_sql_daily_scan_callback,
        out_scan,
        NULL);
    api->close(db);
    return result;
}

static void kbo_foreign_injury_sql_build_daily_query(
    uint32_t game_date_yyyymmdd,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    snprintf(
        out,
        out_size,
        "SELECT player_id, history_text, history_date FROM player_history "
        "WHERE history_date <= '%08u' AND history_text LIKE '[I]Injured%%' "
        "UNION ALL SELECT CAST(substr(injury_text, instr(injury_text, 'player_') + 7, "
        "instr(substr(injury_text, instr(injury_text, 'player_') + 7), '.html') - 1) AS INTEGER), "
        "injury_text, injury_date FROM league_injuries "
        "WHERE injury_date <= '%08u' AND injury_text LIKE '%%player_%%.html%%' "
        "UNION ALL SELECT CAST(substr(injury_text, instr(injury_text, 'player_') + 7, "
        "instr(substr(injury_text, instr(injury_text, 'player_') + 7), '.html') - 1) AS INTEGER), "
        "injury_text, injury_date FROM team_injuries "
        "WHERE injury_date <= '%08u' AND injury_text LIKE '%%player_%%.html%%';",
        game_date_yyyymmdd,
        game_date_yyyymmdd,
        game_date_yyyymmdd);
}

static void kbo_foreign_injury_sql_build_discovery_query(
    uint32_t game_date_yyyymmdd,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    snprintf(
        out,
        out_size,
        "SELECT player_id, history_text, history_date FROM player_history "
        "WHERE history_date = '%08u' AND history_text LIKE '[I]Injured%%' "
        "UNION ALL SELECT CAST(substr(injury_text, instr(injury_text, 'player_') + 7, "
        "instr(substr(injury_text, instr(injury_text, 'player_') + 7), '.html') - 1) AS INTEGER), "
        "injury_text, injury_date FROM league_injuries "
        "WHERE injury_date = '%08u' AND injury_text LIKE '%%player_%%.html%%' "
        "UNION ALL SELECT CAST(substr(injury_text, instr(injury_text, 'player_') + 7, "
        "instr(substr(injury_text, instr(injury_text, 'player_') + 7), '.html') - 1) AS INTEGER), "
        "injury_text, injury_date FROM team_injuries "
        "WHERE injury_date = '%08u' AND injury_text LIKE '%%player_%%.html%%';",
        game_date_yyyymmdd,
        game_date_yyyymmdd,
        game_date_yyyymmdd);
}

static int kbo_foreign_injury_scan_text_data_sqlite_discovery(
    const char* sql,
    int min_days,
    uint32_t game_date_yyyymmdd,
    KboForeignInjurySqlDiscoveryRow* out_rows,
    int max_rows,
    KboForeignInjurySqlDiscoveryScan* out_scan)
{
    if (out_scan != NULL) {
        memset(out_scan, 0, sizeof(*out_scan));
        out_scan->min_days = min_days;
        out_scan->game_date_yyyymmdd = game_date_yyyymmdd;
        out_scan->rows = out_rows;
        out_scan->max_rows = max_rows;
    }
    if (sql == NULL || min_days <= 0 || game_date_yyyymmdd == 0u || out_scan == NULL) {
        return -1;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return -1;
    }

    char db_path[MAX_PATH] = {0};
    snprintf(db_path, sizeof(db_path), "%s\\temp\\text_data.sqlite3", save_path);
    if (GetFileAttributesA(db_path) == INVALID_FILE_ATTRIBUTES) {
        return -1;
    }

    KboForeignInjurySqliteFileApi* api = kbo_foreign_injury_get_sqlite_file_api();
    if (api == NULL) {
        return -1;
    }

    KboForeignInjurySqliteDb* db = NULL;
    int open_result = api->open_v2(db_path, &db, KBO_SQLITE_OPEN_READONLY, NULL);
    if (open_result != 0 || db == NULL) {
        kbo_log_runtimef(
            "foreign injury replacement: text_data sqlite discovery open failed result=%d path=%s",
            open_result,
            db_path);
        return open_result != 0 ? open_result : -1;
    }

    int result = api->exec(
        db,
        sql,
        (void*)&kbo_foreign_injury_sql_discovery_scan_callback,
        out_scan,
        NULL);
    api->close(db);
    return result;
}

static void kbo_foreign_injury_sql_ensure_daily_scan(
    uintptr_t database,
    KboSqlite3ExecFn sqlite_exec,
    uint32_t game_date_yyyymmdd,
    int min_days)
{
    if (game_date_yyyymmdd == 0u || min_days <= 0) {
        return;
    }
    uintptr_t cache_database = database != 0u ? database : 1u;
    kbo_foreign_injury_sql_cache_lock();
    int already_scanned = kbo_foreign_injury_sql_daily_scanned_locked(
        cache_database,
        game_date_yyyymmdd,
        min_days);
    kbo_foreign_injury_sql_cache_unlock();
    if (already_scanned) {
        return;
    }

    char sql[1600] = {0};
    kbo_foreign_injury_sql_build_daily_query(game_date_yyyymmdd, sql, sizeof(sql));
    KboForeignInjurySqlDailyScan scan;
    memset(&scan, 0, sizeof(scan));
    scan.database = cache_database;
    scan.min_days = min_days;
    scan.game_date_yyyymmdd = game_date_yyyymmdd;
    int result = -1;
    if (database != 0u && sqlite_exec != NULL) {
        result = sqlite_exec(
            (void*)database,
            sql,
            (void*)&kbo_foreign_injury_sql_daily_scan_callback,
            &scan,
            NULL);
    }
    KboForeignInjurySqlDailyScan file_scan;
    int file_result = kbo_foreign_injury_scan_text_data_sqlite_daily(
        sql,
        cache_database,
        min_days,
        game_date_yyyymmdd,
        &file_scan);
    if (file_result == 0
            && (result != 0
                || file_scan.rows_seen >= scan.rows_seen
                || file_scan.found_count > scan.found_count)) {
        scan = file_scan;
        result = file_result;
    }
    kbo_log_runtimef(
        "foreign injury replacement: sql daily injury scan date=%u min_days=%d rows=%d found=%d exec=%d",
        game_date_yyyymmdd,
        min_days,
        scan.rows_seen,
        scan.found_count,
        result);
    if (result == 0) {
        kbo_foreign_injury_sql_cache_lock();
        kbo_foreign_injury_sql_mark_daily_scanned_locked(
            cache_database,
            game_date_yyyymmdd,
            min_days);
        kbo_foreign_injury_sql_cache_unlock();
    }
}

int kbo_foreign_injury_collect_sql_long_term_injuries_on_date(
    uint32_t game_date_yyyymmdd,
    int min_days,
    KboForeignInjurySqlDiscoveryRow* out_rows,
    int max_rows,
    int* out_count,
    int* out_rows_seen)
{
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (out_rows_seen != NULL) {
        *out_rows_seen = 0;
    }
    if (out_rows != NULL && max_rows > 0) {
        memset(out_rows, 0, (size_t)max_rows * sizeof(out_rows[0]));
    }
    if (game_date_yyyymmdd == 0u || min_days <= 0 || out_rows == NULL || max_rows <= 0) {
        return -1;
    }

    char sql[1600] = {0};
    kbo_foreign_injury_sql_build_discovery_query(game_date_yyyymmdd, sql, sizeof(sql));

    uintptr_t database = 0u;
    KboSqlite3ExecFn sqlite_exec = NULL;
    uintptr_t global = get_ootp_global_database();
    if (global != 0 && memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
        sqlite_exec = kbo_get_sqlite3_exec_fn();
        if (database == 0 || !memory_range_readable((void*)database, 0x10) || sqlite_exec == NULL) {
            database = 0u;
            sqlite_exec = NULL;
        }
    }

    KboForeignInjurySqlDiscoveryScan scan;
    memset(&scan, 0, sizeof(scan));
    scan.min_days = min_days;
    scan.game_date_yyyymmdd = game_date_yyyymmdd;
    scan.rows = out_rows;
    scan.max_rows = max_rows;

    int result = -1;
    if (database != 0u && sqlite_exec != NULL) {
        result = sqlite_exec(
            (void*)database,
            sql,
            (void*)&kbo_foreign_injury_sql_discovery_scan_callback,
            &scan,
            NULL);
    }

    KboForeignInjurySqlDiscoveryRow file_rows[KBO_FOREIGN_INJURY_SQL_DISCOVERY_MAX];
    memset(file_rows, 0, sizeof(file_rows));
    KboForeignInjurySqlDiscoveryScan file_scan;
    int file_result = kbo_foreign_injury_scan_text_data_sqlite_discovery(
        sql,
        min_days,
        game_date_yyyymmdd,
        file_rows,
        max_rows < KBO_FOREIGN_INJURY_SQL_DISCOVERY_MAX ? max_rows : KBO_FOREIGN_INJURY_SQL_DISCOVERY_MAX,
        &file_scan);
    if (file_result == 0
            && (result != 0
                || file_scan.rows_seen >= scan.rows_seen
                || file_scan.found_count > scan.found_count)) {
        int copy_count = file_scan.found_count;
        if (copy_count > max_rows) {
            copy_count = max_rows;
        }
        memcpy(out_rows, file_rows, (size_t)copy_count * sizeof(out_rows[0]));
        scan = file_scan;
        scan.found_count = copy_count;
        result = file_result;
    }

    if (out_count != NULL) {
        *out_count = scan.found_count;
    }
    if (out_rows_seen != NULL) {
        *out_rows_seen = scan.rows_seen;
    }
    if (scan.rows_seen > 0 || scan.found_count > 0) {
        kbo_log_runtimef(
            "foreign injury replacement: sql discovery injury scan date=%u min_days=%d rows=%d found=%d exec=%d",
            game_date_yyyymmdd,
            min_days,
            scan.rows_seen,
            scan.found_count,
            result);
    }
    return result;
}

int kbo_foreign_injury_recent_sql_has_long_term_injury_date(
    uint32_t player_id,
    int min_days,
    int* out_days,
    uint32_t* out_evidence_date)
{
    uint32_t game_date_yyyymmdd = 0u;
    kbo_get_current_yyyymmdd(&game_date_yyyymmdd);
    return kbo_foreign_injury_recent_sql_has_long_term_injury_date_on_date(
        player_id,
        min_days,
        game_date_yyyymmdd,
        out_days,
        out_evidence_date);
}

int kbo_foreign_injury_recent_sql_has_long_term_injury_date_on_date(
    uint32_t player_id,
    int min_days,
    uint32_t game_date_yyyymmdd,
    int* out_days,
    uint32_t* out_evidence_date)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    if (out_evidence_date != NULL) {
        *out_evidence_date = 0u;
    }
    if (player_id == 0u || min_days <= 0) {
        return 0;
    }

    uintptr_t database = 0u;
    KboSqlite3ExecFn sqlite_exec = NULL;
    if (game_date_yyyymmdd == 0u) {
        kbo_get_current_yyyymmdd(&game_date_yyyymmdd);
    }
    uintptr_t global = get_ootp_global_database();
    if (global != 0 && memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
        sqlite_exec = kbo_get_sqlite3_exec_fn();
        if (database == 0 || !memory_range_readable((void*)database, 0x10) || sqlite_exec == NULL) {
            database = 0u;
            sqlite_exec = NULL;
        }
    }
    uintptr_t cache_database = database != 0u ? database : 1u;

    kbo_foreign_injury_sql_ensure_daily_scan(
        database,
        sqlite_exec,
        game_date_yyyymmdd,
        min_days);
    int daily_days = 0;
    uint32_t daily_date = 0u;
    if (kbo_foreign_injury_sql_daily_cache_get(
            cache_database,
            player_id,
            game_date_yyyymmdd,
            min_days,
            &daily_days,
            &daily_date)) {
        if (out_days != NULL) {
            *out_days = daily_days;
        }
        if (out_evidence_date != NULL) {
            *out_evidence_date = daily_date;
        }
        return 1;
    }

    int cached_days = 0;
    uint32_t cached_date = 0u;
    int cached = kbo_foreign_injury_sql_cache_get(
            cache_database,
            player_id,
            game_date_yyyymmdd,
            min_days,
            &cached_days,
            &cached_date);
    if (cached != 0) {
        if (out_days != NULL) {
            *out_days = cached_days;
        }
        if (out_evidence_date != NULL) {
            *out_evidence_date = cached_date;
        }
        return cached > 0;
    }

    char sql[1600] = {0};
    kbo_foreign_injury_sql_build_query(player_id, game_date_yyyymmdd, sql, sizeof(sql));

    KboForeignInjurySqlScan scan;
    memset(&scan, 0, sizeof(scan));
    scan.min_days = min_days;
    int result = kbo_foreign_injury_scan_sql_database(
        (void*)database,
        sqlite_exec,
        sql,
        min_days,
        &scan);

    if ((result != 0 || !scan.found)) {
        KboForeignInjurySqlScan file_scan;
        int file_result = kbo_foreign_injury_scan_text_data_sqlite(sql, min_days, &file_scan);
        if (file_result == 0 && file_scan.found) {
            scan = file_scan;
            result = file_result;
            kbo_log_runtimef(
                "foreign injury replacement: text_data sql long-term injury evidence player=%u rows=%d days=%d evidence_date=%u min_days=%d",
                player_id,
                scan.rows_seen,
                scan.best_days,
                scan.best_date,
                min_days);
        }
    }

    if (scan.found && out_days != NULL) {
        *out_days = scan.best_days;
    }
    if (scan.found && out_evidence_date != NULL) {
        *out_evidence_date = scan.best_date;
    }
    if (scan.found) {
        kbo_log_runtimef(
            "foreign injury replacement: sql long-term injury evidence player=%u rows=%d days=%d evidence_date=%u min_days=%d exec=%d",
            player_id,
            scan.rows_seen,
            scan.best_days,
            scan.best_date,
            min_days,
            result);
    }
    int found = result == 0 && scan.found;
    kbo_foreign_injury_sql_cache_store(
        cache_database,
        player_id,
        game_date_yyyymmdd,
        min_days,
        found,
        scan.best_days,
        scan.best_date);
    return found;
}

int kbo_foreign_injury_recent_sql_has_long_term_injury(
    uint32_t player_id,
    int min_days,
    int* out_days)
{
    return kbo_foreign_injury_recent_sql_has_long_term_injury_date(player_id, min_days, out_days, NULL);
}
