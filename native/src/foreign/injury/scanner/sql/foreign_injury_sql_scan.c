#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../foreign_injury_scanner_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"
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
#define KBO_FOREIGN_INJURY_SQL_SNAPSHOT_LOG_LIMIT 80
#define KBO_FOREIGN_INJURY_SQL_QUERY_MAX 8192u

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

static int kbo_foreign_injury_text_data_snapshot_path(
    const char* live_db_path,
    char* out_path,
    size_t out_path_size)
{
    if (live_db_path == NULL || live_db_path[0] == '\0'
            || out_path == NULL || out_path_size == 0u) {
        return 0;
    }

    char base[MAX_PATH] = {0};
    DWORD base_len = GetEnvironmentVariableA("LOCALAPPDATA", base, (DWORD)sizeof(base));
    if (base_len == 0u || base_len >= sizeof(base)) {
        base_len = GetTempPathA((DWORD)sizeof(base), base);
        if (base_len == 0u || base_len >= sizeof(base)) {
            return 0;
        }
    }

    char root[1024] = {0};
    int root_written = snprintf(root, sizeof(root), "%s\\OOTP-KBO", base);
    if (root_written <= 0 || (size_t)root_written >= sizeof(root)) {
        return 0;
    }
    CreateDirectoryA(root, NULL);

    char dir[1024] = {0};
    int dir_written = snprintf(dir, sizeof(dir), "%s\\sqlite_snapshots", root);
    if (dir_written <= 0 || (size_t)dir_written >= sizeof(dir)) {
        return 0;
    }
    CreateDirectoryA(dir, NULL);

    int path_written = snprintf(
        out_path,
        out_path_size,
        "%s\\foreign_injury_text_data_%lu_%lu.sqlite3",
        dir,
        GetCurrentProcessId(),
        GetCurrentThreadId());
    return path_written > 0 && (size_t)path_written < out_path_size;
}

static void kbo_foreign_injury_text_data_copy_sidecar(
    const char* live_db_path,
    const char* snapshot_db_path,
    const char* suffix)
{
    if (live_db_path == NULL || snapshot_db_path == NULL || suffix == NULL) {
        return;
    }

    char src[1200] = {0};
    char dst[1200] = {0};
    int src_written = snprintf(src, sizeof(src), "%s%s", live_db_path, suffix);
    int dst_written = snprintf(dst, sizeof(dst), "%s%s", snapshot_db_path, suffix);
    if (src_written <= 0 || dst_written <= 0
            || (size_t)src_written >= sizeof(src)
            || (size_t)dst_written >= sizeof(dst)) {
        return;
    }

    if (GetFileAttributesA(src) == INVALID_FILE_ATTRIBUTES
            || !CopyFileA(src, dst, FALSE)) {
        DeleteFileA(dst);
    }
}

static int kbo_foreign_injury_text_data_make_snapshot(
    const char* live_db_path,
    char* out_snapshot_path,
    size_t out_snapshot_path_size)
{
    if (!kbo_foreign_injury_text_data_snapshot_path(
            live_db_path,
            out_snapshot_path,
            out_snapshot_path_size)) {
        return 0;
    }

    if (!CopyFileA(live_db_path, out_snapshot_path, FALSE)) {
        static volatile LONG copy_fail_log_count = 0;
        LONG slot = InterlockedIncrement(&copy_fail_log_count);
        if (slot <= KBO_FOREIGN_INJURY_SQL_SNAPSHOT_LOG_LIMIT || (slot % 250) == 0) {
            kbo_log_runtimef(
                "foreign injury replacement: text_data sqlite snapshot copy failed gle=%lu path=%s",
                GetLastError(),
                live_db_path);
        }
        return 0;
    }

    kbo_foreign_injury_text_data_copy_sidecar(live_db_path, out_snapshot_path, "-wal");
    kbo_foreign_injury_text_data_copy_sidecar(live_db_path, out_snapshot_path, "-shm");
    return 1;
}

