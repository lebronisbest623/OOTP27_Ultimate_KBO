#include "../internal/fa_market_data_internal.h"
#include "../../core/sql/text_data/core_sql_text_data_paths.h"

uint32_t kbo_fa_market_get_player_original_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
}

int kbo_get_fa_market_classification_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("cache\\fa_market_classification.csv", out, out_size);
}

void kbo_fa_market_text_data_source_paths(
    const char* save_path,
    char* source_db,
    size_t source_db_size,
    char* source_wal,
    size_t source_wal_size,
    char* source_shm,
    size_t source_shm_size)
{
    kbo_core_sql_text_data_companion_paths(
        save_path,
        source_db,
        source_db_size,
        source_wal,
        source_wal_size,
        source_shm,
        source_shm_size);
}

int kbo_fa_market_get_file_signature(const char* path, KboFaMarketFileSignature* out)
{
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA data;
    memset(&data, 0, sizeof(data));
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)
            || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        out->exists = 0;
        return 1;
    }
    out->exists = 1;
    out->size_high = data.nFileSizeHigh;
    out->size_low = data.nFileSizeLow;
    out->last_write_time = data.ftLastWriteTime;
    return 1;
}

int kbo_fa_market_file_signature_equal(
    const KboFaMarketFileSignature* lhs,
    const KboFaMarketFileSignature* rhs)
{
    if (lhs == NULL || rhs == NULL) {
        return 0;
    }
    return lhs->exists == rhs->exists
        && lhs->size_high == rhs->size_high
        && lhs->size_low == rhs->size_low
        && lhs->last_write_time.dwHighDateTime == rhs->last_write_time.dwHighDateTime
        && lhs->last_write_time.dwLowDateTime == rhs->last_write_time.dwLowDateTime;
}

int kbo_fa_market_get_text_data_signatures(
    const char* save_path,
    KboFaMarketFileSignature* db_sig,
    KboFaMarketFileSignature* wal_sig,
    KboFaMarketFileSignature* shm_sig)
{
    char source_db[MAX_PATH] = {0};
    char source_wal[MAX_PATH] = {0};
    char source_shm[MAX_PATH] = {0};
    kbo_fa_market_text_data_source_paths(
        save_path,
        source_db,
        sizeof(source_db),
        source_wal,
        sizeof(source_wal),
        source_shm,
        sizeof(source_shm));

    if (!kbo_fa_market_get_file_signature(source_db, db_sig)
            || !kbo_fa_market_get_file_signature(source_wal, wal_sig)
            || !kbo_fa_market_get_file_signature(source_shm, shm_sig)) {
        return 0;
    }
    return db_sig != NULL && db_sig->exists;
}

int kbo_fa_market_history_cache_matches(
    const char* save_path,
    const KboFaMarketFileSignature* db_sig,
    const KboFaMarketFileSignature* wal_sig,
    const KboFaMarketFileSignature* shm_sig)
{
    return g_kbo_fa_market_history_cache_valid
        && save_path != NULL
        && _stricmp(g_kbo_fa_market_history_cache_save_path, save_path) == 0
        && kbo_fa_market_file_signature_equal(&g_kbo_fa_market_history_cache_db_sig, db_sig)
        && kbo_fa_market_file_signature_equal(&g_kbo_fa_market_history_cache_wal_sig, wal_sig)
        && kbo_fa_market_file_signature_equal(&g_kbo_fa_market_history_cache_shm_sig, shm_sig);
}

int kbo_fa_market_copy_history_cache_for_rows(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories)
{
    if (rows == NULL || row_count <= 0 || histories == NULL || max_histories < row_count) {
        return 0;
    }
    memset(histories, 0, (SIZE_T)max_histories * sizeof(histories[0]));
    int found = 0;
    for (int i = 0; i < row_count; i++) {
        histories[i].player_id = rows[i].player_id;
        for (int j = 0; j < g_kbo_fa_market_history_cache_count; j++) {
            if (g_kbo_fa_market_history_cache[j].player_id == rows[i].player_id) {
                histories[i] = g_kbo_fa_market_history_cache[j];
                if (histories[i].found) {
                    found++;
                }
                break;
            }
        }
    }
    return found;
}

