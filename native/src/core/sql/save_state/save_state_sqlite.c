#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "save_state_sqlite.h"

#include <stdio.h>

#include "../../files/save_paths/core_save_paths.h"
#include "../../logging/core_log.h"
#include "../../product/ootp_product.h"
#include "../../sync/spin_lock.h"

#define KBO_SAVE_STATE_SQLITE_OPEN_READWRITE 0x00000002
#define KBO_SAVE_STATE_SQLITE_OPEN_CREATE 0x00000004

typedef int (__cdecl *KboSaveStateSqliteOpenV2Fn)(const char*, void**, int, const char*);
typedef int (__cdecl *KboSaveStateSqliteCloseFn)(void*);
typedef int (__cdecl *KboSaveStateSqliteExecFn)(
    void*,
    const char*,
    KboSaveStateSqliteCallback,
    void*,
    char**);
typedef void (__cdecl *KboSaveStateSqliteFreeFn)(void*);

typedef struct KboSaveStateSqliteApi {
    int attempted;
    int available;
    HMODULE module;
    KboSaveStateSqliteOpenV2Fn open_v2;
    KboSaveStateSqliteCloseFn close;
    KboSaveStateSqliteExecFn exec;
    KboSaveStateSqliteFreeFn free_fn;
} KboSaveStateSqliteApi;

static KboSaveStateSqliteApi g_kbo_save_state_sqlite_api = {0};
static KboSpinLock g_kbo_save_state_sqlite_lock = KBO_SPIN_LOCK_INIT;

static KboSaveStateSqliteOpenV2Fn kbo_save_state_sqlite_open_v2_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboSaveStateSqliteOpenV2Fn open_v2;
    } cast;
    cast.proc = proc;
    return cast.open_v2;
}

static KboSaveStateSqliteCloseFn kbo_save_state_sqlite_close_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboSaveStateSqliteCloseFn close;
    } cast;
    cast.proc = proc;
    return cast.close;
}

static KboSaveStateSqliteExecFn kbo_save_state_sqlite_exec_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboSaveStateSqliteExecFn exec;
    } cast;
    cast.proc = proc;
    return cast.exec;
}

static KboSaveStateSqliteFreeFn kbo_save_state_sqlite_free_from_proc(FARPROC proc)
{
    union {
        FARPROC proc;
        KboSaveStateSqliteFreeFn free_fn;
    } cast;
    cast.proc = proc;
    return cast.free_fn;
}

int kbo_save_state_db_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(KBO_PRODUCT_SAVE_STATE_SQLITE_FILE, out, out_size);
}

static KboSaveStateSqliteApi* kbo_save_state_sqlite_api(void)
{
    if (g_kbo_save_state_sqlite_api.attempted) {
        return g_kbo_save_state_sqlite_api.available ? &g_kbo_save_state_sqlite_api : NULL;
    }

    g_kbo_save_state_sqlite_api.attempted = 1;
    HMODULE module = LoadLibraryA("winsqlite3.dll");
    if (module == NULL) {
        kbo_log_runtimef(
            "KBO save-state sqlite unavailable reason=load_winsqlite3_failed gle=%lu",
            (unsigned long)GetLastError());
        return NULL;
    }

    g_kbo_save_state_sqlite_api.module = module;
    g_kbo_save_state_sqlite_api.open_v2 =
        kbo_save_state_sqlite_open_v2_from_proc(GetProcAddress(module, "sqlite3_open_v2"));
    g_kbo_save_state_sqlite_api.close =
        kbo_save_state_sqlite_close_from_proc(GetProcAddress(module, "sqlite3_close"));
    g_kbo_save_state_sqlite_api.exec =
        kbo_save_state_sqlite_exec_from_proc(GetProcAddress(module, "sqlite3_exec"));
    g_kbo_save_state_sqlite_api.free_fn =
        kbo_save_state_sqlite_free_from_proc(GetProcAddress(module, "sqlite3_free"));
    g_kbo_save_state_sqlite_api.available =
        g_kbo_save_state_sqlite_api.open_v2 != NULL
        && g_kbo_save_state_sqlite_api.close != NULL
        && g_kbo_save_state_sqlite_api.exec != NULL
        && g_kbo_save_state_sqlite_api.free_fn != NULL;
    if (!g_kbo_save_state_sqlite_api.available) {
        kbo_log_runtime_line("KBO save-state sqlite unavailable reason=missing_winsqlite3_exports");
        return NULL;
    }

    return &g_kbo_save_state_sqlite_api;
}

