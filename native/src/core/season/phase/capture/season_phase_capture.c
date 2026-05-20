#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "season_phase_capture.h"

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../core_league_context_parts/api/league_context_lookup.h"
#include "../../../dates/core_text_date.h"
#include "../../../logging/core_log.h"
#include "../../../sync/lock.h"
#include "../../../dates/constants/kbo_date_constants.h"

volatile LONG g_kbo_season_phase_capture_event_write_cursor = 0;
volatile LONG g_kbo_season_phase_capture_event_published_sequence = 0;
volatile LONG g_kbo_season_phase_capture_event_consumed_sequence = 0;
uintptr_t g_kbo_season_phase_capture_event_league_ptrs[KBO_SEASON_PHASE_CAPTURE_EVENT_RING_SIZE];
uint32_t g_kbo_season_phase_capture_event_values[KBO_SEASON_PHASE_CAPTURE_EVENT_RING_SIZE];
uint32_t g_kbo_season_phase_capture_event_site_rvas[KBO_SEASON_PHASE_CAPTURE_EVENT_RING_SIZE];

static KboRwLock g_kbo_season_phase_capture_lock = KBO_RW_LOCK_INIT;
static KboSeasonPhaseCaptureSnapshot g_kbo_season_phase_latest_capture;
static int g_kbo_season_phase_latest_capture_valid = 0;

static int kbo_season_phase_capture_phase_known(uint32_t phase)
{
    return phase <= KBO_SEASON_PHASE_POSTSEASON;
}

static int kbo_season_phase_capture_year_plausible(uint32_t year)
{
    return year >= KBO_SEASON_YEAR_MIN && year <= KBO_SIM_YEAR_MAX;
}

static uint32_t kbo_season_phase_capture_date_serial(uint32_t yyyymmdd)
{
    if (yyyymmdd == 0u) {
        return 0u;
    }
    return kbo_date_serial(
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

static int kbo_season_phase_capture_league_matches(uintptr_t league_ptr, uint32_t league_id)
{
    if (league_ptr == 0u || league_id == 0u
            || !memory_range_readable((void*)league_ptr, OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET),
                sizeof(uint32_t))) {
        return 0;
    }

    uint32_t legacy_id = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET);
    uint32_t alternate_id = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
    if (legacy_id == league_id
            && kbo_league_candidate_matches_id(league_ptr, league_id, OOTP27_KBO_LEAGUE_ID_OFFSET)) {
        return 1;
    }
    if (alternate_id == league_id
            && kbo_league_candidate_matches_id(league_ptr, league_id, OOTP27_KBO_LEAGUE_ID_OFFSET + 8u)) {
        return 1;
    }
    return 0;
}

static int kbo_season_phase_capture_snapshot_still_usable(
    const KboSeasonPhaseCaptureSnapshot* snapshot,
    uint32_t league_id,
    uint32_t today_yyyymmdd)
{
    if (snapshot == NULL
            || snapshot->sequence == 0u
            || snapshot->league_id != league_id
            || !kbo_season_phase_capture_phase_known(snapshot->phase)
            || !kbo_season_phase_capture_year_plausible(snapshot->league_year)
            || !kbo_season_phase_capture_league_matches(snapshot->league_ptr, league_id)) {
        return 0;
    }

    uint32_t today_serial = kbo_season_phase_capture_date_serial(today_yyyymmdd);
    uint32_t event_serial = kbo_season_phase_capture_date_serial(snapshot->date);
    if (today_serial != 0u && event_serial != 0u && today_serial < event_serial) {
        return 0;
    }

    uint32_t date_year = today_yyyymmdd / 10000u;
    if (date_year >= KBO_SEASON_YEAR_MIN && date_year <= KBO_SIM_YEAR_MAX
            && (snapshot->league_year + 1u < date_year || snapshot->league_year > date_year + 1u)) {
        return 0;
    }

    return 1;
}