static int kbo_foreign_injury_exec_text_data_sqlite_file(
    KboForeignInjurySqliteFileApi* api,
    const char* db_path,
    const char* sql,
    void* callback,
    void* callback_arg,
    const char* context)
{
    if (api == NULL || db_path == NULL || sql == NULL) {
        return -1;
    }

    KboForeignInjurySqliteDb* db = NULL;
    int open_result = api->open_v2(db_path, &db, KBO_SQLITE_OPEN_READONLY, NULL);
    if (open_result != 0 || db == NULL) {
        kbo_log_runtimef(
            "foreign injury replacement: text_data sqlite %s open failed result=%d path=%s",
            context != NULL ? context : "query",
            open_result,
            db_path);
        return open_result != 0 ? open_result : -1;
    }

    int result = api->exec(db, sql, callback, callback_arg, NULL);
    api->close(db);
    return result;
}

static void kbo_foreign_injury_sql_cache_unlock(void)
{
    kbo_lock_leave(&g_kbo_foreign_injury_sql_evidence_cache_lock);
}

void kbo_foreign_injury_sql_cache_invalidate_all(const char* source)
{
    kbo_foreign_injury_sql_cache_lock();
    memset(g_kbo_foreign_injury_sql_evidence_cache, 0, sizeof(g_kbo_foreign_injury_sql_evidence_cache));
    memset(g_kbo_foreign_injury_sql_daily_cache, 0, sizeof(g_kbo_foreign_injury_sql_daily_cache));
    memset(g_kbo_foreign_injury_sql_daily_scanned, 0, sizeof(g_kbo_foreign_injury_sql_daily_scanned));
    kbo_foreign_injury_sql_cache_unlock();

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 80 || (slot % 250) == 0) {
        kbo_log_runtimef(
            "foreign injury replacement: sql evidence cache invalidated source=%s",
            source != NULL ? source : "");
    }
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
    if (evidence_date == 0u) {
        evidence_date = scan->game_date_yyyymmdd;
    }
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
    const char* news_filter =
        "(%s LIKE '%%injur%%' OR %s LIKE '%%diagnos%%' OR %s LIKE '%%out of action%%' OR %s LIKE '%%miss%%' "
        "OR %s LIKE '%%sidelined%%' OR %s LIKE '%%doctor%%' OR %s LIKE '%%medical%%' "
        "OR %s LIKE '%%disabled list%%' OR %s LIKE '%%injured list%%')";
    char league_news_filter[768] = {0};
    char team_news_filter[768] = {0};
    snprintf(
        league_news_filter,
        sizeof(league_news_filter),
        news_filter,
        "news_text",
        "news_text",
        "news_text",
        "news_text",
        "news_text",
        "news_text",
        "news_text",
        "news_text",
        "news_text");
    snprintf(
        team_news_filter,
        sizeof(team_news_filter),
        news_filter,
        "news_text",
        "news_text",
        "news_text",
        "news_text",
        "news_text",
        "news_text",
        "news_text",
        "news_text",
        "news_text");

    snprintf(
        out,
        out_size,
        "SELECT history_text, history_date FROM player_history WHERE player_id=%u AND (%u=0 OR history_date <= '%08u') "
        "UNION ALL SELECT injury_text, injury_date FROM league_injuries WHERE injury_text LIKE '%s' AND (%u=0 OR injury_date <= '%08u') "
        "UNION ALL SELECT injury_text, injury_date FROM team_injuries WHERE injury_text LIKE '%s' AND (%u=0 OR injury_date <= '%08u') "
        "UNION ALL SELECT news_text, news_date FROM league_news WHERE news_text LIKE '%s' AND (%u=0 OR news_date <= '%08u') AND %s "
        "UNION ALL SELECT news_text, news_date FROM team_news WHERE news_text LIKE '%s' AND (%u=0 OR news_date <= '%08u') AND %s;",
        player_id,
        max_date_yyyymmdd,
        max_date_yyyymmdd,
        player_href,
        max_date_yyyymmdd,
        max_date_yyyymmdd,
        player_href,
        max_date_yyyymmdd,
        max_date_yyyymmdd,
        player_href,
        max_date_yyyymmdd,
        max_date_yyyymmdd,
        league_news_filter,
        player_href,
        max_date_yyyymmdd,
        max_date_yyyymmdd,
        team_news_filter);
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

    int result = kbo_foreign_injury_exec_text_data_sqlite_file(
        api,
        db_path,
        sql,
        (void*)&kbo_foreign_injury_sql_scan_callback,
        out_scan,
        "query");
    if (result != 0) {
        char snapshot_path[1024] = {0};
        if (kbo_foreign_injury_text_data_make_snapshot(
                db_path,
                snapshot_path,
                sizeof(snapshot_path))) {
            memset(out_scan, 0, sizeof(*out_scan));
            out_scan->min_days = min_days;
            int snapshot_result = kbo_foreign_injury_exec_text_data_sqlite_file(
                api,
                snapshot_path,
                sql,
                (void*)&kbo_foreign_injury_sql_scan_callback,
                out_scan,
                "snapshot_query");
            if (snapshot_result == 0) {
                static volatile LONG snapshot_log_count = 0;
                LONG slot = InterlockedIncrement(&snapshot_log_count);
                if (slot <= KBO_FOREIGN_INJURY_SQL_SNAPSHOT_LOG_LIMIT || (slot % 250) == 0) {
                    kbo_log_runtimef(
                        "foreign injury replacement: text_data sqlite snapshot query used result=%d rows=%d found=%d path=%s",
                        result,
                        out_scan->rows_seen,
                        out_scan->found,
                        snapshot_path);
                }
            }
            result = snapshot_result;
        }
    }
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

    int result = kbo_foreign_injury_exec_text_data_sqlite_file(
        api,
        db_path,
        sql,
        (void*)&kbo_foreign_injury_sql_daily_scan_callback,
        out_scan,
        "daily");
    if (result != 0) {
        char snapshot_path[1024] = {0};
        if (kbo_foreign_injury_text_data_make_snapshot(
                db_path,
                snapshot_path,
                sizeof(snapshot_path))) {
            memset(out_scan, 0, sizeof(*out_scan));
            out_scan->database = cache_database;
            out_scan->min_days = min_days;
            out_scan->game_date_yyyymmdd = game_date_yyyymmdd;
            int snapshot_result = kbo_foreign_injury_exec_text_data_sqlite_file(
                api,
                snapshot_path,
                sql,
                (void*)&kbo_foreign_injury_sql_daily_scan_callback,
                out_scan,
                "snapshot_daily");
            if (snapshot_result == 0) {
                static volatile LONG snapshot_log_count = 0;
                LONG slot = InterlockedIncrement(&snapshot_log_count);
                if (slot <= KBO_FOREIGN_INJURY_SQL_SNAPSHOT_LOG_LIMIT || (slot % 250) == 0) {
                    kbo_log_runtimef(
                        "foreign injury replacement: text_data sqlite snapshot daily used date=%u result=%d rows=%d found=%d path=%s",
                        game_date_yyyymmdd,
                        result,
                        out_scan->rows_seen,
                        out_scan->found_count,
                        snapshot_path);
                }
            }
            result = snapshot_result;
        }
    }
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
    int written = snprintf(
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
            "WHERE injury_date <= '%08u' AND injury_text LIKE '%%player_%%.html%%' "
            "UNION ALL SELECT CAST(substr(news_text, instr(news_text, 'player_') + 7, "
            "instr(substr(news_text, instr(news_text, 'player_') + 7), '.html') - 1) AS INTEGER), "
            "news_text, news_date FROM league_news "
            "WHERE news_date <= '%08u' AND news_text LIKE '%%player_%%.html%%' "
            "AND (news_text LIKE '%%injur%%' OR news_text LIKE '%%diagnos%%' OR news_text LIKE '%%out of action%%' OR news_text LIKE '%%miss%%' "
            "OR news_text LIKE '%%sidelined%%' OR news_text LIKE '%%doctor%%' OR news_text LIKE '%%medical%%' "
            "OR news_text LIKE '%%disabled list%%' OR news_text LIKE '%%injured list%%') "
            "UNION ALL SELECT CAST(substr(news_text, instr(news_text, 'player_') + 7, "
            "instr(substr(news_text, instr(news_text, 'player_') + 7), '.html') - 1) AS INTEGER), "
            "news_text, news_date FROM team_news "
            "WHERE news_date <= '%08u' AND news_text LIKE '%%player_%%.html%%' "
            "AND (news_text LIKE '%%injur%%' OR news_text LIKE '%%diagnos%%' OR news_text LIKE '%%out of action%%' OR news_text LIKE '%%miss%%' "
            "OR news_text LIKE '%%sidelined%%' OR news_text LIKE '%%doctor%%' OR news_text LIKE '%%medical%%' "
            "OR news_text LIKE '%%disabled list%%' OR news_text LIKE '%%injured list%%')",
            game_date_yyyymmdd,
            game_date_yyyymmdd,
            game_date_yyyymmdd,
            game_date_yyyymmdd,
            game_date_yyyymmdd);
    if (written < 0 || (size_t)written >= out_size) {
        return;
    }
    size_t len = strlen(out);
    if (len + 1u < out_size) {
        out[len] = ';';
        out[len + 1u] = '\0';
    }
}

