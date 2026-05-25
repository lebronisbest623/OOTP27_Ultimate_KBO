#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "foreign_no_minor_contract_repair.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"
#include "../../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"

static int kbo_foreign_no_minor_contract_repair_enabled(void)
{
    int disabled = 0;
    if (kbo_read_localappdata_json_flag_value(
            KBO_RUNTIME_FLAG_DISABLE_KBO_NO_MINOR_CONTRACT_PATCH_KEY,
            &disabled)
            && disabled) {
        return 0;
    }
    return kbo_fix_enabled() && kbo_custom_foreign_policy_enabled();
}

static uintptr_t* kbo_foreign_no_minor_contract_copy_player_vector_snapshot(
    uintptr_t player_vector,
    int32_t player_count,
    const char** out_failure_reason)
{
    if (out_failure_reason != NULL) {
        *out_failure_reason = "unknown";
    }
    if (player_vector == 0u || player_count <= 0 || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT) {
        if (out_failure_reason != NULL) { *out_failure_reason = "invalid_vector"; }
        return NULL;
    }
    if ((SIZE_T)player_count > ((SIZE_T)-1 / sizeof(uintptr_t))) {
        if (out_failure_reason != NULL) { *out_failure_reason = "count_overflow"; }
        return NULL;
    }

    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        if (out_failure_reason != NULL) { *out_failure_reason = "unreadable_vector"; }
        return NULL;
    }

    uintptr_t* snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (snapshot == NULL) {
        if (out_failure_reason != NULL) { *out_failure_reason = "alloc_failed"; }
        return NULL;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        if (out_failure_reason != NULL) { *out_failure_reason = "copy_failed"; }
        return NULL;
    }

    if (out_failure_reason != NULL) {
        *out_failure_reason = NULL;
    }
    return snapshot;
}

int kbo_foreign_no_minor_contract_repair_all(const char* source)
{
    if (!kbo_foreign_no_minor_contract_repair_enabled()) {
        return 0;
    }
    if (kbo_runtime_save_in_progress()) {
        return 0;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id == 0u) {
        return 0;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    uint32_t vector_offset = 0u;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        static volatile LONG no_vector_log_count = 0;
        if (InterlockedIncrement(&no_vector_log_count) <= 20) {
            kbo_log_runtimef(
                "foreign no-minor affiliate repair skipped source=%s reason=no_player_vector",
                source != NULL ? source : "");
        }
        return 0;
    }

    const char* snapshot_failure_reason = NULL;
    uintptr_t* player_snapshot = kbo_foreign_no_minor_contract_copy_player_vector_snapshot(
        player_vector,
        player_count,
        &snapshot_failure_reason);
    if (player_snapshot == NULL) {
        static volatile LONG snapshot_fail_log_count = 0;
        if (InterlockedIncrement(&snapshot_fail_log_count) <= 20) {
            kbo_log_runtimef(
                "foreign no-minor affiliate repair skipped source=%s reason=player_vector_snapshot_failed detail=%s vector=%p count=%d vector_off=0x%x",
                source != NULL ? source : "",
                snapshot_failure_reason != NULL ? snapshot_failure_reason : "unknown",
                (void*)player_vector,
                player_count,
                vector_offset);
        }
        return 0;
    }

    int scanned = 0;
    int foreign_seen = 0;
    int affiliate_seen = 0;
    int repaired = 0;
    static volatile LONG detail_log_count = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        scanned++;
        if (!kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }
        foreign_seen++;

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (player_id == 0u
                || current_team_id == 0u
                || current_league_id == 0u
                || current_league_id == kbo_league_id) {
            continue;
        }

        uint8_t* affiliate_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 1);
        if (affiliate_team == NULL || !memory_range_readable(affiliate_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint32_t affiliate_team_id = *(uint32_t*)(affiliate_team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint32_t affiliate_league_id = *(uint32_t*)(affiliate_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        uint32_t parent_team_id = *(uint32_t*)(affiliate_team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (affiliate_team_id != current_team_id
                || affiliate_league_id != current_league_id
                || affiliate_league_id == kbo_league_id
                || parent_team_id == 0u) {
            continue;
        }

        uint8_t* parent_team = find_kbo_team_by_numeric_id_any_league(parent_team_id, 1);
        if (parent_team == NULL || !memory_range_readable(parent_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint32_t parent_league_id = *(uint32_t*)(parent_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (parent_league_id != kbo_league_id) {
            continue;
        }
        affiliate_seen++;

        KboForeignNoMinorContractRepairResult result;
        if (!kbo_foreign_no_minor_contract_repair_affiliate_assignment(
                player,
                affiliate_team,
                parent_team,
                &result)) {
            continue;
        }

        repaired++;
        LONG detail_slot = InterlockedIncrement(&detail_log_count);
        if (detail_slot <= 200) {
            kbo_log_runtimef(
                "foreign no-minor affiliate assignment repaired source=%s player=%u affiliate_team=%u parent_team=%u before_current=%u before_active=%u before_original=%u before_default=%u before_league=%u before_draft_league=%u before_original_league=%u before_contract_level=%u after_current=%u after_active=%u after_original=%u after_default=%u after_league=%u after_contract_level=%u removed_affiliate=%d removed_parent_restricted=%d added_parent_assignment=%d",
                source != NULL ? source : "",
                result.player_id,
                result.affiliate_team_id,
                result.parent_team_id,
                result.before_current_team_id,
                result.before_active_team_id,
                result.before_original_team_id,
                result.before_default_team_id,
                result.before_current_league_id,
                result.before_draft_league_id,
                result.before_original_league_id,
                (uint32_t)result.before_contract_level,
                result.after_current_team_id,
                result.after_active_team_id,
                result.after_original_team_id,
                result.after_default_team_id,
                result.after_current_league_id,
                (uint32_t)result.after_contract_level,
                result.removed_affiliate_arrays,
                result.removed_parent_restricted,
                result.added_parent_assignment_arrays);
        }
    }

    HeapFree(GetProcessHeap(), 0, player_snapshot);

    if (repaired > 0) {
        kbo_log_runtimef(
            "foreign no-minor affiliate repair summary source=%s scanned=%d foreign_seen=%d affiliate_seen=%d repaired=%d",
            source != NULL ? source : "",
            scanned,
            foreign_seen,
            affiliate_seen,
            repaired);
    }
    return repaired;
}
