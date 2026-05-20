#ifndef KBOFIX_SRC_FA_COMPENSATION_FINANCE_SQL_FA_COMPENSATION_CASH_TRANSFER_SQL_STORE_H_
#define KBOFIX_SRC_FA_COMPENSATION_FINANCE_SQL_FA_COMPENSATION_CASH_TRANSFER_SQL_STORE_H_

#include <stddef.h>

#include "../fa_compensation_cash_transfer.h"

int kbo_fa_compensation_cash_transfer_sql_path(char* out, size_t out_size);
int kbo_fa_compensation_cash_transfer_sql_already_applied(
    uint32_t season,
    uint32_t player_id,
    uint32_t signing_team_id,
    uint32_t original_team_id,
    const char* action);
int kbo_fa_compensation_cash_transfer_sql_append(
    const KboFaCompensationRecord* rec,
    uint32_t amount,
    uint32_t applied_yyyymmdd,
    const char* action,
    const char* source,
    int32_t signing_old_cash,
    int32_t signing_new_cash,
    int32_t original_old_cash,
    int32_t original_new_cash);

#endif
