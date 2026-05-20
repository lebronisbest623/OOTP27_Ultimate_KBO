#include "cbt_draft_order_ledger.h"
#include "sql/cbt_draft_order_ledger_sql_store.h"

int kbo_cbt_draft_order_append_ledger(const KboCbtDraftOrderMove* move, const char* source)
{
    return kbo_cbt_draft_order_ledger_sql_append(move, source);
}
