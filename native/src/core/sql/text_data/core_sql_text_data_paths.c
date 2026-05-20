#include "core_sql_text_data_paths.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#include "../../files/save_paths/core_save_paths.h"
#include "../../product/ootp_product.h"

static void kbo_clear_path(char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
}

int kbo_core_sql_text_data_path_from_save(const char* save_path, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (save_path == NULL || save_path[0] == '\0') {
        return 0;
    }

    int written = snprintf(
        out,
        out_size,
        "%s\\%s\\%s",
        save_path,
        KBO_OOTP_TEXT_DATA_TEMP_DIR,
        KBO_OOTP_TEXT_DATA_SQLITE_FILE);
    return written > 0 && (size_t)written < out_size;
}

int kbo_core_sql_current_text_data_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }
    return kbo_core_sql_text_data_path_from_save(save_path, out, out_size);
}

void kbo_core_sql_text_data_companion_paths(
    const char* save_path,
    char* source_db,
    size_t source_db_size,
    char* source_wal,
    size_t source_wal_size,
    char* source_shm,
    size_t source_shm_size)
{
    kbo_clear_path(source_db, source_db_size);
    kbo_clear_path(source_wal, source_wal_size);
    kbo_clear_path(source_shm, source_shm_size);
    if (!kbo_core_sql_text_data_path_from_save(save_path, source_db, source_db_size)) {
        return;
    }
    if (source_wal != NULL && source_wal_size > 0u) {
        snprintf(source_wal, source_wal_size, "%s-wal", source_db);
    }
    if (source_shm != NULL && source_shm_size > 0u) {
        snprintf(source_shm, source_shm_size, "%s-shm", source_db);
    }
}
