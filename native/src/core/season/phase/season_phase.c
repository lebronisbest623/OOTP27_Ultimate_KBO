#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "season_phase.h"
#include "capture/season_phase_capture.h"

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../core_league_context_parts/api/league_context_lookup.h"
#include "../season_calendar.h"
#include "../../dates/constants/kbo_date_constants.h"

static void kbo_season_phase_init_info(KboSeasonPhaseInfo* info)
{
    if (info == NULL) {
        return;
    }
    info->date = 0u;
    info->league_id = 0u;
    info->league_ptr = 0u;
    info->league_year = 0u;
    info->raw_phase = KBO_SEASON_PHASE_UNKNOWN;
    info->raw_phase_year = 0u;
    info->captured_phase = KBO_SEASON_PHASE_UNKNOWN;
    info->capture_raw_phase_after_write = KBO_SEASON_PHASE_UNKNOWN;
    info->capture_phase_year = 0u;
    info->capture_site_rva = 0u;
    info->capture_date = 0u;
    info->capture_sequence = 0u;
    info->effective_phase = KBO_SEASON_PHASE_UNKNOWN;
    info->opening_day = 0u;
    info->known_offseason_start = 0u;
    info->raw_valid = 0;
    info->capture_valid = 0;
    info->capture_used = 0;
    info->opening_day_valid = 0;
    info->corrected = 0;
}

int kbo_season_phase_read_raw(uint32_t league_id, KboSeasonPhaseInfo* out_info)
{
    kbo_season_phase_init_info(out_info);
    if (league_id == 0u || out_info == NULL) {
        return 0;
    }

    uintptr_t league_ptr = kbo_find_league_ptr_from_id(league_id);
    if (league_ptr == 0u
            || !memory_range_readable(
                (void*)league_ptr,
                OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET + sizeof(uint32_t))) {
        out_info->league_id = league_id;
        return 0;
    }

    uint32_t league_year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    uint8_t raw_phase = *(uint8_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    uint32_t raw_phase_year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET);
    if (league_year < KBO_SEASON_YEAR_MIN || league_year > KBO_SIM_YEAR_MAX || raw_phase > KBO_SEASON_PHASE_POSTSEASON) {
        out_info->league_id = league_id;
        out_info->league_ptr = league_ptr;
        out_info->league_year = league_year;
        out_info->raw_phase = raw_phase;
        out_info->raw_phase_year = raw_phase_year;
        return 0;
    }

    out_info->league_id = league_id;
    out_info->league_ptr = league_ptr;
    out_info->league_year = league_year;
    out_info->raw_phase = raw_phase;
    out_info->raw_phase_year = raw_phase_year;
    out_info->raw_valid = 1;
    return 1;
}

static uint32_t kbo_season_phase_schedule_year(uint32_t today, uint32_t league_year)
{
    uint32_t date_year = today / 10000u;
    uint32_t month_day = today % 10000u;
    if (league_year < KBO_SEASON_YEAR_MIN || league_year > KBO_SIM_YEAR_MAX) {
        return date_year;
    }
    if (date_year > league_year && month_day <= 701u) {
        return date_year;
    }
    return league_year;
}

static int kbo_season_phase_resolve_opening_day(
    const KboSeasonPhaseInfo* raw_info,
    uint32_t today,
    uint32_t* out_opening_day)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (raw_info == NULL || out_opening_day == NULL || today == 0u) {
        return 0;
    }

    uint32_t schedule_year = kbo_season_phase_schedule_year(today, raw_info->league_year);
    return kbo_season_calendar_resolve_opening_day_with_league_ptr(
        raw_info->league_id,
        schedule_year,
        today,
        raw_info->league_ptr,
        "season_phase_resolve",
        out_opening_day);
}

int kbo_season_phase_resolve(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    uint32_t known_offseason_start_yyyymmdd,
    KboSeasonPhaseInfo* out_info)
{
    KboSeasonPhaseInfo info;
    kbo_season_phase_init_info(&info);
    info.date = today_yyyymmdd;
    info.league_id = league_id;
    info.known_offseason_start = known_offseason_start_yyyymmdd;

    KboSeasonPhaseInfo raw_info;
    if (kbo_season_phase_read_raw(league_id, &raw_info)) {
        info = raw_info;
        info.date = today_yyyymmdd;
        info.known_offseason_start = known_offseason_start_yyyymmdd;
    } else if (league_id != 0u) {
        info.league_id = league_id;
        info.raw_phase = raw_info.raw_phase;
        info.raw_phase_year = raw_info.raw_phase_year;
        info.league_ptr = raw_info.league_ptr;
        info.league_year = raw_info.league_year;
    }

    KboSeasonPhaseCaptureSnapshot capture = {0};
    if (kbo_season_phase_capture_latest(league_id, today_yyyymmdd, &capture)) {
        info.capture_valid = 1;
        info.captured_phase = capture.phase;
        info.capture_raw_phase_after_write = capture.raw_phase_after_write;
        info.capture_phase_year = capture.phase_year;
        info.capture_site_rva = capture.site_rva;
        info.capture_date = capture.date;
        info.capture_sequence = capture.sequence;

        if (!info.raw_valid || info.league_ptr == 0u || info.league_ptr != capture.league_ptr) {
            info.league_ptr = capture.league_ptr;
            info.league_year = capture.league_year;
            info.raw_phase = capture.raw_phase_after_write;
            info.raw_phase_year = capture.phase_year;
            info.raw_valid = 1;
        }
    }

    uint32_t opening_day = 0u;
    if (kbo_season_phase_resolve_opening_day(&info, today_yyyymmdd, &opening_day)) {
        info.opening_day = opening_day;
        info.opening_day_valid = 1;
    }

    uint8_t phase_source = info.raw_phase;
    if (info.capture_valid) {
        phase_source = info.captured_phase;
        info.capture_used = 1;
    }

    int source_corrected = 0;
    info.effective_phase = kbo_season_phase_effective_from_values(
        today_yyyymmdd,
        info.league_year,
        phase_source,
        info.opening_day,
        known_offseason_start_yyyymmdd,
        &source_corrected);
    info.corrected = source_corrected
        || (info.capture_used && (!info.raw_valid || info.raw_phase != info.captured_phase));

    if (out_info != NULL) {
        *out_info = info;
    }
    return info.effective_phase != KBO_SEASON_PHASE_UNKNOWN;
}