static void kbo_foreign_injury_sql_build_discovery_query(
    uint32_t game_date_yyyymmdd,
    int allow_backdated,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    const char* op = allow_backdated ? "<=" : "=";
    int written = snprintf(
            out,
            out_size,
            "SELECT player_id, history_text, history_date FROM player_history "
            "WHERE history_date %s '%08u' AND history_text LIKE '[I]Injured%%' "
            "UNION ALL SELECT CAST(substr(injury_text, instr(injury_text, 'player_') + 7, "
            "instr(substr(injury_text, instr(injury_text, 'player_') + 7), '.html') - 1) AS INTEGER), "
            "injury_text, injury_date FROM league_injuries "
            "WHERE injury_date %s '%08u' AND injury_text LIKE '%%player_%%.html%%' "
            "UNION ALL SELECT CAST(substr(injury_text, instr(injury_text, 'player_') + 7, "
            "instr(substr(injury_text, instr(injury_text, 'player_') + 7), '.html') - 1) AS INTEGER), "
            "injury_text, injury_date FROM team_injuries "
            "WHERE injury_date %s '%08u' AND injury_text LIKE '%%player_%%.html%%' "
            "UNION ALL SELECT CAST(substr(news_text, instr(news_text, 'player_') + 7, "
            "instr(substr(news_text, instr(news_text, 'player_') + 7), '.html') - 1) AS INTEGER), "
            "news_text, news_date FROM league_news "
            "WHERE news_date %s '%08u' AND news_text LIKE '%%player_%%.html%%' "
            "AND (news_text LIKE '%%injur%%' OR news_text LIKE '%%diagnos%%' OR news_text LIKE '%%out of action%%' OR news_text LIKE '%%miss%%' "
            "OR news_text LIKE '%%sidelined%%' OR news_text LIKE '%%doctor%%' OR news_text LIKE '%%medical%%' "
            "OR news_text LIKE '%%disabled list%%' OR news_text LIKE '%%injured list%%') "
            "UNION ALL SELECT CAST(substr(news_text, instr(news_text, 'player_') + 7, "
            "instr(substr(news_text, instr(news_text, 'player_') + 7), '.html') - 1) AS INTEGER), "
            "news_text, news_date FROM team_news "
            "WHERE news_date %s '%08u' AND news_text LIKE '%%player_%%.html%%' "
            "AND (news_text LIKE '%%injur%%' OR news_text LIKE '%%diagnos%%' OR news_text LIKE '%%out of action%%' OR news_text LIKE '%%miss%%' "
            "OR news_text LIKE '%%sidelined%%' OR news_text LIKE '%%doctor%%' OR news_text LIKE '%%medical%%' "
            "OR news_text LIKE '%%disabled list%%' OR news_text LIKE '%%injured list%%')",
            op,
            game_date_yyyymmdd,
            op,
            game_date_yyyymmdd,
            op,
            game_date_yyyymmdd,
            op,
            game_date_yyyymmdd,
            op,
            game_date_yyyymmdd);
    if (written < 0 || (size_t)written >= out_size) {
        return;
    }
    size_t len = strlen(out);
    if (len + 1u < out_size) {
        out[len] = ';';
        out[len + 1u] = '\0';
    }
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

    int result = kbo_foreign_injury_exec_text_data_sqlite_file(
        api,
        db_path,
        sql,
        (void*)&kbo_foreign_injury_sql_discovery_scan_callback,
        out_scan,
        "discovery");
    if (result != 0) {
        char snapshot_path[1024] = {0};
        if (kbo_foreign_injury_text_data_make_snapshot(
                db_path,
                snapshot_path,
                sizeof(snapshot_path))) {
            memset(out_scan, 0, sizeof(*out_scan));
            out_scan->min_days = min_days;
            out_scan->game_date_yyyymmdd = game_date_yyyymmdd;
            out_scan->rows = out_rows;
            out_scan->max_rows = max_rows;
            int snapshot_result = kbo_foreign_injury_exec_text_data_sqlite_file(
                api,
                snapshot_path,
                sql,
                (void*)&kbo_foreign_injury_sql_discovery_scan_callback,
                out_scan,
                "snapshot_discovery");
            if (snapshot_result == 0) {
                static volatile LONG snapshot_log_count = 0;
                LONG slot = InterlockedIncrement(&snapshot_log_count);
                if (slot <= KBO_FOREIGN_INJURY_SQL_SNAPSHOT_LOG_LIMIT || (slot % 250) == 0) {
                    kbo_log_runtimef(
                        "foreign injury replacement: text_data sqlite snapshot discovery used date=%u result=%d rows=%d found=%d path=%s",
                        game_date_yyyymmdd,
                        result,
                        out_scan->rows_seen,
                        out_scan->found_count,
                        snapshot_path);
                }
            }
            result = snapshot_result;
        }
    }
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

    char sql_live[KBO_FOREIGN_INJURY_SQL_QUERY_MAX] = {0};
    char sql_file[KBO_FOREIGN_INJURY_SQL_QUERY_MAX] = {0};
    kbo_foreign_injury_sql_build_daily_query(game_date_yyyymmdd, sql_live, sizeof(sql_live));
    kbo_foreign_injury_sql_build_daily_query(game_date_yyyymmdd, sql_file, sizeof(sql_file));
    KboForeignInjurySqlDailyScan scan;
    memset(&scan, 0, sizeof(scan));
    scan.database = cache_database;
    scan.min_days = min_days;
    scan.game_date_yyyymmdd = game_date_yyyymmdd;
    int result = -1;
    if (database != 0u && sqlite_exec != NULL) {
        result = sqlite_exec(
            (void*)database,
            sql_live,
            (void*)&kbo_foreign_injury_sql_daily_scan_callback,
            &scan,
            NULL);
    }
    KboForeignInjurySqlDailyScan file_scan;
    int file_result = kbo_foreign_injury_scan_text_data_sqlite_daily(
        sql_file,
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

int kbo_foreign_injury_collect_sql_long_term_injuries_on_date_mode(
    uint32_t game_date_yyyymmdd,
    int min_days,
    int allow_backdated,
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

    char sql_live[KBO_FOREIGN_INJURY_SQL_QUERY_MAX] = {0};
    char sql_file[KBO_FOREIGN_INJURY_SQL_QUERY_MAX] = {0};
    kbo_foreign_injury_sql_build_discovery_query(
        game_date_yyyymmdd,
        allow_backdated,
        sql_live,
        sizeof(sql_live));
    kbo_foreign_injury_sql_build_discovery_query(
        game_date_yyyymmdd,
        allow_backdated,
        sql_file,
        sizeof(sql_file));

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
            sql_live,
            (void*)&kbo_foreign_injury_sql_discovery_scan_callback,
            &scan,
            NULL);
    }

    KboForeignInjurySqlDiscoveryRow file_rows[KBO_FOREIGN_INJURY_SQL_DISCOVERY_MAX];
    memset(file_rows, 0, sizeof(file_rows));
    KboForeignInjurySqlDiscoveryScan file_scan;
    int file_result = kbo_foreign_injury_scan_text_data_sqlite_discovery(
        sql_file,
        min_days,
        game_date_yyyymmdd,
        file_rows,
        max_rows < KBO_FOREIGN_INJURY_SQL_DISCOVERY_MAX ? max_rows : KBO_FOREIGN_INJURY_SQL_DISCOVERY_MAX,
        &file_scan);
    if (file_result == 0) {
        if (result != 0 && scan.found_count <= 0) {
            int copy_count = file_scan.found_count;
            if (copy_count > max_rows) {
                copy_count = max_rows;
            }
            memcpy(out_rows, file_rows, (size_t)copy_count * sizeof(out_rows[0]));
            scan = file_scan;
            scan.found_count = copy_count;
            result = file_result;
        } else {
            for (int i = 0; i < file_scan.found_count && scan.found_count < max_rows; i++) {
                int duplicate = 0;
                for (int j = 0; j < scan.found_count; j++) {
                    if (out_rows[j].player_id == file_rows[i].player_id) {
                        duplicate = 1;
                        if (file_rows[i].days > out_rows[j].days) {
                            out_rows[j].days = file_rows[i].days;
                            out_rows[j].evidence_date = file_rows[i].evidence_date;
                        }
                        break;
                    }
                }
                if (!duplicate) {
                    out_rows[scan.found_count++] = file_rows[i];
                }
            }
            if (file_scan.rows_seen > scan.rows_seen) {
                scan.rows_seen = file_scan.rows_seen;
            }
            if (result != 0) {
                result = file_result;
            }
        }
    }

    if (out_count != NULL) {
        *out_count = scan.found_count;
    }
    if (out_rows_seen != NULL) {
        *out_rows_seen = scan.rows_seen;
    }
    if (scan.rows_seen > 0 || scan.found_count > 0) {
        kbo_log_runtimef(
            "foreign injury replacement: sql discovery injury scan date=%u min_days=%d rows=%d found=%d exec=%d allow_backdated=%d",
            game_date_yyyymmdd,
            min_days,
            scan.rows_seen,
            scan.found_count,
            result,
            allow_backdated);
    }
    return result;
}

