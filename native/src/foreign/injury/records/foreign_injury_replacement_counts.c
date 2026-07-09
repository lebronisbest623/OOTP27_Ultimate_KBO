#include "../internal/foreign_injury_internal.h"

void kbo_count_foreign_injury_replacements_for_team(
    uint32_t team_id,
    int* out_open,
    int* out_pending,
    int* out_closed)
{
    if (out_open != NULL) { *out_open = 0; }
    if (out_pending != NULL) { *out_pending = 0; }
    if (out_closed != NULL) { *out_closed = 0; }

    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements_shared();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (team_id != 0u && !kbo_team_ids_share_org(rec->team_id, team_id)) {
            continue;
        }
        if (kbo_foreign_injury_status_uses_slot(rec->status)
                && kbo_foreign_injury_record_has_minimum_injury_basis(rec)) {
            if (out_open != NULL) { (*out_open)++; }
        } else if (rec->status == KBO_FOREIGN_INJURY_STATUS_PENDING) {
            if (out_pending != NULL) { (*out_pending)++; }
        } else if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            if (out_closed != NULL) { (*out_closed)++; }
        }
    }
    kbo_unlock_foreign_injury_replacements_shared();
}
