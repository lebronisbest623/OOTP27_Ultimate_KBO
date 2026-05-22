#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core_sql_text_data_exec.h"

#include <stdio.h>

#include "../../logging/core_log.h"
#include "../../sync/spin_lock.h"
#include "core_sql_text_data_paths.h"

#define KBO_TEXT_DATA_SQLITE_OPEN_READWRITE 0x00000002

typedef int (__cdecl *KboTextDataSqliteOpenV2Fn)(const char*, void**, int, const char*);
typedef int (__cdecl *KboTextDataSqliteCloseFn)(void*);
typedef int (__cdecl *KboTextDataSqliteExecFn)(void*, const char*, void*, void*, char**);
typedef void (__cdecl *KboTextDataSqliteFreeFn)(void*);

typedef struct KboTextDataSqliteApi {
    int attempted;
    int available;
    HMODULE module;
    KboTextDataSqliteOpenV2Fn open_v2;
    KboTextDataSqliteCloseFn close;
    KboTextDataSqliteExecFn exec;
    KboTextDataSqliteFreeFn free_fn;
} KboTextDataSqliteApi;

static KboTextDataSqliteApi g_kbo_text_data_sqlite_api = {0};
static KboSpinLock g_kbo_text_data_sqlite_lock = KBO_SPIN_LOCK_INIT;

static KboTextDataSqliteOpenV2Fn kbo_text_data_sqlite_open_v2_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboTextDataSqliteOpenV2Fn open_v2;
    } cast;
    cast.proc = proc;
    return cast.open_v2;
}

static KboTextDataSqliteCloseFn kbo_text_data_sqlite_close_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboTextDataSqliteCloseFn close;
    } cast;
    cast.proc = proc;
    return cast.close;
}

static KboTextDataSqliteExecFn kbo_text_data_sqlite_exec_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboTextDataSqliteExecFn exec;
    } cast;
    cast.proc = proc;
    return cast.exec;
}

static KboTextDataSqliteFreeFn kbo_text_data_sqlite_free_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboTextDataSqliteFreeFn free_fn;
    } cast;
    cast.proc = proc;
    return cast.free_fn;
}

static KboTextDataSqliteApi* kbo_text_data_sqlite_api(void)
{
    if (g_kbo_text_data_sqlite_api.attempted) {
        return g_kbo_text_data_sqlite_api.available ? &g_kbo_text_data_sqlite_api : NULL;
    }

    g_kbo_text_data_sqlite_api.attempted = 1;
    HMODULE module = LoadLibraryA("winsqlite3.dll");
    if (module == NULL) {
        kbo_log_runtimef(
            "text_data sqlite unavailable reason=load_winsqlite3_failed gle=%lu",
            GetLastError());
        return NULL;
    }

    g_kbo_text_data_sqlite_api.module = module;
    g_kbo_text_data_sqlite_api.open_v2 =
        kbo_text_data_sqlite_open_v2_from_proc(GetProcAddress(module, "sqlite3_open_v2"));
    g_kbo_text_data_sqlite_api.close =
        kbo_text_data_sqlite_close_from_proc(GetProcAddress(module, "sqlite3_close"));
    g_kbo_text_data_sqlite_api.exec =
        kbo_text_data_sqlite_exec_from_proc(GetProcAddress(module, "sqlite3_exec"));
    g_kbo_text_data_sqlite_api.free_fn =
        kbo_text_data_sqlite_free_from_proc(GetProcAddress(module, "sqlite3_free"));
    g_kbo_text_data_sqlite_api.available =
        g_kbo_text_data_sqlite_api.open_v2 != NULL
        && g_kbo_text_data_sqlite_api.close != NULL
        && g_kbo_text_data_sqlite_api.exec != NULL
        && g_kbo_text_data_sqlite_api.free_fn != NULL;
    if (!g_kbo_text_data_sqlite_api.available) {
        kbo_log_runtime_line("text_data sqlite unavailable reason=missing_winsqlite3_exports");
        return NULL;
    }
    return &g_kbo_text_data_sqlite_api;
}

static void kbo_text_data_sqlite_free_error(KboTextDataSqliteApi* api, char* err)
{
    if (api != NULL && api->free_fn != NULL && err != NULL) {
        api->free_fn(err);
    }
}

int kbo_core_sql_text_data_exec(
    const char* sql,
    const char* source,
    const char* op)
{
    if (sql == NULL || sql[0] == '\0') {
        return 0;
    }

    KboTextDataSqliteApi* api = kbo_text_data_sqlite_api();
    if (api == NULL) {
        return 0;
    }

    char db_path[MAX_PATH] = {0};
    if (!kbo_core_sql_current_text_data_path(db_path, sizeof(db_path))) {
        kbo_log_runtimef(
            "text_data sql skipped source=%s op=%s reason=save_path_unavailable",
            source != NULL ? source : "",
            op != NULL ? op : "");
        return 0;
    }

    kbo_spin_lock(&g_kbo_text_data_sqlite_lock);

    void* database = NULL;
    int open_rc = api->open_v2(
        db_path,
        &database,
        KBO_TEXT_DATA_SQLITE_OPEN_READWRITE,
        NULL);
    if (open_rc != 0 || database == NULL) {
        kbo_log_runtimef(
            "text_data sql skipped source=%s op=%s reason=open_failed rc=%d db=%p path=%s",
            source != NULL ? source : "",
            op != NULL ? op : "",
            open_rc,
            database,
            db_path);
        if (database != NULL) {
            api->close(database);
        }
        kbo_spin_unlock(&g_kbo_text_data_sqlite_lock);
        return 0;
    }

    char* busy_err = NULL;
    char* exec_err = NULL;
    int busy_rc = api->exec(database, "PRAGMA busy_timeout=5000;", NULL, NULL, &busy_err);
    int exec_rc = api->exec(database, sql, NULL, NULL, &exec_err);
    int ok = exec_rc == 0;
    kbo_log_runtimef(
        "text_data sql exec source=%s op=%s busy_rc=%d exec_rc=%d ok=%d busy_err=%s exec_err=%s path=%s",
        source != NULL ? source : "",
        op != NULL ? op : "",
        busy_rc,
        exec_rc,
        ok,
        busy_err != NULL ? busy_err : "",
        exec_err != NULL ? exec_err : "",
        db_path);
    kbo_text_data_sqlite_free_error(api, busy_err);
    kbo_text_data_sqlite_free_error(api, exec_err);
    api->close(database);
    kbo_spin_unlock(&g_kbo_text_data_sqlite_lock);
    return ok;
}
