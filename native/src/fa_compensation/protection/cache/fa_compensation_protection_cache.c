#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "fa_compensation_protection_cache.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/sync/lock.h"

#define KBO_FA_PROTECTION_TEAM_CACHE_SIZE 8

typedef struct KboFaProtectionTeamCandidateCacheEntry {
    uint8_t valid;
    uint32_t signing_team_id;
    uint32_t league_id;
    uintptr_t player_vector;
    int32_t player_count;
    int candidate_count;
    KboFaProtectedCandidate candidates[KBO_FA_PROTECTION_TEAM_CACHE_CANDIDATE_MAX];
    KboFaProtectedCandidate auto_protected[KBO_FA_COMPENSATION_PROTECTED_LIST_MAX];
    int auto_count;
} KboFaProtectionTeamCandidateCacheEntry;

static KboRwLock g_kbo_fa_protection_team_cache_lock = KBO_RW_LOCK_INIT;
static KboFaProtectionTeamCandidateCacheEntry g_kbo_fa_protection_team_cache[KBO_FA_PROTECTION_TEAM_CACHE_SIZE];
static LONG g_kbo_fa_protection_team_cache_cursor = 0;


int kbo_fa_materialize_protection_candidates(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* base_candidates,
    int base_candidate_count,
    const KboFaProtectedCandidate* base_auto_protected,
    int base_auto_count,
    KboFaProtectedCandidate* candidates,
    int max_candidates,
    KboFaProtectedCandidate* auto_protected,
    int max_auto_protected)
{
    if (rec == NULL || base_candidates == NULL || candidates == NULL || max_candidates <= 0) {
        return 0;
    }

    int count = 0;
    int auto_count = 0;
    int signed_fa_added = 0;
    if (auto_protected != NULL && max_auto_protected > 0 && base_auto_protected != NULL) {
        for (int i = 0; i < base_auto_count && auto_count < max_auto_protected; i++) {
            KboFaProtectedCandidate automatic = base_auto_protected[i];
            if (automatic.player_id == rec->player_id) {
                snprintf(automatic.reason, sizeof(automatic.reason), "%s", "signed_fa");
                signed_fa_added = 1;
            }
            auto_protected[auto_count++] = automatic;
        }
    } else if (base_auto_protected != NULL) {
        for (int i = 0; i < base_auto_count; i++) {
            if (base_auto_protected[i].player_id == rec->player_id) {
                signed_fa_added = 1;
                break;
            }
        }
    }

    for (int i = 0; i < base_candidate_count && count < max_candidates; i++) {
        if (base_candidates[i].player_id == rec->player_id) {
            if (!signed_fa_added && auto_protected != NULL && auto_count < max_auto_protected) {
                KboFaProtectedCandidate automatic = base_candidates[i];
                snprintf(automatic.reason, sizeof(automatic.reason), "%s", "signed_fa");
                auto_protected[auto_count++] = automatic;
                signed_fa_added = 1;
            }
            continue;
        }
        candidates[count++] = base_candidates[i];
    }
    return count;
}

int kbo_fa_try_materialize_cached_protection_candidates(
    const KboFaCompensationRecord* rec,
    uintptr_t player_vector,
    int32_t player_count,
    KboFaProtectedCandidate* candidates,
    int max_candidates,
    KboFaProtectedCandidate* auto_protected,
    int max_auto_protected)
{
    if (rec == NULL || candidates == NULL || max_candidates <= 0 || player_vector == 0 || player_count <= 0) {
        return -1;
    }
    int result = -1;
    kbo_rw_lock_enter_shared(&g_kbo_fa_protection_team_cache_lock);
    for (int i = 0; i < KBO_FA_PROTECTION_TEAM_CACHE_SIZE; i++) {
        const KboFaProtectionTeamCandidateCacheEntry* entry = &g_kbo_fa_protection_team_cache[i];
        if (!entry->valid
                || entry->signing_team_id != rec->signing_team_id
                || entry->league_id != rec->league_id
                || entry->player_vector != player_vector
                || entry->player_count != player_count) {
            continue;
        }
        result = kbo_fa_materialize_protection_candidates(
            rec,
            entry->candidates,
            entry->candidate_count,
            entry->auto_protected,
            entry->auto_count,
            candidates,
            max_candidates,
            auto_protected,
            max_auto_protected);
        break;
    }
    kbo_rw_lock_leave_shared(&g_kbo_fa_protection_team_cache_lock);
    if (result >= 0) {
        kbo_profiler_record_us("fa_compensation.protection.team_cache_hit", 0);
    }
    return result;
}

void kbo_fa_store_protection_candidate_cache(
    const KboFaCompensationRecord* rec,
    uintptr_t player_vector,
    int32_t player_count,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const KboFaProtectedCandidate* auto_protected,
    int auto_count)
{
    if (rec == NULL || candidates == NULL || candidate_count < 0 || player_vector == 0 || player_count <= 0) {
        return;
    }
    LONG cursor = InterlockedIncrement(&g_kbo_fa_protection_team_cache_cursor);
    int slot = (int)((uint32_t)cursor % KBO_FA_PROTECTION_TEAM_CACHE_SIZE);
    kbo_rw_lock_enter_exclusive(&g_kbo_fa_protection_team_cache_lock);
    KboFaProtectionTeamCandidateCacheEntry* entry = &g_kbo_fa_protection_team_cache[slot];
    memset(entry, 0, sizeof(*entry));
    entry->valid = 1u;
    entry->signing_team_id = rec->signing_team_id;
    entry->league_id = rec->league_id;
    entry->player_vector = player_vector;
    entry->player_count = player_count;
    entry->candidate_count = candidate_count > KBO_FA_PROTECTION_TEAM_CACHE_CANDIDATE_MAX
        ? KBO_FA_PROTECTION_TEAM_CACHE_CANDIDATE_MAX
        : candidate_count;
    memcpy(entry->candidates, candidates, (SIZE_T)entry->candidate_count * sizeof(entry->candidates[0]));
    if (auto_protected != NULL && auto_count > 0) {
        entry->auto_count = auto_count > KBO_FA_COMPENSATION_PROTECTED_LIST_MAX
            ? KBO_FA_COMPENSATION_PROTECTED_LIST_MAX
            : auto_count;
        memcpy(entry->auto_protected, auto_protected, (SIZE_T)entry->auto_count * sizeof(entry->auto_protected[0]));
    }
    kbo_rw_lock_leave_exclusive(&g_kbo_fa_protection_team_cache_lock);
}

