#include "core_team_collect.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"

/* Core league team collection helpers. */

int collect_kbo_league_team_ids(
    uint32_t league_id,
    uint32_t* team_ids,
    int max_team_ids,
    int* out_scanned,
    int* out_unreadable)
{
    if (out_scanned != NULL) {
        *out_scanned = 0;
    }
    if (out_unreadable != NULL) {
        *out_unreadable = 0;
    }
    if (league_id == 0 || team_ids == NULL || max_team_ids <= 0) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > KBO_RUNTIME_MAX_TEAM_VECTOR_COUNT
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    int found = 0;
    int unreadable = 0;
    for (int32_t i = 0; i < team_count && found < max_team_ids; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            unreadable++;
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id == 0 || team_id > KBO_RUNTIME_PLAUSIBLE_TEAM_ID_MAX) {
            continue;
        }

        int duplicate = 0;
        for (int j = 0; j < found; j++) {
            if (team_ids[j] == team_id) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) {
            team_ids[found++] = team_id;
        }
    }

    if (out_scanned != NULL) {
        *out_scanned = team_count;
    }
    if (out_unreadable != NULL) {
        *out_unreadable = unreadable;
    }
    return found;
}
