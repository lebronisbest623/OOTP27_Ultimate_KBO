#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core_sql_league_news.h"
#include "core_sql_league_news_sqlite.h"

#include <stdio.h>

#include "../../logging/core_log.h"
#include "../text_data/core_sql_text_data_paths.h"

typedef int (__cdecl *KboLeagueNewsSqliteOpenV2Fn)(const char*, void**, int, const char*);
typedef int (__cdecl *KboLeagueNewsSqliteCloseFn)(void*);
typedef void (__cdecl *KboLeagueNewsSqliteFreeFn)(void*);

#define KBO_LEAGUE_NEWS_SQLITE_OPEN_READWRITE 0x00000002

typedef struct KboLeagueNewsSqliteFileApi {
    int attempted;
    int available;
    HMODULE module;
    KboLeagueNewsSqliteOpenV2Fn open_v2;
    KboLeagueNewsSqliteCloseFn close;
    KboSqlite3ExecFn exec;
    KboLeagueNewsSqliteFreeFn free_fn;
} KboLeagueNewsSqliteFileApi;

static KboLeagueNewsSqliteFileApi g_kbo_league_news_sqlite_file_api = {0};

static KboLeagueNewsSqliteOpenV2Fn kbo_league_news_sqlite_open_v2_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboLeagueNewsSqliteOpenV2Fn open_v2;
    } cast;
    cast.proc = proc;
    return cast.open_v2;
}

static KboLeagueNewsSqliteCloseFn kbo_league_news_sqlite_close_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboLeagueNewsSqliteCloseFn close;
    } cast;
    cast.proc = proc;
    return cast.close;
}

static KboLeagueNewsSqliteFreeFn kbo_league_news_sqlite_free_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboLeagueNewsSqliteFreeFn free_fn;
    } cast;
    cast.proc = proc;
    return cast.free_fn;
}

static KboSqlite3ExecFn kbo_sqlite3_exec_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboSqlite3ExecFn exec;
    } cast;
    cast.proc = proc;
    return cast.exec;
}

KboSqlite3ExecFn kbo_get_sqlite3_exec_fn(void)
{
    static KboSqlite3ExecFn cached_exec = NULL;
    if (cached_exec != NULL) {
        return cached_exec;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe != NULL) {
        FARPROC proc = GetProcAddress(exe, "sqlite3_exec");
        if (proc != NULL) {
            cached_exec = kbo_sqlite3_exec_from_proc(proc);
            return cached_exec;
        }
    }

    const char* module_names[] = {
        "sqlite3.dll",
        "SQLite3.dll",
        "winsqlite3.dll",
        "WinSQLite3.dll",
    };
    for (int i = 0; i < (int)(sizeof(module_names) / sizeof(module_names[0])); i++) {
        HMODULE module = GetModuleHandleA(module_names[i]);
        if (module == NULL) {
            continue;
        }
        FARPROC proc = GetProcAddress(module, "sqlite3_exec");
        if (proc != NULL) {
            cached_exec = kbo_sqlite3_exec_from_proc(proc);
            return cached_exec;
        }
    }

    return cached_exec;
}

int kbo_sqlite_exec_direct(void* database, const char* sql)
{
    KboSqlite3ExecFn sqlite_exec = kbo_get_sqlite3_exec_fn();
    if (sqlite_exec == NULL || database == NULL || sql == NULL) {
        return 0;
    }
    return sqlite_exec(database, sql, NULL, NULL, NULL) == 0 ? 1 : 0;
}

