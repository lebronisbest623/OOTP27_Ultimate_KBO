#include "schedule_dates.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../core/dates/constants/kbo_date_constants.h"

static int kbo_allstar_schedule_memory_executable(const void* address)
{
    if (address == NULL) {
        return 0;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) {
        return 0;
    }

    DWORD protect = mbi.Protect & 0xffu;
    return protect == PAGE_EXECUTE
        || protect == PAGE_EXECUTE_READ
        || protect == PAGE_EXECUTE_READWRITE
        || protect == PAGE_EXECUTE_WRITECOPY;
}

static int kbo_allstar_date_slot_looks_like_serializer_callback(uint8_t* league, uint32_t offset, uintptr_t* out_slot, uintptr_t* out_callback)
{
    if (out_slot != NULL) {
        *out_slot = 0;
    }
    if (out_callback != NULL) {
        *out_callback = 0;
    }
    if (league == NULL || !memory_range_readable(league + offset, sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t slot = *(uintptr_t*)(league + offset);
    if (out_slot != NULL) {
        *out_slot = slot;
    }
    if (slot < 0x10000u || !memory_range_readable((void*)(slot + 0x10u), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t callback = *(uintptr_t*)(slot + 0x10u);
    if (out_callback != NULL) {
        *out_callback = callback;
    }
    return kbo_allstar_schedule_memory_executable((void*)callback);
}

int kbo_allstar_schedule_date_ready(uint8_t* league)
{
    if (league == NULL || !memory_range_readable(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET, 8u)) {
        return 0;
    }
    if (kbo_allstar_date_slot_looks_like_serializer_callback(league, OOTP27_ALLSTAR_DATE_YEAR_OFFSET, NULL, NULL)) {
        return 0;
    }

    uint16_t year = *(uint16_t*)(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET);
    uint8_t day = *(uint8_t*)(league + OOTP27_ALLSTAR_DATE_DAY_OFFSET);
    uint8_t month = *(uint8_t*)(league + OOTP27_ALLSTAR_DATE_MONTH_OFFSET);
    return year >= KBO_SEASON_YEAR_MIN && year <= KBO_SIM_YEAR_MAX && month >= 1u && month <= 12u && day >= 1u && day <= 31u;
}

int kbo_allstar_season_start_date_ready(uint8_t* league)
{
    if (league == NULL || !memory_range_readable(league + OOTP27_SEASON_START_DATE_YEAR_OFFSET, 8u)) {
        return 0;
    }
    if (kbo_allstar_date_slot_looks_like_serializer_callback(league, OOTP27_SEASON_START_DATE_YEAR_OFFSET, NULL, NULL)) {
        return 0;
    }

    uint16_t year = *(uint16_t*)(league + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
    uint8_t day = *(uint8_t*)(league + OOTP27_SEASON_START_DATE_DAY_OFFSET);
    uint8_t month = *(uint8_t*)(league + OOTP27_SEASON_START_DATE_MONTH_OFFSET);
    return year >= KBO_SEASON_YEAR_MIN && year <= KBO_SIM_YEAR_MAX && month >= 1u && month <= 12u && day >= 1u && day <= 31u;
}

int kbo_allstar_load_schedule_dates(
    uint32_t league_year,
    KboAllstarScheduleDate* out_start,
    KboAllstarScheduleDate* out_allstar,
    char* out_path,
    size_t out_path_size);

static int kbo_allstar_year_plausible(uint32_t year)
{
    return year >= KBO_SEASON_YEAR_MIN && year <= KBO_SIM_YEAR_MAX;
}

static uint32_t kbo_allstar_read_league_year_candidate(uintptr_t league_ptr)
{
    if (league_ptr == 0
            || !memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        return 0u;
    }

    uint32_t year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    return kbo_allstar_year_plausible(year) ? year : 0u;
}

static uint32_t kbo_allstar_resolve_schedule_year(uintptr_t league_ptr, const char* source)
{
    uint32_t year = kbo_allstar_read_league_year_candidate(league_ptr);
    if (year != 0u) {
        return year;
    }

    uintptr_t plus8 = league_ptr + 8u;
    year = kbo_allstar_read_league_year_candidate(plus8);
    if (year != 0u) {
        static volatile LONG s_plus8_log_count = 0;
        LONG log_index = InterlockedIncrement(&s_plus8_log_count);
        if (log_index <= 40) {
            kbo_log_runtimef(
                "KBO all-star schedule year resolved from raw+8 source=%s raw=%p year=%u",
                source != NULL ? source : "",
                (void*)league_ptr,
                year);
        }
        return year;
    }

    if (league_ptr > 8u) {
        uintptr_t minus8 = league_ptr - 8u;
        year = kbo_allstar_read_league_year_candidate(minus8);
        if (year != 0u) {
            static volatile LONG s_minus8_log_count = 0;
            LONG log_index = InterlockedIncrement(&s_minus8_log_count);
            if (log_index <= 20) {
                kbo_log_runtimef(
                    "KBO all-star schedule year resolved from raw-8 source=%s raw=%p year=%u",
                    source != NULL ? source : "",
                    (void*)league_ptr,
                    year);
            }
            return year;
        }
    }

    return 0u;
}

int seed_kbo_allstar_schedule_dates(uintptr_t league_ptr, const char* source)
{
    uint8_t* league = (uint8_t*)league_ptr;
    if (league == NULL || !memory_range_readable(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET, 16u)) {
        return 0;
    }

    uint32_t league_year = kbo_allstar_resolve_schedule_year(league_ptr, source);
    KboAllstarScheduleDate start = {0};
    KboAllstarScheduleDate allstar = {0};
    char schedule_path[MAX_PATH] = {0};
    if (!kbo_allstar_load_schedule_dates(league_year, &start, &allstar, schedule_path, sizeof(schedule_path))) {
        kbo_log_runtimef(
            "KBO all-star native events date seed skipped source=%s league=%p year=%u reason=schedule_unavailable",
            source != NULL ? source : "",
            league,
            league_year);
        return 0;
    }

    uintptr_t season_slot = 0;
    uintptr_t season_callback = 0;
    if (kbo_allstar_date_slot_looks_like_serializer_callback(
            league,
            OOTP27_SEASON_START_DATE_YEAR_OFFSET,
            &season_slot,
            &season_callback)) {
        static volatile LONG s_unsafe_skip_log_count = 0;
        LONG log_index = InterlockedIncrement(&s_unsafe_skip_log_count);
        if (log_index <= 80) {
            kbo_log_runtimef(
                "KBO all-star schedule dates skipped source=%s league=%p offset=0x%x slot=%p callback=%p reason=target_slot_is_serializer_callback",
                source != NULL ? source : "",
                league,
                OOTP27_SEASON_START_DATE_YEAR_OFFSET,
                (void*)season_slot,
                (void*)season_callback);
        }
        return 0;
    }

    uintptr_t allstar_slot = 0;
    uintptr_t allstar_callback = 0;
    if (kbo_allstar_date_slot_looks_like_serializer_callback(
            league,
            OOTP27_ALLSTAR_DATE_YEAR_OFFSET,
            &allstar_slot,
            &allstar_callback)) {
        static volatile LONG s_unsafe_allstar_skip_log_count = 0;
        LONG log_index = InterlockedIncrement(&s_unsafe_allstar_skip_log_count);
        if (log_index <= 80) {
            kbo_log_runtimef(
                "KBO all-star schedule dates skipped source=%s league=%p offset=0x%x slot=%p callback=%p reason=target_slot_is_serializer_callback",
                source != NULL ? source : "",
                league,
                OOTP27_ALLSTAR_DATE_YEAR_OFFSET,
                (void*)allstar_slot,
                (void*)allstar_callback);
        }
        return 0;
    }

    uint16_t old_start_year = *(uint16_t*)(league + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
    uint8_t old_start_day = league[OOTP27_SEASON_START_DATE_DAY_OFFSET];
    uint8_t old_start_month = league[OOTP27_SEASON_START_DATE_MONTH_OFFSET];
    uint16_t old_allstar_year = *(uint16_t*)(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET);
    uint8_t old_allstar_day = league[OOTP27_ALLSTAR_DATE_DAY_OFFSET];
    uint8_t old_allstar_month = league[OOTP27_ALLSTAR_DATE_MONTH_OFFSET];
    uint8_t old_allstar_hour = league[OOTP27_ALLSTAR_DATE_HOUR_OFFSET];
    uint8_t old_allstar_minute = league[OOTP27_ALLSTAR_DATE_MIN_OFFSET];

    int changed = old_start_year != start.year
        || old_start_month != start.month
        || old_start_day != start.day
        || old_allstar_year != allstar.year
        || old_allstar_month != allstar.month
        || old_allstar_day != allstar.day
        || old_allstar_hour != allstar.hour
        || old_allstar_minute != allstar.minute;

    *(uint16_t*)(league + OOTP27_SEASON_START_DATE_YEAR_OFFSET) = start.year;
    league[OOTP27_SEASON_START_DATE_DAY_OFFSET] = start.day;
    league[OOTP27_SEASON_START_DATE_MONTH_OFFSET] = start.month;
    league[OOTP27_SEASON_START_DATE_HOUR_OFFSET] = start.hour;
    league[OOTP27_SEASON_START_DATE_MIN_OFFSET] = start.minute;
    league[OOTP27_SEASON_START_DATE_SEC_OFFSET] = start.second;

    *(uint16_t*)(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET) = allstar.year;
    league[OOTP27_ALLSTAR_DATE_DAY_OFFSET] = allstar.day;
    league[OOTP27_ALLSTAR_DATE_MONTH_OFFSET] = allstar.month;
    league[OOTP27_ALLSTAR_DATE_HOUR_OFFSET] = allstar.hour;
    league[OOTP27_ALLSTAR_DATE_MIN_OFFSET] = allstar.minute;
    league[OOTP27_ALLSTAR_DATE_SEC_OFFSET] = allstar.second;

    static volatile LONG s_log_count = 0;
    LONG log_index = InterlockedIncrement(&s_log_count);
    if (changed || log_index <= 80) {
        kbo_log_runtimef(
            "KBO all-star schedule dates seeded source=%s league=%p changed=%d start=%04u-%02u-%02u allstar=%04u-%02u-%02u %02u:%02u path=%s",
            source != NULL ? source : "",
            league,
            changed,
            (unsigned)start.year,
            (unsigned)start.month,
            (unsigned)start.day,
            (unsigned)allstar.year,
            (unsigned)allstar.month,
            (unsigned)allstar.day,
            (unsigned)allstar.hour,
            (unsigned)allstar.minute,
            schedule_path);
    }
    return 1;
}
