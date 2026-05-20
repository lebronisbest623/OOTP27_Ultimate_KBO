#ifndef KBOFIX_SRC_CORE_SQL_SAVE_STATE_SQLITE_H_
#define KBOFIX_SRC_CORE_SQL_SAVE_STATE_SQLITE_H_

#include <stddef.h>

typedef int (__cdecl *KboSaveStateSqliteCallback)(void*, int, char**, char**);

int kbo_save_state_db_path(char* out, size_t out_size);
int kbo_save_state_exec(const char* sql, const char* source);
int kbo_save_state_query(
    const char* sql,
    KboSaveStateSqliteCallback callback,
    void* callback_arg,
    const char* source);

#endif
