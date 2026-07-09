#include "../../internal/foreign_injury_internal.h"

/* Foreign-count exclusion checks used to take the record lock and resolve
 * team orgs per player inside org-count scan loops (records x players team
 * lookups per rebuild). The snapshot is built once per scan: records that can
 * never exclude (not OPEN/ACTIVE) are dropped up front and each record's org
 * id is resolved once. The per-player check is then lock-free and only
 * resolves the candidate team's org when an injured-player id matches. */

void kbo_foreign_injury_build_exclusion_snapshot(KboForeignInjuryExclusionSnapshot* out)
{
    if (out == NULL) {
        return;
    }
    out->count = 0;

    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements_shared();
    for (int i = 0;
            i < g_kbo_foreign_injury_replacement_count
                && out->count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX;
            i++) {
        const KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->injured_player_id == 0u
                || (rec->status != KBO_FOREIGN_INJURY_STATUS_OPEN
                    && rec->status != KBO_FOREIGN_INJURY_STATUS_ACTIVE)) {
            continue;
        }
        out->records[out->count] = *rec;
        out->count++;
    }
    kbo_unlock_foreign_injury_replacements_shared();

    /* Resolve record orgs outside the lock: team lookups can be slow. */
    for (int i = 0; i < out->count; i++) {
        out->org_team_ids[i] = kbo_org_team_id_for_team_id(out->records[i].team_id);
    }
}

int kbo_foreign_injury_player_excluded_from_foreign_count_snapshot(
    const KboForeignInjuryExclusionSnapshot* snapshot,
    uint32_t team_id,
    uint32_t player_id)
{
    if (snapshot == NULL || team_id == 0u || player_id == 0u) {
        return 0;
    }

    uint32_t team_org_id = 0u;
    int team_org_resolved = 0;
    for (int i = 0; i < snapshot->count; i++) {
        const KboForeignInjuryReplacement* rec = &snapshot->records[i];
        if (rec->injured_player_id != player_id || snapshot->org_team_ids[i] == 0u) {
            continue;
        }
        if (!team_org_resolved) {
            team_org_id = kbo_org_team_id_for_team_id(team_id);
            team_org_resolved = 1;
        }
        if (snapshot->org_team_ids[i] == team_org_id
                && kbo_foreign_injury_state_record_has_minimum_injury_basis(rec)) {
            return 1;
        }
    }
    return 0;
}
