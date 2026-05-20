#include "../ui_fa_views_internal.h"

int kbo_fa_compensation_record_is_final(const KboFaCompensationRecord* rec)
{
    if (rec == NULL) {
        return 0;
    }
    return rec->status == KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED
        || rec->status == KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED;
}

int kbo_fa_compensation_debug_row_count(
    const KboFaCompensationProtectionDebugRow* rows,
    int count,
    uint32_t fa_player_id)
{
    int row_count = 0;
    if (rows == NULL || count <= 0 || fa_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (rows[i].fa_player_id == fa_player_id) {
            row_count++;
        }
    }
    return row_count;
}

const char* kbo_fa_compensation_decision_label(
    const KboFaCompensationRecord* rec,
    const KboFaCompensationDecisionRow* decision,
    int has_decision)
{
    if (has_decision && decision != NULL) {
        if (strcmp(decision->action, "CASH_ONLY") == 0) {
            return "현금 보상";
        }
        if (decision->selected_player_name[0] != '\0') {
            return decision->selected_player_name;
        }
        return "선수+현금";
    }
    if (rec != NULL && rec->requires_player_compensation && rec->protect_count > 0u) {
        return "보호 명단 대기";
    }
    return "현금 보상";
}
