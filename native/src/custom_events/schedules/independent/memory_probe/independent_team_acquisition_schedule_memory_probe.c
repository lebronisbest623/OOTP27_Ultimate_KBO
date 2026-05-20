#include "../independent_team_acquisition_schedule_module.h"

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../../runtime_memory/runtime_memory.h"

static int kbo_independent_team_acquisition_memory_executable(const void* address)
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

static int kbo_independent_team_acquisition_date_slot_looks_like_serializer_callback(
    uintptr_t league_ptr,
    uintptr_t* out_slot,
    uintptr_t* out_callback)
{
    if (out_slot != NULL) {
        *out_slot = 0u;
    }
    if (out_callback != NULL) {
        *out_callback = 0u;
    }
    if (league_ptr == 0u
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET),
                sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t slot = *(uintptr_t*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
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
    return kbo_independent_team_acquisition_memory_executable((void*)callback);
}

uint32_t kbo_independent_team_acquisition_add_months(
    uint32_t yyyymmdd,
    uint32_t months)
{
    uint32_t result = yyyymmdd;
    for (uint32_t i = 0u; i < months; i++) {
        result = kbo_add_one_month_yyyymmdd(result);
        if (result == 0u) {
            return 0u;
        }
    }
    return result;
}

static int kbo_independent_team_acquisition_start_date_plausible(
    uint32_t yyyymmdd,
    uint32_t expected_year)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year != expected_year
            || month < 2u
            || month > 5u
            || day < 1u
            || day > 31u
            || kbo_date_serial(year, month, day) == 0u) {
        return 0;
    }
    return 1;
}

int kbo_independent_team_acquisition_read_start_date_from_ptr(
    uintptr_t league_ptr,
    uint32_t expected_year,
    uint32_t* out_start_date)
{
    if (out_start_date != NULL) {
        *out_start_date = 0u;
    }
    if (league_ptr == 0u
            || out_start_date == NULL
            || expected_year < 1982u
            || expected_year > 2200u
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET),
                OOTP27_SEASON_START_DATE_SEC_OFFSET - OOTP27_SEASON_START_DATE_YEAR_OFFSET + sizeof(uint8_t))) {
        return 0;
    }
    if (kbo_independent_team_acquisition_date_slot_looks_like_serializer_callback(league_ptr, NULL, NULL)) {
        return 0;
    }

    uint32_t year = *(uint16_t*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
    uint32_t day = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_DAY_OFFSET);
    uint32_t month = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_MONTH_OFFSET);
    if (year < 1982u || year > 2200u
            || year != expected_year
            || month < 1u || month > 12u
            || day < 1u || day > 31u
            || kbo_date_serial(year, month, day) == 0u) {
        return 0;
    }

    uint32_t start_date = year * 10000u + month * 100u + day;
    if (!kbo_independent_team_acquisition_start_date_plausible(start_date, expected_year)) {
        return 0;
    }

    *out_start_date = start_date;
    return 1;
}

static int kbo_independent_team_acquisition_candidate_matches_league_id(
    uintptr_t candidate,
    uint32_t league_id)
{
    static const uint32_t id_offsets[KBO_INDEPENDENT_ACQUISITION_LEAGUE_ID_OFFSET_COUNT] = {
        OOTP27_KBO_LEAGUE_ID_OFFSET,
        OOTP27_SEASON_PHASE_ALT_ID_A_OFFSET,
        OOTP27_SEASON_PHASE_ALT_ID_B_OFFSET,
        OOTP27_SEASON_PHASE_ALT_ID_C_OFFSET
    };
    if (candidate == 0u || league_id == 0u
            || !memory_range_readable((void*)candidate, OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)) {
        return 0;
    }
    for (uint32_t i = 0u; i < KBO_INDEPENDENT_ACQUISITION_LEAGUE_ID_OFFSET_COUNT; i++) {
        if (*(uint32_t*)(candidate + id_offsets[i]) == league_id) {
            return 1;
        }
    }
    return 0;
}

int kbo_independent_team_acquisition_candidate_score(
    uintptr_t candidate,
    uint32_t league_id,
    uint32_t expected_year,
    uint32_t* out_start_date)
{
    uint32_t start_date = 0u;
    if (!kbo_independent_team_acquisition_candidate_matches_league_id(candidate, league_id)
            || !kbo_independent_team_acquisition_read_start_date_from_ptr(
                candidate,
                expected_year,
                &start_date)) {
        return 0;
    }

    int score = 100;
    if (memory_range_readable((void*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        uint32_t league_year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        if (league_year == expected_year) {
            score += 30;
        } else if (league_year + 1u == expected_year || league_year == expected_year + 1u) {
            score += 10;
        } else if (league_year >= 1982u && league_year <= 2200u) {
            score -= 20;
        }
    }

    if (out_start_date != NULL) {
        *out_start_date = start_date;
    }
    return score;
}
