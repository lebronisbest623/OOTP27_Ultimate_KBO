#include "../schedule_dates.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../../../../core/dates/constants/kbo_date_constants.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/product/ootp_product.h"

static int kbo_allstar_read_uint_attr(const char* text, const char* name, uint32_t* out_value)
{
    if (text == NULL || name == NULL || out_value == NULL) {
        return 0;
    }

    const char* found = strstr(text, name);
    if (found == NULL) {
        return 0;
    }

    const char* equals = strchr(found, '=');
    if (equals == NULL) {
        return 0;
    }

    const char* p = equals + 1;
    while (*p == ' ' || *p == '\t' || *p == '\"' || *p == '\'') {
        ++p;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    uint32_t value = 0;
    while (*p >= '0' && *p <= '9') {
        value = value * 10u + (uint32_t)(*p - '0');
        ++p;
    }

    *out_value = value;
    return 1;
}

static int kbo_allstar_serial_to_ymd(uint32_t serial, uint32_t* out_year, uint32_t* out_month, uint32_t* out_day)
{
    if (serial == 0 || out_year == NULL || out_month == NULL || out_day == NULL) {
        return 0;
    }

    uint32_t year = serial / 365u + 1u;
    while (year > 1u && kbo_date_serial(year, 1u, 1u) > serial) {
        --year;
    }
    while (kbo_date_serial(year + 1u, 1u, 1u) != 0u && kbo_date_serial(year + 1u, 1u, 1u) <= serial) {
        ++year;
    }

    uint32_t month = 1u;
    while (month < 12u) {
        uint32_t next_month_start = kbo_date_serial(year, month + 1u, 1u);
        if (next_month_start == 0u || next_month_start > serial) {
            break;
        }
        ++month;
    }

    uint32_t month_start = kbo_date_serial(year, month, 1u);
    if (month_start == 0u || serial < month_start) {
        return 0;
    }

    *out_year = year;
    *out_month = month;
    *out_day = serial - month_start + 1u;
    return 1;
}

static int kbo_allstar_try_load_schedule_file(
    const char* path,
    uint32_t league_year,
    KboAllstarScheduleDate* out_start,
    KboAllstarScheduleDate* out_allstar)
{
    if (path == NULL || out_start == NULL || out_allstar == NULL) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char text[8192] = {0};
    DWORD read = 0;
    BOOL ok = ReadFile(file, text, sizeof(text) - 1u, &read, NULL);
    CloseHandle(file);
    if (!ok || read == 0) {
        return 0;
    }
    text[read < sizeof(text) ? read : sizeof(text) - 1u] = '\0';

    uint32_t start_month = 0;
    uint32_t start_day = 0;
    uint32_t allstar_day = 0;
    if (!kbo_allstar_read_uint_attr(text, "start_month", &start_month)
            || !kbo_allstar_read_uint_attr(text, "start_day", &start_day)
            || !kbo_allstar_read_uint_attr(text, "allstar_game_day", &allstar_day)) {
        return 0;
    }
    if (league_year < KBO_SEASON_YEAR_MIN || league_year > KBO_SIM_YEAR_MAX
            || start_month < 1u || start_month > 12u
            || start_day < 1u || start_day > 31u
            || allstar_day < 1u || allstar_day > 250u
            || kbo_date_serial(league_year, start_month, start_day) == 0u) {
        return 0;
    }

    uint32_t allstar_serial = kbo_date_serial(league_year, start_month, start_day) + allstar_day - 1u;
    uint32_t allstar_year = 0;
    uint32_t allstar_month = 0;
    uint32_t allstar_dom = 0;
    if (!kbo_allstar_serial_to_ymd(allstar_serial, &allstar_year, &allstar_month, &allstar_dom)
            || allstar_year < KBO_SEASON_YEAR_MIN || allstar_year > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    uint32_t allstar_time = 1830u;
    const char* type4 = strstr(text, "type=\"4\"");
    if (type4 != NULL) {
        const char* line_start = type4;
        while (line_start > text && line_start[-1] != '\n' && line_start[-1] != '\r') {
            --line_start;
        }
        char line[512] = {0};
        size_t i = 0;
        while (line_start[i] != '\0' && line_start[i] != '\n' && line_start[i] != '\r' && i + 1u < sizeof(line)) {
            line[i] = line_start[i];
            ++i;
        }
        uint32_t parsed_time = 0;
        if (kbo_allstar_read_uint_attr(line, "time", &parsed_time) && parsed_time <= 2359u) {
            allstar_time = parsed_time;
        }
    }

    out_start->year = (uint16_t)league_year;
    out_start->month = (uint8_t)start_month;
    out_start->day = (uint8_t)start_day;
    out_start->hour = 0u;
    out_start->minute = 0u;
    out_start->second = 0u;

    out_allstar->year = (uint16_t)allstar_year;
    out_allstar->month = (uint8_t)allstar_month;
    out_allstar->day = (uint8_t)allstar_dom;
    out_allstar->hour = (uint8_t)(allstar_time / 100u);
    out_allstar->minute = (uint8_t)(allstar_time % 100u);
    out_allstar->second = 0u;
    return 1;
}

int kbo_allstar_load_schedule_dates(uint32_t league_year, KboAllstarScheduleDate* out_start, KboAllstarScheduleDate* out_allstar, char* out_path, size_t out_path_size)
{
    char exe_path[MAX_PATH] = {0};
    DWORD got = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (got == 0 || got >= sizeof(exe_path)) {
        return 0;
    }

    char* slash = strrchr(exe_path, '\\');
    if (slash == NULL) {
        return 0;
    }
    *slash = '\0';

    const char* suffixes[] = { "" };
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        char path[MAX_PATH] = {0};
        int written = snprintf(
            path,
            sizeof(path),
            "%s\\" KBO_OOTP_DATA_DIR "\\" KBO_OOTP_SCHEDULES_DIR "\\" KBO_OOTP_KBO_SCHEDULE_PREFIX "%u%s" KBO_OOTP_KBO_SCHEDULE_EXTENSION,
            exe_path,
            league_year,
            suffixes[i]);
        if (written <= 0 || (size_t)written >= sizeof(path)) {
            continue;
        }
        if (!kbo_allstar_try_load_schedule_file(path, league_year, out_start, out_allstar)) {
            continue;
        }
        if (out_path != NULL && out_path_size > 0) {
            snprintf(out_path, out_path_size, "%s", path);
        }
        return 1;
    }

    return 0;
}

