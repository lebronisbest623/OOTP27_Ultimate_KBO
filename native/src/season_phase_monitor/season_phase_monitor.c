#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../bootstrap/abi/ootp_offsets.h"
#include "../bootstrap/profiling/profiler.h"
#include "../core/dates/core_current_date.h"
#include "../core/dates/tick/current_date_tick_capture.h"
#include "../core/core_flags/api/flags_api.h"
#include "../core/core_league_context_parts/api/league_context_lookup.h"
#include "../core/logging/core_log.h"
#include "../core/runtime_tuning/runtime_tuning_policy.h"
#include "../runtime_memory/runtime_memory.h"
#include "season_phase_monitor.h"
/* League season-phase diagnostics. Included from native/KBOFix.c. */
static volatile LONG g_kbo_season_phase_monitor_started = 0;
static const char* kbo_league_phase_label(uint8_t phase)
{
    switch (phase) {
    case 0: return "phase0_offseason_or_reset";
    case 1: return "phase1_offseason_started";
    case 2: return "phase2_preseason_or_spring";
    case 3: return "phase3_regular_season";
    case 4: return "phase4_postseason_or_transition";
    default: return "phase_unknown";
    }
}
__declspec(noinline) void ootp_kbo_season_phase_write_probe(
    uintptr_t league_ptr,
    uint32_t value,
    uint32_t site_rva)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (league_ptr == 0 || !memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET), sizeof(uint8_t))) {
        kbo_log_runtimef("KBO season phase write probe site=0x%x league=%p value=%u reason=bad_league", site_rva, (void*)league_ptr, value);
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "season.phase_write");
    }
    uint8_t old_value = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET) = (uint8_t)value;
    uint8_t new_value = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);

    uint32_t date_key = (uint32_t)InterlockedCompareExchange(
        &g_kbo_current_date_tick_last_published_date,
        0,
        0);

    uint32_t league_id_a = 0;
    uint32_t league_id_b = 0;
    uint32_t league_year = 0;
    if (memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET), sizeof(uint32_t))) {
        league_id_a = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET);
    }
    if (memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET + 8u), sizeof(uint32_t))) {
        league_id_b = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
    }
    if (memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        league_year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    }

#define KBO_SEASON_PHASE_PROBE_OFFSET(index) (OOTP27_SEASON_PHASE_PROBE_BASE_OFFSET + ((uint32_t)(index) * sizeof(uint32_t)))

    kbo_log_runtimef(
        "KBO season phase write site=0x%x league=%p ids=%u/%u date=%u league_year=%u old=%u new=%u label=%s",
        site_rva,
        (void*)league_ptr,
        league_id_a,
        league_id_b,
        date_key,
        league_year,
        (unsigned)old_value,
        (unsigned)new_value,
        kbo_league_phase_label(new_value));
    KBO_HOOK_PROFILE_END(profile_hook, "season.phase_write");
}

static void kbo_log_season_phase_league_candidates(uint32_t league_id)
{
    static volatile LONG logged = 0;

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        kbo_log_runtimef("KBO season phase candidates unavailable league_id=%u reason=no_global", league_id);
        return;
    }

    if (InterlockedCompareExchange(&logged, 1, 0) != 0) {
        return;
    }

    static const uint32_t league_vec_offsets[] = {
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0xc0u, 0xc8u, 0xd0u, 0xd8u,
        0xe0u, 0xe8u, 0xf0u, 0xf8u, 0x100u, 0x108u, 0x110u, 0x118u, 0x120u, 0x128u
    };

    int logged_rows = 0;
    for (size_t i = 0; i < sizeof(league_vec_offsets) / sizeof(league_vec_offsets[0]); i++) {
        uint32_t vec_off = league_vec_offsets[i];
        uint32_t cnt_off = vec_off + 8u;
        if (!memory_range_readable((void*)(global + vec_off), 16u)) {
            continue;
        }

        uintptr_t candidate_vec = *(uintptr_t*)(global + vec_off);
        int32_t candidate_count = *(int32_t*)(global + cnt_off);
        if (candidate_vec == 0 || candidate_count <= 0 || candidate_count > 10000
                || !memory_range_readable((void*)candidate_vec, (SIZE_T)candidate_count * sizeof(uintptr_t))) {
            continue;
        }

        kbo_log_runtimef(
            "KBO season phase candidate vector league_id=%u vec_off=0x%x vector=%p count=%d",
            league_id,
            vec_off,
            (void*)candidate_vec,
            candidate_count);

        for (int32_t j = 0; j < candidate_count && logged_rows < 40; j++) {
            uintptr_t candidate = *(uintptr_t*)(candidate_vec + (uintptr_t)j * sizeof(uintptr_t));
            if (candidate == 0
                    || !memory_range_readable((void*)candidate, OOTP27_SEASON_PHASE_CANDIDATE_READABLE_BYTES)) {
                continue;
            }

            uint32_t id_4cc0 = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_ID_OFFSET);
            uint32_t id_4cc4 = *(uint32_t*)(candidate + OOTP27_SEASON_PHASE_ALT_ID_A_OFFSET);
            uint32_t id_4cc8 = *(uint32_t*)(candidate + OOTP27_SEASON_PHASE_ALT_ID_B_OFFSET);
            uint32_t id_4ccc = *(uint32_t*)(candidate + OOTP27_SEASON_PHASE_ALT_ID_C_OFFSET);
            uint32_t league_year = 0;
            uint8_t phase = 0xffu;
            uint32_t phase_year = 0;

            if (memory_range_readable((void*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
                league_year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
            }
            if (memory_range_readable((void*)(candidate + OOTP27_KBO_LEAGUE_PHASE_OFFSET), sizeof(uint8_t))) {
                phase = *(uint8_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
            }
            if (memory_range_readable((void*)(candidate + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET), sizeof(uint32_t))) {
                phase_year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET);
            }

            kbo_log_runtimef(
                "KBO season phase candidate row vec_off=0x%x index=%d ptr=%p ids={4cc0:%u,4cc4:%u,4cc8:%u,4ccc:%u} league_year=%u phase=%u phase_year=%u",
                vec_off,
                j,
                (void*)candidate,
                id_4cc0,
                id_4cc4,
                id_4cc8,
                id_4ccc,
                league_year,
                (unsigned)phase,
                phase_year);
            logged_rows++;
        }
    }

    if (logged_rows == 0) {
        kbo_log_runtimef("KBO season phase candidates empty league_id=%u", league_id);
    }
}

