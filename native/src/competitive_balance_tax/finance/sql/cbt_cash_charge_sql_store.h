#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_FINANCE_SQL_CBT_CASH_CHARGE_SQL_STORE_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_FINANCE_SQL_CBT_CASH_CHARGE_SQL_STORE_H_

#include <stdint.h>

#include "../../records/cbt_records.h"

int kbo_cbt_cash_charge_sql_already_applied(uint32_t season, uint32_t team_id);
int kbo_cbt_cash_charge_sql_append_ledger(
    const KboCbtRecord* rec,
    int32_t old_cash,
    int32_t new_cash,
    uint32_t applied_yyyymmdd,
    const char* source);

#endif
