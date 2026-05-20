#include "../api/league_context_lookup.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../league_roles/kbo_league_roles.h"
#include "../../logging/core_log.h"

uint32_t kbo_resolve_kbo_league_id(void)
{
    static uint32_t cached_league_id = 0;
    uint32_t override = kbo_read_kbo_league_id_override();
    if (override != 0) {
        if (cached_league_id == 0 || cached_league_id != override) {
            kbo_log_runtimef("KBO league id override used override=%u cached=%u", override, cached_league_id);
        }
        cached_league_id = override;
        return cached_league_id;
    }

    if (cached_league_id != 0) {
        return cached_league_id;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return kbo_league_role_main_league_id();
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return kbo_league_role_main_league_id();
    }

    uint32_t league_ids[128] = {0};
    int32_t counts[128] = {0};
    int32_t seen = 0;

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        if (((uint8_t*)team_ptr)[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }

        uint32_t league_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0 || league_id > 2000000u) {
            continue;
        }

        int index = -1;
        for (int j = 0; j < seen; j++) {
            if (league_ids[j] == league_id) {
                index = j;
                break;
            }
        }
        if (index < 0) {
            if (seen >= (int32_t)(sizeof(league_ids) / sizeof(league_ids[0]))) {
                continue;
            }
            league_ids[seen] = league_id;
            counts[seen] = 0;
            index = seen;
            seen++;
        }
        counts[index]++;
    }

    uint32_t current_year = kbo_find_current_kbo_league_year();
    uint32_t best_league_id = 0;
    int best_count = -1;
    for (int i = 0; i < seen; i++) {
        if (counts[i] <= 0) {
            continue;
        }

        uint32_t league_year = 0;
        if (current_year != 0) {
            league_year = kbo_find_league_year_from_id(league_ids[i]);
        }
        int score = counts[i];
        if (current_year != 0 && league_year == current_year) {
            score += 400;
        }
        if (score > best_count) {
            best_count = score;
            best_league_id = league_ids[i];
        }
    }

    if (best_league_id == 0 && seen > 0) {
        int fallback_count = counts[0];
        best_league_id = league_ids[0];
        for (int i = 1; i < seen; i++) {
            if (counts[i] > fallback_count) {
                fallback_count = counts[i];
                best_league_id = league_ids[i];
            }
        }
    }

    if (best_league_id == 0) {
        best_league_id = kbo_league_role_main_league_id();
    }

    cached_league_id = best_league_id;
    kbo_log_runtimef("KBO league id resolved current_year=%u team_count=%d selected=%u",
        current_year, team_count, best_league_id);
    return best_league_id;
}
