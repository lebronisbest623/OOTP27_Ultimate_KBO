#ifndef KBOFIX_SRC_CAPTAIN_SQL_CAPTAIN_SELECTION_SQL_STORE_H_
#define KBOFIX_SRC_CAPTAIN_SQL_CAPTAIN_SELECTION_SQL_STORE_H_

#include "../internal/captain_selection_internal.h"

int kbo_captain_selection_sql_path(char* out, size_t out_size);
int kbo_captain_selection_sql_exists(uint32_t season);
int kbo_captain_selection_sql_load(uint32_t season, KboCaptainSelectionRow* rows, int max_rows, int* out_count);
int kbo_captain_selection_sql_replace_season(
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source);

#endif
