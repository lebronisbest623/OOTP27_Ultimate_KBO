#ifndef KBOFIX_SRC_CORE_SQL_TEXT_DATA_CORE_SQL_TEXT_DATA_PATHS_H_
#define KBOFIX_SRC_CORE_SQL_TEXT_DATA_CORE_SQL_TEXT_DATA_PATHS_H_

#include <stddef.h>

int kbo_core_sql_current_text_data_path(char* out, size_t out_size);
int kbo_core_sql_text_data_path_from_save(const char* save_path, char* out, size_t out_size);
void kbo_core_sql_text_data_companion_paths(
    const char* save_path,
    char* source_db,
    size_t source_db_size,
    char* source_wal,
    size_t source_wal_size,
    char* source_shm,
    size_t source_shm_size);

#endif
