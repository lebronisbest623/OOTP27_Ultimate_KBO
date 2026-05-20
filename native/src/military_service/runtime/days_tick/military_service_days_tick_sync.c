#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "military_service_days_tick.h"
#include "military_service_tick.h"

static uint32_t kbo_military_days_tick_sync_serial_from_date(uint32_t date)
{
    return kbo_date_serial(
        date / 10000u,
        (date / 100u) % 100u,
        date % 100u);
}

static int kbo_military_days_tick_ready_for_work(const KboCurrentDateTickWork* work)
{
    if (work == NULL || kbo_military_days_tick_sync_serial_from_date(work->date) == 0u) {
        return 0;
    }
    if (!kbo_fix_enabled() || get_ootp_cached_global_database() == 0u) {
        return 0;
    }
    if (kbo_runtime_save_in_progress()) {
        return 0;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || player_vector == 0u
            || player_count <= 0
            || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT) {
        return 0;
    }
    return 1;
}

int kbo_military_days_tick_sync_consumer(uint32_t date, uint32_t site_rva, void* context)
{
    (void)context;
    KboCurrentDateTickWork work = {
        .date = date,
        .event_date = date,
        .site_rva = site_rva,
        .sequence = 0u,
        .missed_events = 0u,
        .gap = 0
    };
    if (!kbo_military_days_tick_ready_for_work(&work)) {
        return 0;
    }

    const char* source = site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA
        ? "military_days_tick_sync_save_enter"
        : "military_days_tick_sync_post_advance";
    kbo_tick_military_service_days_for_date(date, source, NULL);
    return !kbo_runtime_save_in_progress();
}