void kbo_fa_market_store_history_cache(
    const char* save_path,
    const KboFaMarketFileSignature* db_sig,
    const KboFaMarketFileSignature* wal_sig,
    const KboFaMarketFileSignature* shm_sig,
    const KboFaMarketHistoryCase* histories,
    int history_count)
{
    if (save_path == NULL || save_path[0] == '\0' || histories == NULL || history_count <= 0
            || history_count > KBO_FA_MARKET_CLASSIFICATION_MAX
            || db_sig == NULL || wal_sig == NULL || shm_sig == NULL) {
        return;
    }
    memset(g_kbo_fa_market_history_cache, 0, sizeof(g_kbo_fa_market_history_cache));
    memcpy(
        g_kbo_fa_market_history_cache,
        histories,
        (SIZE_T)history_count * sizeof(histories[0]));
    g_kbo_fa_market_history_cache_count = history_count;
    snprintf(g_kbo_fa_market_history_cache_save_path, sizeof(g_kbo_fa_market_history_cache_save_path), "%s", save_path);
    g_kbo_fa_market_history_cache_db_sig = *db_sig;
    g_kbo_fa_market_history_cache_wal_sig = *wal_sig;
    g_kbo_fa_market_history_cache_shm_sig = *shm_sig;
    g_kbo_fa_market_history_cache_valid = 1;
}

int kbo_fa_market_copy_file_if_present(const char* source, const char* destination)
{
    if (source == NULL || destination == NULL || source[0] == '\0' || destination[0] == '\0') {
        return 0;
    }
    DWORD attributes = GetFileAttributesA(source);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        DeleteFileA(destination);
        return 1;
    }
    if (CopyFileA(source, destination, FALSE)) {
        return 1;
    }
    kbo_log_runtimef("FA market history sqlite copy failed src=%s dst=%s gle=%lu", source, destination, GetLastError());
    return 0;
}

int kbo_fa_market_copy_text_data_sqlite(char* out_path, size_t out_path_size)
{
    if (out_path == NULL || out_path_size == 0) {
        return 0;
    }
    out_path[0] = '\0';

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        kbo_log_runtime_line("FA market history sqlite skipped reason=no_current_save_path");
        return 0;
    }

    char source_db[MAX_PATH] = {0};
    char source_wal[MAX_PATH] = {0};
    char source_shm[MAX_PATH] = {0};
    kbo_fa_market_text_data_source_paths(
        save_path,
        source_db,
        sizeof(source_db),
        source_wal,
        sizeof(source_wal),
        source_shm,
        sizeof(source_shm));

    if (GetFileAttributesA(source_db) == INVALID_FILE_ATTRIBUTES) {
        kbo_log_runtimef("FA market history sqlite skipped reason=source_missing path=%s", source_db);
        return 0;
    }

    DWORD pid = GetCurrentProcessId();
    char dest_name[128] = {0};
    char dest_db[MAX_PATH] = {0};
    char dest_wal[MAX_PATH] = {0};
    char dest_shm[MAX_PATH] = {0};
    snprintf(dest_name, sizeof(dest_name), "cache\\fa_market_text_data_%lu.sqlite3", (unsigned long)pid);
    if (!kbo_get_save_scoped_data_file(dest_name, dest_db, sizeof(dest_db))) {
        kbo_log_runtime_line("FA market history sqlite skipped reason=no_cache_path");
        return 0;
    }
    snprintf(dest_wal, sizeof(dest_wal), "%s-wal", dest_db);
    snprintf(dest_shm, sizeof(dest_shm), "%s-shm", dest_db);

    if (!kbo_fa_market_copy_file_if_present(source_db, dest_db)) {
        return 0;
    }
    kbo_fa_market_copy_file_if_present(source_wal, dest_wal);
    kbo_fa_market_copy_file_if_present(source_shm, dest_shm);

    snprintf(out_path, out_path_size, "%s", dest_db);
    return out_path[0] != '\0';
}

void kbo_fa_market_copy_sqlite_text_column(
    KboFaMarketSqliteApi* api,
    KboFaMarketSqlite3Stmt* stmt,
    int column,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (api == NULL || stmt == NULL || api->column_text == NULL) {
        return;
    }
    const unsigned char* text = api->column_text(stmt, column);
    if (text == NULL) {
        return;
    }
    snprintf(out, out_size, "%s", (const char*)text);
}

uint32_t kbo_fa_market_resolve_league_id(uint32_t requested_league_id)
{
    (void)requested_league_id;

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id != 0u) {
        return league_id;
    }
    return kbo_resolve_kbo_league_id();
}