void kbo_season_phase_capture_drain(uint32_t league_id, uint32_t today_yyyymmdd)
{
    if (league_id == 0u) {
        return;
    }

    LONG latest = InterlockedCompareExchange(
        &g_kbo_season_phase_capture_event_published_sequence,
        0,
        0);
    LONG consumed = InterlockedCompareExchange(
        &g_kbo_season_phase_capture_event_consumed_sequence,
        0,
        0);
    if (latest == consumed) {
        return;
    }

    if (latest - consumed > (LONG)KBO_SEASON_PHASE_CAPTURE_EVENT_RING_SIZE) {
        kbo_log_runtimef(
            "KBO season phase capture ring overflow consumed=%ld latest=%ld",
            (long)consumed,
            (long)latest);
        consumed = latest - (LONG)KBO_SEASON_PHASE_CAPTURE_EVENT_RING_SIZE;
    }

    for (LONG event_no = consumed; event_no < latest; event_no++) {
        uint32_t index = (uint32_t)event_no & KBO_SEASON_PHASE_CAPTURE_EVENT_RING_MASK;
        uintptr_t league_ptr = g_kbo_season_phase_capture_event_league_ptrs[index];
        uint32_t value = g_kbo_season_phase_capture_event_values[index];
        uint32_t site_rva = g_kbo_season_phase_capture_event_site_rvas[index];
        if (!kbo_season_phase_capture_phase_known(value)
                || !kbo_season_phase_capture_league_matches(league_ptr, league_id)) {
            continue;
        }

        uint32_t league_year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        uint8_t raw_phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
        uint32_t phase_year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET);
        if (!kbo_season_phase_capture_year_plausible(league_year)
                || !kbo_season_phase_capture_phase_known(raw_phase)
                || (phase_year != 0u && !kbo_season_phase_capture_year_plausible(phase_year))) {
            continue;
        }

        KboSeasonPhaseCaptureSnapshot snapshot = {0};
        snapshot.sequence = (uint32_t)event_no + 1u;
        snapshot.date = today_yyyymmdd;
        snapshot.league_id = league_id;
        snapshot.league_ptr = league_ptr;
        snapshot.league_year = league_year;
        snapshot.phase = (uint8_t)value;
        snapshot.raw_phase_after_write = raw_phase;
        snapshot.phase_year = phase_year;
        snapshot.site_rva = site_rva;

        kbo_rw_lock_enter_exclusive(&g_kbo_season_phase_capture_lock);
        int log_change = !g_kbo_season_phase_latest_capture_valid
            || g_kbo_season_phase_latest_capture.league_ptr != snapshot.league_ptr
            || g_kbo_season_phase_latest_capture.phase != snapshot.phase
            || g_kbo_season_phase_latest_capture.site_rva != snapshot.site_rva
            || g_kbo_season_phase_latest_capture.date != snapshot.date;
        g_kbo_season_phase_latest_capture = snapshot;
        g_kbo_season_phase_latest_capture_valid = 1;
        kbo_rw_lock_leave_exclusive(&g_kbo_season_phase_capture_lock);

        if (log_change) {
            kbo_log_runtimef(
                "KBO season phase capture accepted site=0x%x league=%p league_id=%u date=%u league_year=%u phase=%u raw_after=%u phase_year=%u label=%s",
                site_rva,
                (void*)league_ptr,
                league_id,
                today_yyyymmdd,
                league_year,
                value,
                (unsigned)raw_phase,
                phase_year,
                kbo_season_phase_label((uint8_t)value));
        }
    }

    InterlockedExchange(&g_kbo_season_phase_capture_event_consumed_sequence, latest);
}

int kbo_season_phase_capture_latest(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    KboSeasonPhaseCaptureSnapshot* out_snapshot)
{
    if (out_snapshot != NULL) {
        *out_snapshot = (KboSeasonPhaseCaptureSnapshot){0};
    }
    if (league_id == 0u || out_snapshot == NULL) {
        return 0;
    }

    kbo_season_phase_capture_drain(league_id, today_yyyymmdd);

    KboSeasonPhaseCaptureSnapshot snapshot = {0};
    int valid = 0;
    kbo_rw_lock_enter_shared(&g_kbo_season_phase_capture_lock);
    if (g_kbo_season_phase_latest_capture_valid) {
        snapshot = g_kbo_season_phase_latest_capture;
        valid = 1;
    }
    kbo_rw_lock_leave_shared(&g_kbo_season_phase_capture_lock);

    if (!valid || !kbo_season_phase_capture_snapshot_still_usable(&snapshot, league_id, today_yyyymmdd)) {
        return 0;
    }

    *out_snapshot = snapshot;
    return 1;
}
