#include "../internal/foreign_quota_internal.h"

/* Active foreign count adjustment policy. Included from native/KBOFix.c. */

int32_t kbo_apply_active_asian_quota_count_exception(
    uintptr_t team_ptr,
    int32_t original_count,
    int pitcher_count)
{
    if (original_count <= 0) {
        return original_count;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    kbo_count_active_asian_quota_by_position(team_ptr, &asian_hitters, &asian_pitchers);

    int use_exception = 0;
    if (pitcher_count) {
        use_exception = (asian_pitchers > 0u && asian_hitters == 0u);
    } else {
        use_exception = (asian_hitters > 0u);
    }
    if (!use_exception) {
        return original_count;
    }

    int32_t adjusted = original_count - 1;
    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        uint32_t team_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        kbo_log_runtimef(
            "asian quota active foreign slot exception team=%u type=%s original=%d adjusted=%d asian_hitters=%u asian_pitchers=%u",
            team_id,
            pitcher_count ? "pitcher" : "hitter",
            original_count,
            adjusted,
            asian_hitters,
            asian_pitchers);
    }
    return adjusted;
}

int32_t kbo_custom_foreign_policy_neutralized_count(
    uintptr_t team_ptr,
    int32_t original_count,
    int pitcher_count)
{
    if (original_count <= 0) {
        return original_count;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);

    uint32_t team_id = 0u;
    if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
    }
    uint32_t effective = kbo_foreign_quota_effective_count_for_team(
        team_id,
        asian_hitters + asian_pitchers,
        non_asian_hitters + non_asian_pitchers);

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        kbo_log_runtimef(
            "custom foreign policy neutralized OOTP active count team=%u type=%s original=%d adjusted=0 active_effective=%u active_asian_h=%u active_asian_p=%u active_non_asian_h=%u active_non_asian_p=%u",
            team_id,
            pitcher_count ? "pitcher" : "hitter",
            original_count,
            effective,
            asian_hitters,
            asian_pitchers,
            non_asian_hitters,
            non_asian_pitchers);
    }
    return 0;
}

