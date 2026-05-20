#include "state_team_vector.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"

int kbo_hub_get_team_vector(uintptr_t* out_vector, int32_t* out_count)
{
    if (out_vector == NULL || out_count == NULL) {
        return 0;
    }
    *out_vector = 0;
    *out_count = 0;

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

    *out_vector = team_vector;
    *out_count = team_count;
    return 1;
}