static KboLeagueNewsSqliteFileApi* kbo_league_news_get_sqlite_file_api(void)
{
    if (g_kbo_league_news_sqlite_file_api.attempted) {
        return g_kbo_league_news_sqlite_file_api.available
            ? &g_kbo_league_news_sqlite_file_api
            : NULL;
    }

    g_kbo_league_news_sqlite_file_api.attempted = 1;
    HMODULE module = LoadLibraryA("winsqlite3.dll");
    if (module == NULL) {
        kbo_log_runtimef(
            "league_news text_data fallback unavailable reason=load_winsqlite3_failed gle=%lu",
            GetLastError());
        return NULL;
    }

    g_kbo_league_news_sqlite_file_api.module = module;
    g_kbo_league_news_sqlite_file_api.open_v2 =
        kbo_league_news_sqlite_open_v2_from_proc(GetProcAddress(module, "sqlite3_open_v2"));
    g_kbo_league_news_sqlite_file_api.close =
        kbo_league_news_sqlite_close_from_proc(GetProcAddress(module, "sqlite3_close"));
    g_kbo_league_news_sqlite_file_api.exec =
        kbo_sqlite3_exec_from_proc(GetProcAddress(module, "sqlite3_exec"));
    g_kbo_league_news_sqlite_file_api.free_fn =
        kbo_league_news_sqlite_free_from_proc(GetProcAddress(module, "sqlite3_free"));
    g_kbo_league_news_sqlite_file_api.available =
        g_kbo_league_news_sqlite_file_api.open_v2 != NULL
        && g_kbo_league_news_sqlite_file_api.close != NULL
        && g_kbo_league_news_sqlite_file_api.exec != NULL
        && g_kbo_league_news_sqlite_file_api.free_fn != NULL;
    if (!g_kbo_league_news_sqlite_file_api.available) {
        kbo_log_runtime_line("league_news text_data fallback unavailable reason=missing_winsqlite3_exports");
        return NULL;
    }
    return &g_kbo_league_news_sqlite_file_api;
}

static int kbo_league_news_text_data_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    return kbo_core_sql_current_text_data_path(out, out_size);
}

static void kbo_league_news_sqlite_free_error(
    KboLeagueNewsSqliteFileApi* api,
    char* err)
{
    if (api != NULL && api->free_fn != NULL && err != NULL) {
        api->free_fn(err);
    }
}

int kbo_insert_league_news_table_text_data_fallback(
    const char* create_sql,
    const char* delete_sql,
    const char* insert_sql,
    const char* source,
    const char* title,
    const char* news_date,
    uint32_t league_id)
{
    if (create_sql == NULL || delete_sql == NULL || insert_sql == NULL) {
        return 0;
    }

    KboLeagueNewsSqliteFileApi* api = kbo_league_news_get_sqlite_file_api();
    if (api == NULL) {
        return 0;
    }

    char db_path[MAX_PATH] = {0};
    if (!kbo_league_news_text_data_path(db_path, sizeof(db_path))) {
        kbo_log_runtimef(
            "league_news text_data fallback skipped source=%s title=%s date=%s league_id=%u reason=save_path_unavailable",
            source != NULL ? source : "",
            title != NULL ? title : "",
            news_date != NULL ? news_date : "",
            league_id);
        return 0;
    }

    void* database = NULL;
    int open_rc = api->open_v2(
        db_path,
        &database,
        KBO_LEAGUE_NEWS_SQLITE_OPEN_READWRITE,
        NULL);
    if (open_rc != 0 || database == NULL) {
        kbo_log_runtimef(
            "league_news text_data fallback skipped source=%s title=%s date=%s league_id=%u reason=open_failed rc=%d db=%p path=%s",
            source != NULL ? source : "",
            title != NULL ? title : "",
            news_date != NULL ? news_date : "",
            league_id,
            open_rc,
            database,
            db_path);
        if (database != NULL) {
            api->close(database);
        }
        return 0;
    }

    char* create_err = NULL;
    char* delete_err = NULL;
    char* insert_err = NULL;
    int create_rc = api->exec(database, create_sql, NULL, NULL, &create_err);
    int delete_rc = api->exec(database, delete_sql, NULL, NULL, &delete_err);
    int insert_rc = api->exec(database, insert_sql, NULL, NULL, &insert_err);
    int inserted = insert_rc == 0;
    kbo_log_runtimef(
        "league_news text_data fallback insert source=%s title=%s date=%s league_id=%u create_rc=%d delete_rc=%d insert_rc=%d inserted=%d create_err=%s delete_err=%s insert_err=%s path=%s",
        source != NULL ? source : "",
        title != NULL ? title : "",
        news_date != NULL ? news_date : "",
        league_id,
        create_rc,
        delete_rc,
        insert_rc,
        inserted,
        create_err != NULL ? create_err : "",
        delete_err != NULL ? delete_err : "",
        insert_err != NULL ? insert_err : "",
        db_path);
    kbo_league_news_sqlite_free_error(api, create_err);
    kbo_league_news_sqlite_free_error(api, delete_err);
    kbo_league_news_sqlite_free_error(api, insert_err);
    api->close(database);
    return inserted;
}
