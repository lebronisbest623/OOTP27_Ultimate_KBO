#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../military_service/players/team_policy/military_service_team_policy.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/classification/team_classification.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "foreign_quota_team_policy.h"

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

int kbo_foreign_quota_team_is_military_or_police(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }
    return kbo_team_id_is_military_service_team(team_id)
        || kbo_foreign_quota_team_matches_csv_id(team_id, "SANG")
        || kbo_foreign_quota_team_matches_csv_id(team_id, "KPB");
}

int kbo_foreign_quota_team_blocks_foreign_ownership(uint32_t team_id)
{
    return kbo_foreign_quota_team_is_military_or_police(team_id);
}

int kbo_foreign_quota_team_is_futures_independent(uint32_t team_id)
{
    if (team_id == 0u || kbo_foreign_quota_team_is_military_or_police(team_id)) {
        return 0;
    }
    return kbo_team_classification_independent_kind_for_team(team_id)
        == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES;
}

int kbo_foreign_quota_team_allows_injury_extra_slots(uint32_t team_id)
{
    return !kbo_foreign_quota_team_blocks_foreign_ownership(team_id)
        && !kbo_foreign_quota_team_is_futures_independent(team_id);
}

uint32_t kbo_foreign_quota_base_effective_limit_for_team(uint32_t team_id)
{
    if (kbo_foreign_quota_team_blocks_foreign_ownership(team_id)) {
        return 0u;
    }
    if (kbo_foreign_quota_team_is_futures_independent(team_id)) {
        return KBO_FOREIGN_QUOTA_FUTURES_INDEPENDENT_FOREIGN_LIMIT;
    }
    return KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
}

uint32_t kbo_foreign_quota_effective_count_for_team(
    uint32_t team_id,
    uint32_t asian_count,
    uint32_t non_asian_foreign_count)
{
    if (kbo_foreign_quota_team_is_futures_independent(team_id)) {
        return asian_count + non_asian_foreign_count;
    }
    return non_asian_foreign_count + (asian_count > 0u ? asian_count - 1u : 0u);
}
