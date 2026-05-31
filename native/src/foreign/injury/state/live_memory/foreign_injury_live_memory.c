#include "../../internal/foreign_injury_internal.h"

#define KBO_FOREIGN_INJURY_LIVE_MAX_OBJECTS 16
#define KBO_FOREIGN_INJURY_CAREER_ENDING_DAYS 365

int kbo_foreign_injury_read_live_memory(uint8_t* player, KboForeignInjuryLiveMemory* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (player == NULL || out == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    out->career_ending = player[OOTP27_PLAYER_CAREER_ENDING_INJURY_OFFSET] != 0u ? 1u : 0u;
    int32_t active_count = *(int32_t*)(player + OOTP27_PLAYER_ACTIVE_INJURY_COUNT_OFFSET);
    out->active_count = active_count;
    if (active_count <= 0) {
        return 1;
    }

    out->active = 1u;
    if (active_count > 64) {
        return 1;
    }

    uintptr_t vector = *(uintptr_t*)(player + OOTP27_PLAYER_ACTIVE_INJURY_VECTOR_OFFSET);
    int32_t scan_count = active_count < KBO_FOREIGN_INJURY_LIVE_MAX_OBJECTS
        ? active_count
        : KBO_FOREIGN_INJURY_LIVE_MAX_OBJECTS;
    if (vector == 0u || !memory_range_readable((void*)vector, (size_t)scan_count * sizeof(uintptr_t))) {
        return 1;
    }

    int selected = 0;
    int32_t best_days_left = 0;
    for (int32_t i = 0; i < scan_count; i++) {
        uintptr_t injury_object = *(uintptr_t*)(vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (injury_object == 0u
                || !memory_range_readable((void*)injury_object, OOTP27_PLAYER_INJURY_OBJECT_READABLE_BYTES)) {
            continue;
        }

        uint8_t pending = *(uint8_t*)(injury_object + OOTP27_PLAYER_INJURY_OBJECT_PENDING_DIAGNOSIS_OFFSET);
        uint8_t day_to_day = *(uint8_t*)(injury_object + OOTP27_PLAYER_INJURY_OBJECT_DAY_TO_DAY_OFFSET);
        if (pending != 0u) {
            out->pending_diagnosis = 1u;
        }
        if (day_to_day != 0u) {
            out->day_to_day = 1u;
        }
        if (pending != 0u || day_to_day != 0u) {
            continue;
        }

        int32_t days_left = *(int32_t*)(injury_object + OOTP27_PLAYER_INJURY_OBJECT_DAYS_LEFT_OFFSET);
        int32_t total_days = *(int32_t*)(injury_object + OOTP27_PLAYER_INJURY_OBJECT_TOTAL_DAYS_OFFSET);
        if (!selected || days_left > best_days_left) {
            selected = 1;
            best_days_left = days_left;
            out->injury_object = injury_object;
            out->injury_id = *(uint32_t*)(injury_object + OOTP27_PLAYER_INJURY_OBJECT_ID_OFFSET);
            out->days_left = days_left;
            out->total_days = total_days;
        }
    }

    if (selected && out->career_ending != 0u && out->days_left < KBO_FOREIGN_INJURY_CAREER_ENDING_DAYS) {
        out->days_left = KBO_FOREIGN_INJURY_CAREER_ENDING_DAYS;
        if (out->total_days < out->days_left) {
            out->total_days = out->days_left;
        }
    }
    return 1;
}
