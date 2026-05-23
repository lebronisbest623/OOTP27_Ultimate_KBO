#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "team_human_control.h"
#include "../lookup/team_lookup.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
/* Human-controlled team resolver.
 *
 * OOTP keeps the active human-control context behind the global current-league
 * slot. The old human-manager vector field stores stale profile/team data even
 * when the user has no team, so user actions must be gated by this live context.
 */

#define KBO_HUMAN_CONTROL_MAX_TEAMS 16

static uint32_t g_kbo_human_control_last_log_hash = 0;

static int kbo_human_control_team_id_valid(uint32_t team_id)
{
    if (team_id == 0 || team_id > KBO_RUNTIME_PLAUSIBLE_CONTEXT_ID_MAX) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    return team != NULL;
}

static uint32_t kbo_human_control_hash(const uint32_t* team_ids, int count)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < count; i++) {
        hash ^= team_ids[i];
        hash *= 16777619u;
    }
    hash ^= (uint32_t)count;
    return hash;
}

static void kbo_human_control_log_if_changed(const uint32_t* team_ids, int count, const char* source)
{
    uint32_t hash = kbo_human_control_hash(team_ids, count);
    if (hash == g_kbo_human_control_last_log_hash) {
        return;
    }
    g_kbo_human_control_last_log_hash = hash;

    char teams[160];
    teams[0] = '\0';
    for (int i = 0; i < count; i++) {
        char item[24];
        _snprintf_s(item, sizeof(item), _TRUNCATE, "%s%u", i == 0 ? "" : ",", team_ids[i]);
        strncat_s(teams, sizeof(teams), item, _TRUNCATE);
    }

    kbo_log_runtimef(
        "KBO human controlled teams resolved source=%s count=%d teams=%s",
        source != NULL ? source : "",
        count,
        teams[0] != '\0' ? teams : "-");
}

static int kbo_resolve_human_controlled_team_ids_uncached(uint32_t* out_team_ids, int max_team_ids, const char* source)
{
    if (out_team_ids == NULL || max_team_ids <= 0) {
        return 0;
    }
    out_team_ids[0] = 0u;

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_LEAGUE_OFFSET), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t context = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_LEAGUE_OFFSET);
    if (context == 0
            || !memory_range_readable((void*)context, OOTP27_HUMAN_CONTROL_CONTEXT_READABLE_BYTES)) {
        return 0;
    }

    uint8_t* context_bytes = (uint8_t*)context;
    uint32_t league_id = *(uint32_t*)(context_bytes + OOTP27_HUMAN_CONTROL_CONTEXT_LEAGUE_ID_OFFSET);
    uint32_t team_id = *(uint32_t*)(context_bytes + OOTP27_HUMAN_CONTROL_CONTEXT_TEAM_ID_OFFSET);
    uint32_t team_id_confirm = *(uint32_t*)(context_bytes + OOTP27_HUMAN_CONTROL_CONTEXT_TEAM_ID_CONFIRM_OFFSET);
    if (team_id == 0u && team_id_confirm == 0u) {
        kbo_human_control_log_if_changed(out_team_ids, 0, source);
        return 0;
    }

    if (team_id == 0u || team_id != team_id_confirm || !kbo_human_control_team_id_valid(team_id)) {
        kbo_log_runtimef(
            "KBO human control team candidate rejected source=%s league=%u team=%u confirm=%u",
            source != NULL ? source : "",
            league_id,
            team_id,
            team_id_confirm);
        kbo_human_control_log_if_changed(out_team_ids, 0, source);
        return 0;
    }

    if (league_id != 0u) {
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
        if (team == NULL
                || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))
                || *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) != league_id) {
            kbo_log_runtimef(
                "KBO human control team candidate rejected source=%s reason=league_mismatch league=%u team=%u",
                source != NULL ? source : "",
                league_id,
                team_id);
            kbo_human_control_log_if_changed(out_team_ids, 0, source);
            return 0;
        }
    }

    out_team_ids[0] = team_id;
    kbo_human_control_log_if_changed(out_team_ids, 1, source);
    return 1;
}

static int kbo_resolve_human_controlled_team_ids(uint32_t* out_team_ids, int max_team_ids, const char* source)
{
    if (out_team_ids == NULL || max_team_ids <= 0) {
        return 0;
    }

    /* Team-control state gates user actions, so it must reflect OOTP memory immediately. */
    return kbo_resolve_human_controlled_team_ids_uncached(out_team_ids, max_team_ids, source);
}

int kbo_collect_human_controlled_team_ids(uint32_t* out_team_ids, int max_team_ids, const char* source)
{
    return kbo_resolve_human_controlled_team_ids(out_team_ids, max_team_ids, source);
}

int kbo_team_is_human_controlled(uint32_t team_id, const char* source)
{
    if (team_id == 0) {
        return 0;
    }

    uint32_t team_ids[KBO_HUMAN_CONTROL_MAX_TEAMS];
    int count = kbo_resolve_human_controlled_team_ids(
        team_ids,
        KBO_HUMAN_CONTROL_MAX_TEAMS,
        source);
    for (int i = 0; i < count; i++) {
        if (team_ids[i] == team_id) {
            return 1;
        }
    }
    return 0;
}

