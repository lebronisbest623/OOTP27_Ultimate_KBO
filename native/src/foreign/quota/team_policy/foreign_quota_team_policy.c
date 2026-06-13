#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../military_service/players/team_policy/military_service_team_policy.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/classification/team_classification.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "foreign_quota_team_policy.h"

typedef struct KboForeignQuotaTeamPolicyCacheEntry {
    uint32_t team_id;
    DWORD tick;
    KboForeignQuotaTeamPolicySnapshot snapshot;
    uint8_t valid;
} KboForeignQuotaTeamPolicyCacheEntry;

enum {
    KBO_FOREIGN_QUOTA_TEAM_POLICY_CACHE_SIZE = 512,
    KBO_FOREIGN_QUOTA_TEAM_POLICY_CACHE_TTL_MS = 1000u
};

static KboForeignQuotaTeamPolicyCacheEntry
    g_kbo_foreign_quota_team_policy_cache[KBO_FOREIGN_QUOTA_TEAM_POLICY_CACHE_SIZE];

static int kbo_foreign_quota_team_matches_csv_id(uint32_t team_id, const char* csv_id)
{
    if (team_id == 0u || csv_id == NULL || csv_id[0] == '\0') {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_csv_id_any_league(csv_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) == team_id;
}

static void kbo_foreign_quota_team_policy_compute(
    uint32_t team_id,
    KboForeignQuotaTeamPolicySnapshot* out_snapshot)
{
    if (out_snapshot == NULL) {
        return;
    }

    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->team_id = team_id;

    int military_or_police = team_id != 0u
        && (kbo_team_id_is_military_service_team(team_id)
        || kbo_foreign_quota_team_matches_csv_id(team_id, "SANG")
        || kbo_foreign_quota_team_matches_csv_id(team_id, "KPB"));
    int futures_independent = team_id != 0u
        && !military_or_police
        && kbo_team_classification_independent_kind_for_team(team_id)
            == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES;

    out_snapshot->military_or_police = military_or_police ? 1u : 0u;
    out_snapshot->blocks_foreign_ownership = military_or_police ? 1u : 0u;
    out_snapshot->futures_independent = futures_independent ? 1u : 0u;
    out_snapshot->allows_injury_extra_slots =
        (!military_or_police && !futures_independent) ? 1u : 0u;
    if (military_or_police) {
        out_snapshot->base_effective_limit = 0u;
    } else if (futures_independent) {
        out_snapshot->base_effective_limit =
            KBO_FOREIGN_QUOTA_FUTURES_INDEPENDENT_FOREIGN_LIMIT;
    } else {
        out_snapshot->base_effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    }
}

int kbo_foreign_quota_team_policy_snapshot(
    uint32_t team_id,
    KboForeignQuotaTeamPolicySnapshot* out_snapshot)
{
    if (out_snapshot == NULL) {
        return 0;
    }

    DWORD now = GetTickCount();
    uint32_t slot = (team_id ^ (team_id >> 8u))
        & (KBO_FOREIGN_QUOTA_TEAM_POLICY_CACHE_SIZE - 1u);
    KboForeignQuotaTeamPolicyCacheEntry* cached =
        &g_kbo_foreign_quota_team_policy_cache[slot];
    if (cached->valid
            && cached->team_id == team_id
            && cached->tick != 0u
            && (DWORD)(now - cached->tick) <= KBO_FOREIGN_QUOTA_TEAM_POLICY_CACHE_TTL_MS) {
        *out_snapshot = cached->snapshot;
        return 1;
    }

    KboForeignQuotaTeamPolicySnapshot snapshot;
    kbo_foreign_quota_team_policy_compute(team_id, &snapshot);

    cached->valid = 0u;
    cached->team_id = team_id;
    cached->snapshot = snapshot;
    cached->tick = now;
    cached->valid = 1u;

    *out_snapshot = snapshot;
    return 1;
}

int kbo_foreign_quota_team_is_military_or_police(uint32_t team_id)
{
    KboForeignQuotaTeamPolicySnapshot snapshot;
    return kbo_foreign_quota_team_policy_snapshot(team_id, &snapshot)
        && snapshot.military_or_police;
}

int kbo_foreign_quota_team_blocks_foreign_ownership(uint32_t team_id)
{
    KboForeignQuotaTeamPolicySnapshot snapshot;
    return kbo_foreign_quota_team_policy_snapshot(team_id, &snapshot)
        && snapshot.blocks_foreign_ownership;
}

int kbo_foreign_quota_team_is_futures_independent(uint32_t team_id)
{
    KboForeignQuotaTeamPolicySnapshot snapshot;
    return kbo_foreign_quota_team_policy_snapshot(team_id, &snapshot)
        && snapshot.futures_independent;
}

int kbo_foreign_quota_team_allows_injury_extra_slots(uint32_t team_id)
{
    KboForeignQuotaTeamPolicySnapshot snapshot;
    return kbo_foreign_quota_team_policy_snapshot(team_id, &snapshot)
        && snapshot.allows_injury_extra_slots;
}

uint32_t kbo_foreign_quota_base_effective_limit_for_team(uint32_t team_id)
{
    KboForeignQuotaTeamPolicySnapshot snapshot;
    if (!kbo_foreign_quota_team_policy_snapshot(team_id, &snapshot)) {
        return KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    }
    return snapshot.base_effective_limit;
}

uint32_t kbo_foreign_quota_effective_count_for_team(
    uint32_t team_id,
    uint32_t asian_count,
    uint32_t non_asian_foreign_count)
{
    KboForeignQuotaTeamPolicySnapshot snapshot;
    if (kbo_foreign_quota_team_policy_snapshot(team_id, &snapshot)
            && snapshot.futures_independent) {
        return asian_count + non_asian_foreign_count;
    }
    return non_asian_foreign_count + (asian_count > 0u ? asian_count - 1u : 0u);
}