static void kbo_log_season_phase_probe_window(uintptr_t league_ptr, uint32_t date_key)
{
    static uintptr_t last_ptr = 0;
    static uint32_t last_date_key = 0;

    if (league_ptr == 0 || date_key == 0) {
        return;
    }
    if (league_ptr == last_ptr && date_key == last_date_key) {
        return;
    }
    last_ptr = league_ptr;
    last_date_key = date_key;

    if (!memory_range_readable(
            (void*)(league_ptr + OOTP27_SEASON_PHASE_PROBE_BASE_OFFSET),
            OOTP27_SEASON_PHASE_PROBE_BYTES)) {
        return;
    }

    kbo_log_runtimef(
        "KBO season phase probe league=%p date=%u "
        "44d0=%u/%u/%u 44d4=%u/%u/%u 44d8=%u/%u/%u 44dc=%u/%u/%u "
        "44e0=%u/%u/%u 44e4=%u/%u/%u 44e8=%u/%u/%u 44ec=%u/%u/%u "
        "44f0=%u/%u/%u 44f4=%u/%u/%u 44f8=%u/%u/%u 44fc=%u/%u/%u "
        "4500=%u/%u/%u 4504=%u/%u/%u 4508=%u/%u/%u 450c=%u/%u/%u "
        "4510=%u/%u/%u 4514=%u/%u/%u 4518=%u/%u/%u 451c=%u/%u/%u",
        (void*)league_ptr,
        date_key,
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(0)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(0)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(0)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(1)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(1)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(1)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(2)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(2)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(2)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(3)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(3)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(3)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(4)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(4)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(4)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(5)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(5)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(5)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(6)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(6)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(6)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(7)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(7)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(7)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(8)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(8)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(8)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(9)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(9)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(9)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(10)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(10)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(10)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(11)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(11)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(11)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(12)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(12)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(12)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(13)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(13)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(13)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(14)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(14)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(14)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(15)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(15)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(15)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(16)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(16)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(16)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(17)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(17)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(17)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(18)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(18)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(18)),
        (unsigned)*(uint8_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(19)), (unsigned)*(uint16_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(19)), *(uint32_t*)(league_ptr + KBO_SEASON_PHASE_PROBE_OFFSET(19)));

#undef KBO_SEASON_PHASE_PROBE_OFFSET
}

static void kbo_log_season_phase_changed_words(uintptr_t league_ptr, uint32_t date_key)
{
    enum { BASE_OFFSET = 0x4000u, END_OFFSET = 0x4d00u, WORD_COUNT = (END_OFFSET - BASE_OFFSET) / 4u };
    static uintptr_t last_ptr = 0;
    static uint32_t last_date_key = 0;
    static uint32_t last_words[WORD_COUNT];
    static int has_snapshot = 0;

    if (league_ptr == 0 || date_key == 0) {
        return;
    }
    if (!memory_range_readable((void*)(league_ptr + BASE_OFFSET), END_OFFSET - BASE_OFFSET)) {
        has_snapshot = 0;
        last_ptr = 0;
        return;
    }
    if (league_ptr != last_ptr || !has_snapshot) {
        for (uint32_t i = 0; i < WORD_COUNT; i++) {
            last_words[i] = *(uint32_t*)(league_ptr + BASE_OFFSET + i * 4u);
        }
        last_ptr = league_ptr;
        last_date_key = date_key;
        has_snapshot = 1;
        kbo_log_runtimef("KBO season phase diff baseline league=%p date=%u range=0x%x-0x%x", (void*)league_ptr, date_key, BASE_OFFSET, END_OFFSET);
        return;
    }
    if (date_key == last_date_key) {
        return;
    }

    char changes[1600];
    size_t used = 0;
    int count = 0;
    int truncated = 0;
    changes[0] = '\0';

    for (uint32_t i = 0; i < WORD_COUNT; i++) {
        uint32_t offset = BASE_OFFSET + i * 4u;
        uint32_t old_value = last_words[i];
        uint32_t new_value = *(uint32_t*)(league_ptr + offset);
        if (old_value == new_value) {
            continue;
        }

        count++;
        if (used + 64u < sizeof(changes)) {
            int wrote = snprintf(
                changes + used,
                sizeof(changes) - used,
                "%s%04x:%u->%u",
                used == 0 ? "" : " ",
                offset,
                old_value,
                new_value);
            if (wrote > 0) {
                used += (size_t)wrote;
            }
        } else {
            truncated = 1;
        }
        last_words[i] = new_value;
    }

    if (count > 0) {
        kbo_log_runtimef(
            "KBO season phase diff league=%p date=%u prev_date=%u count=%d%s changes=%s",
            (void*)league_ptr,
            date_key,
            last_date_key,
            count,
            truncated ? " truncated=1" : "",
            changes);
    }
    last_date_key = date_key;
}

