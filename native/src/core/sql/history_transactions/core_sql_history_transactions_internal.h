#ifndef KBO_CORE_SQL_HISTORY_TRANSACTIONS_INTERNAL_H
#define KBO_CORE_SQL_HISTORY_TRANSACTIONS_INTERNAL_H

int kbo_history_sqlite_exec_logged(void* database, const char* sql, const char* label, const char* source);

#endif