static void kbo_save_state_sqlite_free_error(KboSaveStateSqliteApi* api, char* err)
{
    if (api != NULL && api->free_fn != NULL && err != NULL) {
        api->free_fn(err);
    }
}

static int kbo_save_state_sqlite_exec_internal(
    KboSaveStateSqliteApi* api,
    void* db,
    const char* sql,
    KboSaveStateSqliteCallback callback,
    void* callback_arg,
    const char* source)
{
    char* err = NULL;
    int rc = api->exec(db, sql, callback, callback_arg, &err);
    if (rc != 0) {
        kbo_log_runtimef(
            "KBO save-state sqlite exec failed source=%s rc=%d err=%s",
            source != NULL ? source : "",
            rc,
            err != NULL ? err : "");
        kbo_save_state_sqlite_free_error(api, err);
        return 0;
    }
    kbo_save_state_sqlite_free_error(api, err);
    return 1;
}

static int kbo_save_state_sqlite_init(KboSaveStateSqliteApi* api, void* db, const char* source)
{
    static const char* sql =
        "PRAGMA busy_timeout=5000;"
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE IF NOT EXISTS kbo_schema ("
        "schema_key TEXT PRIMARY KEY,"
        "schema_version INTEGER NOT NULL,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('save_state', 1, datetime('now'));";
    return kbo_save_state_sqlite_exec_internal(api, db, sql, NULL, NULL, source);
}

static int kbo_save_state_sqlite_open(void** out_db, KboSaveStateSqliteApi** out_api, const char* source)
{
    if (out_db == NULL || out_api == NULL) {
        return 0;
    }
    *out_db = NULL;
    *out_api = NULL;

    KboSaveStateSqliteApi* api = kbo_save_state_sqlite_api();
    if (api == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_save_state_db_path(path, sizeof(path))) {
        kbo_log_runtimef(
            "KBO save-state sqlite skipped source=%s reason=path_unavailable",
            source != NULL ? source : "");
        return 0;
    }

    void* db = NULL;
    int rc = api->open_v2(
        path,
        &db,
        KBO_SAVE_STATE_SQLITE_OPEN_READWRITE | KBO_SAVE_STATE_SQLITE_OPEN_CREATE,
        NULL);
    if (rc != 0 || db == NULL) {
        kbo_log_runtimef(
            "KBO save-state sqlite open failed source=%s rc=%d path=%s",
            source != NULL ? source : "",
            rc,
            path);
        if (db != NULL) {
            api->close(db);
        }
        return 0;
    }

    if (!kbo_save_state_sqlite_init(api, db, source)) {
        api->close(db);
        return 0;
    }

    *out_db = db;
    *out_api = api;
    return 1;
}

static int kbo_save_state_sqlite_run(
    const char* sql,
    KboSaveStateSqliteCallback callback,
    void* callback_arg,
    const char* source)
{
    if (sql == NULL || sql[0] == '\0') {
        return 0;
    }

    kbo_spin_lock(&g_kbo_save_state_sqlite_lock);

    void* db = NULL;
    KboSaveStateSqliteApi* api = NULL;
    int ok = kbo_save_state_sqlite_open(&db, &api, source);
    if (ok) {
        ok = kbo_save_state_sqlite_exec_internal(api, db, sql, callback, callback_arg, source);
        api->close(db);
    }

    kbo_spin_unlock(&g_kbo_save_state_sqlite_lock);
    return ok;
}

int kbo_save_state_exec(const char* sql, const char* source)
{
    return kbo_save_state_sqlite_run(sql, NULL, NULL, source);
}

int kbo_save_state_query(
    const char* sql,
    KboSaveStateSqliteCallback callback,
    void* callback_arg,
    const char* source)
{
    return kbo_save_state_sqlite_run(sql, callback, callback_arg, source);
}