static DWORD WINAPI kbo_season_phase_monitor_thread(LPVOID parameter)
{
    (void)parameter;

    uint32_t last_date = 0xffffffffu;
    uint32_t last_league_id = 0xffffffffu;
    uint32_t last_league_year = 0xffffffffu;
    uint32_t last_phase_year = 0xffffffffu;
    uint8_t last_phase = 0xffu;
    uintptr_t last_league_ptr = 0;
    KboCurrentDateTickConsumer date_consumer = {0};
    kbo_current_date_tick_consumer_init(
        &date_consumer,
        "season_phase_monitor",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    kbo_log_runtime_line("KBO season phase monitor started");

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->season_phase_monitor_sleep_ms)) {
            break;
        }

        if (!kbo_fix_enabled()) {
            continue;
        }

        KBO_PROFILE_BEGIN(profile_season_phase_monitor_tick);
        KboCurrentDateTickWork date_work = {0};
        if (!kbo_current_date_tick_consumer_next(&date_consumer, &date_work)) {
            if (last_date != 0u) {
                kbo_log_runtime_line("KBO season phase monitor waiting reason=current_date_hook_unavailable");
            }
            last_league_ptr = 0;
            last_date = 0u;
            KBO_PROFILE_END(profile_season_phase_monitor_tick, "season_phase.monitor.no_date");
            continue;
        }
        uint32_t date_key = date_work.date;
        uint32_t year = date_key / 10000u;
        uint32_t month = (date_key / 100u) % 100u;
        uint32_t day = date_key % 100u;

        uint32_t league_id = kbo_resolve_kbo_league_id();
        uintptr_t league_ptr = kbo_find_league_ptr_from_id(league_id);
        if (league_ptr == 0
                || !memory_range_readable((void*)league_ptr, OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET + sizeof(uint32_t))) {
            kbo_log_season_phase_league_candidates(league_id);
            if (last_league_ptr != 0 || last_league_id != league_id) {
                kbo_log_runtimef(
                    "KBO season phase monitor waiting league_id=%u reason=league_unavailable",
                    league_id);
            }
            last_league_ptr = 0;
            last_league_id = league_id;
            KBO_PROFILE_END(profile_season_phase_monitor_tick, "season_phase.monitor.no_league");
            continue;
        }

        uint32_t league_year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        uint8_t phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
        uint32_t phase_year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET);
        kbo_log_season_phase_probe_window(league_ptr, date_key);
        kbo_log_season_phase_changed_words(league_ptr, date_key);

        if (league_ptr != last_league_ptr
                || league_id != last_league_id
                || date_key != last_date
                || league_year != last_league_year
                || phase != last_phase
                || phase_year != last_phase_year) {
            kbo_log_runtimef(
                "KBO season phase league=%p league_id=%u date=%04u-%02u-%02u league_year=%u phase=%u label=%s phase_year=%u",
                (void*)league_ptr,
                league_id,
                year,
                month,
                day,
                league_year,
                (unsigned)phase,
                kbo_league_phase_label(phase),
                phase_year);

            last_league_ptr = league_ptr;
            last_league_id = league_id;
            last_date = date_key;
            last_league_year = league_year;
            last_phase = phase;
            last_phase_year = phase_year;
        }
        kbo_current_date_tick_consumer_mark_processed(&date_consumer);
        KBO_PROFILE_END(profile_season_phase_monitor_tick, "season_phase.monitor.tick");
    }
    kbo_log_runtime_line("KBO season phase monitor stopped");
    InterlockedExchange(&g_kbo_season_phase_monitor_started, 0);

    return 0;
}

int start_kbo_season_phase_monitor(void)
{
    if (InterlockedCompareExchange(&g_kbo_season_phase_monitor_started, 1, 0) != 0) {
        return 1;
    }

    if (!kbo_start_runtime_thread(kbo_season_phase_monitor_thread, NULL, "season phase monitor")) {
        InterlockedExchange(&g_kbo_season_phase_monitor_started, 0);
        return 0;
    }

    return 1;
}

