#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "foreign_retention_guard_internal.h"
#include "repair/foreign_retention_guard_repair_helpers.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../team/lookup/team_lookup.h"
#include "../common/dates/foreign_waiver_date.h"
#include "../common/player_eval/foreign_waiver_player_eval.h"
#include "../common/policy/foreign_waiver_policy.h"
#include "../rights/query/foreign_waiver_rights_query.h"

int kbo_foreign_retention_guard_restore_recorded_holder_signing(
    const char* source,
    uint8_t* player,
    uint8_t* team,
    uint32_t today)
{
    if (player == NULL || team == NULL || today == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (player_id == 0u || team_id == 0u) {
        return 0;
    }

    KboForeignRetentionGuardRecord records[KBO_FOREIGN_RETENTION_GUARD_MAX];
    int count = 0;
    kbo_foreign_retention_guard_snapshot(records, &count, today);

    const KboForeignRetentionGuardRecord* rec = NULL;
    for (int i = 0; i < count; i++) {
        if (records[i].player_id == player_id && records[i].team_id == team_id) {
            rec = &records[i];
            break;
        }
    }
    if (rec == NULL) {
        return 0;
    }

    uint32_t before_current = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t before_active = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t before_original = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint32_t before_default = memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
        : 0u;
    uint32_t before_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint8_t before_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t before_secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t before_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];

    int changed = kbo_foreign_retention_guard_stamp_signed_state(player, team, rec);
    if (changed) {
        kbo_foreign_retention_guard_mark_repaired(rec->player_id, rec->team_id, today);
        kbo_foreign_retention_guard_log_restored(
            source,
            rec,
            player,
            today,
            before_current,
            before_active,
            before_original,
            before_default,
            before_league,
            before_restricted,
            before_secondary,
            before_contract_level);
    }

    return 1;
}

void kbo_foreign_retention_guard_repair(const char* source)
{
    if (!kbo_fix_enabled() || !kbo_foreign_waiver_ai_enabled()) {
        return;
    }

    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        return;
    }

    KboForeignRetentionGuardRecord records[KBO_FOREIGN_RETENTION_GUARD_MAX];
    int count = 0;
    kbo_foreign_retention_guard_snapshot(records, &count, today);
    for (int i = 0; i < count; i++) {
        KboForeignRetentionGuardRecord* rec = &records[i];
        uint32_t holder_team_id = 0u;
        if (kbo_find_active_foreign_waiver_holder(rec->player_id, today, &holder_team_id)
                && holder_team_id != rec->team_id) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(rec->player_id, NULL, NULL);
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(rec->team_id, 1);
        if (player == NULL || team == NULL
                || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint32_t before_current = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t before_active = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t before_original = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
        uint32_t before_default = memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
            : 0u;
        uint32_t before_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint8_t before_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        uint8_t before_secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        uint8_t before_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];

        if (kbo_player_current_assignment_matches_team_or_affiliate(player, rec->team_id)) {
            if (kbo_foreign_retention_guard_clean_signed_holder(player, team, rec)) {
                kbo_foreign_retention_guard_log_cleaned(
                    source,
                    rec,
                    player,
                    today,
                    before_current,
                    before_active,
                    before_original,
                    before_default,
                    before_league,
                    before_restricted,
                    before_secondary,
                    before_contract_level);
            }
            continue;
        }

        if (!kbo_foreign_retention_guard_player_repairable(player, rec)
                || !kbo_foreign_retention_guard_stamp_signed_state(player, team, rec)) {
            continue;
        }
        kbo_foreign_retention_guard_mark_repaired(rec->player_id, rec->team_id, today);
        kbo_foreign_retention_guard_log_restored(
            source,
            rec,
            player,
            today,
            before_current,
            before_active,
            before_original,
            before_default,
            before_league,
            before_restricted,
            before_secondary,
            before_contract_level);
    }
}
