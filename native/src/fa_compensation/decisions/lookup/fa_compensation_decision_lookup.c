#include "../fa_compensation_decisions_internal.h"
#include "../sql/fa_compensation_decisions_sql_store.h"

int kbo_load_latest_fa_compensation_decision(
    uint32_t fa_player_id,
    KboFaCompensationDecisionRow* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (fa_player_id == 0u || out == NULL) {
        return 0;
    }

    return kbo_fa_compensation_decisions_sql_load_latest(fa_player_id, out);
}

