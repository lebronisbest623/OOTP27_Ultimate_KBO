#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../season_calendar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../dates/core_text_date.h"

static int kbo_season_calendar_read_uint_attr(
    const char* text,
    const char* name,
    uint32_t* out_value)
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
    while (*p == ' ' || *p == '\t' || *p == '"' || *p == '\'') {
        ++p;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }

    uint32_t value = 0u;
    while (*p >= '0' && *p <= '9') {
        value = value * 10u + (uint32_t)(*p - '0');
        ++p;
    }

    *out_value = value;
    return 1;
}

static int kbo_season_calendar_load_schedule_opening_day_from_file(
    const char* path,
    uint32_t season,
    uint32_t* out_opening_day)
{
    if (path == NULL || out_opening_day == NULL || season < 1982u || season > 2200u) {
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
    if (!ok || read == 0u) {
        return 0;
    }
    text[read < sizeof(text) ? read : sizeof(text) - 1u] = '\0';

    uint32_t start_month = 0u;
    uint32_t start_day = 0u;
    if (!kbo_season_calendar_read_uint_attr(text, "start_month", &start_month)
            || !kbo_season_calendar_read_uint_attr(text, "start_day", &start_day)) {
        return 0;
    }
    if (start_month < 1u || start_month > 12u || start_day < 1u || start_day > 31u
            || kbo_date_serial(season, start_month, start_day) == 0u) {
        return 0;
    }

    *out_opening_day = season * 10000u + start_month * 100u + start_day;
    return 1;
}

static int kbo_season_calendar_try_schedule_dir(
    const char* dir,
    uint32_t season,
    uint32_t* out_opening_day,
    char* out_path,
    size_t out_path_size)
{
    if (dir == NULL || dir[0] == '\0' || out_opening_day == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    int written = snprintf(
        path,
        sizeof(path),
        "%s\\korean_baseball_organization_int_c_%u.lsdl",
        dir,
        season);
    if (written <= 0 || (size_t)written >= sizeof(path)) {
        return 0;
    }

    if (!kbo_season_calendar_load_schedule_opening_day_from_file(path, season, out_opening_day)) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0u) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return 1;
}

int kbo_season_calendar_load_schedule_opening_day(
    uint32_t season,
    uint32_t* out_opening_day)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (out_opening_day == NULL || season < 1982u || season > 2200u) {
        return 0;
    }

    char exe_path[MAX_PATH] = {0};
    DWORD got = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (got == 0u || got >= sizeof(exe_path)) {
        return 0;
    }

    char* slash = strrchr(exe_path, '\\');
    if (slash == NULL) {
        return 0;
    }
    *slash = '\0';

    char path[MAX_PATH] = {0};
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s\\data\\schedules", exe_path);
    if (kbo_season_calendar_try_schedule_dir(dir, season, out_opening_day, path, sizeof(path))) {
        return 1;
    }
    snprintf(dir, sizeof(dir), "%s\\schedules", exe_path);
    if (kbo_season_calendar_try_schedule_dir(dir, season, out_opening_day, path, sizeof(path))) {
        return 1;
    }

    const char* user_profile = getenv("USERPROFILE");
    if (user_profile != NULL && user_profile[0] != '\0') {
        snprintf(
            dir,
            sizeof(dir),
            "%s\\Documents\\Out of the Park Developments\\OOTP Baseball 27\\schedules",
            user_profile);
        if (kbo_season_calendar_try_schedule_dir(dir, season, out_opening_day, path, sizeof(path))) {
            return 1;
        }
    }

    return 0;
}
