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
 * OOTP stores human managers in the global DB next to the team vector:
 * The human-manager vector, its count, and the manager team id are described by
 * OOTP27_GLOBAL_HUMAN_MANAGER_* offsets.
 */

#define KBO_HUMAN_CONTROL_MAX_TEAMS 16
#define KBO_HUMAN_CONTROL_CACHE_MS 2000u

static uint32_t g_kbo_human_control_cache[KBO_HUMAN_CONTROL_MAX_TEAMS];
static int g_kbo_human_control_cache_count = -1;
static ULONGLONG g_kbo_human_control_cache_ms = 0;
static uint32_t g_kbo_human_control_last_log_hash = 0;

static int kbo_human_control_team_id_valid(uint32_t team_id)
{
    if (team_id == 0 || team_id > 100000u) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    return team != NULL;
}

static int kbo_human_control_append_unique(uint32_t* out_team_ids, int count, int max_team_ids, uint32_t team_id)
{
    if (out_team_ids == NULL || count < 0 || count >= max_team_ids) {
        return count;
    }
    for (int i = 0; i < count; i++) {
        if (out_team_ids[i] == team_id) {
            return count;
        }
    }
    out_team_ids[count] = team_id;
    return count + 1;
}

static int kbo_human_manager_pointer_plausible(uintptr_t manager_ptr)
{
    if (manager_ptr == 0 || !memory_range_readable((void*)manager_ptr, OOTP27_HUMAN_MANAGER_READABLE_BYTES)) {
        return 0;
    }

    uintptr_t vtable = *(uintptr_t*)manager_ptr;
    if (vtable < KBO_RUNTIME_MIN_USER_POINTER || !memory_range_readable((void*)vtable, sizeof(uintptr_t))) {
        return 0;
    }

    return 1;
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

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)global, OOTP27_GLOBAL_HUMAN_MANAGER_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uintptr_t manager_vector = *(uintptr_t*)(global + OOTP27_GLOBAL_HUMAN_MANAGER_VECTOR_OFFSET);
    int32_t manager_count = *(int32_t*)(global + OOTP27_GLOBAL_HUMAN_MANAGER_COUNT_OFFSET);
    if (manager_vector == 0 || manager_count <= 0 || manager_count > 64
            || !memory_range_readable((void*)manager_vector, (SIZE_T)manager_count * sizeof(uintptr_t))) {
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < manager_count && count < max_team_ids; i++) {
        uintptr_t manager_ptr = *(uintptr_t*)(manager_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_human_manager_pointer_plausible(manager_ptr)) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)((uint8_t*)manager_ptr + OOTP27_HUMAN_MANAGER_CONTROLLED_TEAM_OFFSET);
        if (!kbo_human_control_team_id_valid(team_id)) {
            kbo_log_runtimef(
                "KBO human manager team candidate rejected source=%s index=%d team=%u",
                source != NULL ? source : "",
                (int)i,
                team_id);
            continue;
        }

        count = kbo_human_control_append_unique(out_team_ids, count, max_team_ids, team_id);
    }

    kbo_human_control_log_if_changed(out_team_ids, count, source);
    return count;
}

static int kbo_resolve_human_controlled_team_ids(uint32_t* out_team_ids, int max_team_ids, const char* source)
{
    if (out_team_ids == NULL || max_team_ids <= 0) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    if (g_kbo_human_control_cache_count >= 0
            && now - g_kbo_human_control_cache_ms < KBO_HUMAN_CONTROL_CACHE_MS) {
        int count = g_kbo_human_control_cache_count;
        if (count > max_team_ids) {
            count = max_team_ids;
        }
        for (int i = 0; i < count; i++) {
            out_team_ids[i] = g_kbo_human_control_cache[i];
        }
        return count;
    }

    uint32_t resolved[KBO_HUMAN_CONTROL_MAX_TEAMS];
    int count = kbo_resolve_human_controlled_team_ids_uncached(
        resolved,
        KBO_HUMAN_CONTROL_MAX_TEAMS,
        source);

    g_kbo_human_control_cache_count = count;
    g_kbo_human_control_cache_ms = now;
    for (int i = 0; i < count; i++) {
        g_kbo_human_control_cache[i] = resolved[i];
    }

    int copy_count = count;
    if (copy_count > max_team_ids) {
        copy_count = max_team_ids;
    }
    for (int i = 0; i < copy_count; i++) {
        out_team_ids[i] = resolved[i];
    }
    return copy_count;
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