int kbo_foreign_injury_collect_sql_long_term_injuries_on_date(
    uint32_t game_date_yyyymmdd,
    int min_days,
    KboForeignInjurySqlDiscoveryRow* out_rows,
    int max_rows,
    int* out_count,
    int* out_rows_seen)
{
    return kbo_foreign_injury_collect_sql_long_term_injuries_on_date_mode(
        game_date_yyyymmdd,
        min_days,
        0,
        out_rows,
        max_rows,
        out_count,
        out_rows_seen);
}

int kbo_foreign_injury_recent_sql_has_long_term_injury_date(
    uint32_t player_id,
    int min_days,
    int* out_days,
    uint32_t* out_evidence_date)
{
    uint32_t game_date_yyyymmdd = 0u;
    kbo_current_date_tick_latest_published_date(&game_date_yyyymmdd);
    return kbo_foreign_injury_recent_sql_has_long_term_injury_date_on_date(
        player_id,
        min_days,
        game_date_yyyymmdd,
        out_days,
        out_evidence_date);
}

int kbo_foreign_injury_recent_sql_has_long_term_injury_date_on_date_mode(
    uint32_t player_id,
    int min_days,
    uint32_t game_date_yyyymmdd,
    int allow_backdated,
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
        kbo_current_date_tick_latest_published_date(&game_date_yyyymmdd);
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
        if (!kbo_foreign_injury_sql_evidence_date_allowed(
                game_date_yyyymmdd,
                daily_date,
                daily_days,
                allow_backdated)) {
            return 0;
        }
        if (out_days != NULL) {
            *out_days = daily_days;
        }
        if (out_evidence_date != NULL) {
            *out_evidence_date = daily_date;
        }
        return 1;
    }
    if (!allow_backdated) {
        kbo_foreign_injury_sql_cache_lock();
        int daily_scanned = kbo_foreign_injury_sql_daily_scanned_locked(
            cache_database,
            game_date_yyyymmdd,
            min_days);
        kbo_foreign_injury_sql_cache_unlock();
        if (daily_scanned) {
            return 0;
        }
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
        if (cached < 0 && allow_backdated) {
            cached = 0;
        } else {
            if (!kbo_foreign_injury_sql_evidence_date_allowed(
                    game_date_yyyymmdd,
                    cached_date,
                    cached_days,
                    allow_backdated)) {
                return 0;
            }
            if (out_days != NULL) {
                *out_days = cached_days;
            }
            if (out_evidence_date != NULL) {
                *out_evidence_date = cached_date;
            }
            return cached > 0;
        }
    }

    char sql_live[KBO_FOREIGN_INJURY_SQL_QUERY_MAX] = {0};
    char sql_file[KBO_FOREIGN_INJURY_SQL_QUERY_MAX] = {0};
    kbo_foreign_injury_sql_build_query(
        player_id,
        game_date_yyyymmdd,
        sql_live,
        sizeof(sql_live));
    kbo_foreign_injury_sql_build_query(
        player_id,
        game_date_yyyymmdd,
        sql_file,
        sizeof(sql_file));

    KboForeignInjurySqlScan scan;
    memset(&scan, 0, sizeof(scan));
    scan.min_days = min_days;
    int result = kbo_foreign_injury_scan_sql_database(
        (void*)database,
        sqlite_exec,
        sql_live,
        min_days,
        &scan);

    if ((result != 0 || !scan.found)) {
        KboForeignInjurySqlScan file_scan;
        int file_result = kbo_foreign_injury_scan_text_data_sqlite(sql_file, min_days, &file_scan);
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

    int found = result == 0
        && scan.found
        && kbo_foreign_injury_sql_evidence_date_allowed(
            game_date_yyyymmdd,
            scan.best_date,
            scan.best_days,
            allow_backdated);
    if (scan.found && !found) {
        kbo_log_runtimef(
            "foreign injury replacement: sql long-term injury evidence ignored stale player=%u scan_date=%u evidence_date=%u days=%d",
            player_id,
            game_date_yyyymmdd,
            scan.best_date,
            scan.best_days);
    }
    if (found && out_days != NULL) {
        *out_days = scan.best_days;
    }
    if (found && out_evidence_date != NULL) {
        *out_evidence_date = scan.best_date;
    }
    if (found) {
        kbo_log_runtimef(
            "foreign injury replacement: sql long-term injury evidence player=%u rows=%d days=%d evidence_date=%u min_days=%d exec=%d",
            player_id,
            scan.rows_seen,
            scan.best_days,
            scan.best_date,
            min_days,
            result);
    }
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

int kbo_foreign_injury_recent_sql_has_long_term_injury_date_on_date(
    uint32_t player_id,
    int min_days,
    uint32_t game_date_yyyymmdd,
    int* out_days,
    uint32_t* out_evidence_date)
{
    return kbo_foreign_injury_recent_sql_has_long_term_injury_date_on_date_mode(
        player_id,
        min_days,
        game_date_yyyymmdd,
        0,
        out_days,
        out_evidence_date);
}

int kbo_foreign_injury_recent_sql_has_long_term_injury(
    uint32_t player_id,
    int min_days,
    int* out_days)
{
    return kbo_foreign_injury_recent_sql_has_long_term_injury_date(player_id, min_days, out_days, NULL);
}
