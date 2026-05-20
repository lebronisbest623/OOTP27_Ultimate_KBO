#ifndef KBOFIX_SRC_FOREIGN_INTL_ESTABLISHED_FA_POSTSCAN_STATE_SQL_INTL_ESTABLISHED_FA_POSTSCAN_STATE_SQL_STORE_H_
#define KBOFIX_SRC_FOREIGN_INTL_ESTABLISHED_FA_POSTSCAN_STATE_SQL_INTL_ESTABLISHED_FA_POSTSCAN_STATE_SQL_STORE_H_

#include "../../internal/intl_established_fa_postscan_internal.h"

int kbo_intl_established_fa_postscan_state_sql_path(char* out, size_t out_size);
int kbo_intl_established_fa_postscan_state_sql_persist(
    const KboIntlEstablishedFaPostscanState* state,
    const char* source);
int kbo_intl_established_fa_postscan_state_sql_load(
    KboIntlEstablishedFaPostscanState* out,
    const char* source);

#endif
