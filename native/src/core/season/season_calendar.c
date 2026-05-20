#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "season_calendar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core_league_context_parts/api/league_context_lookup.h"
#include "../dates/core_text_date.h"
#include "../dates/tick/current_date_tick_capture.h"
#include "../files/save_paths/core_save_paths.h"
#include "../logging/core_log.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../runtime_memory/runtime_memory.h"

#define KBO_SEASON_CALENDAR_FILE "season_calendar.csv"

static SRWLOCK g_kbo_season_calendar_lock = SRWLOCK_INIT;

static int kbo_season_calendar_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(KBO_SEASON_CALENDAR_FILE, out, out_size);
}

static int kbo_season_calendar_opening_day_valid(uint32_t season, uint32_t opening_day)
{
    uint32_t year = opening_day / 10000u;
    uint32_t month = (opening_day / 100u) % 100u;
    uint32_t day = opening_day % 100u;
    return season >= 1982u
        && season <= 2200u
        && year == season
        && month >= 1u
        && month <= 12u
        && day >= 1u
        && day <= 31u
        && kbo_date_serial(year, month, day) != 0u;
}

static void kbo_season_calendar_sanitize_source(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    const char* text = source != NULL && source[0] != '\0' ? source : "unknown";
    size_t used = 0u;
    for (const char* p = text; *p != '\0' && used + 1u < out_size; ++p) {
        char ch = *p;
        if (ch == ',' || ch == '\r' || ch == '\n') {
            ch = '_';
        }
        out[used++] = ch;
    }
    out[used] = '\0';
}

int kbo_season_calendar_read_league_opening_day(uintptr_t league_ptr, uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 0u;
    }
    if (league_ptr == 0u
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET),
                OOTP27_SEASON_START_DATE_SEC_OFFSET - OOTP27_SEASON_START_DATE_YEAR_OFFSET + sizeof(uint8_t))) {
        return 0;
    }

    uint32_t year = *(uint16_t*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
    uint32_t day = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_DAY_OFFSET);
    uint32_t month = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_MONTH_OFFSET);
    if (year < 1982u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }

    char scratch[16] = {0};
    if (!kbo_format_history_date(scratch, sizeof(scratch), year, month, day)) {
        return 0;
    }

    if (out_date != NULL) {
        *out_date = year * 10000u + month * 100u + day;
    }
    return 1;
}

static int kbo_season_calendar_read_uint_attr(const char* text, const char* name, uint32_t* out_value)
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

int kbo_season_calendar_load_schedule_opening_day(uint32_t season, uint32_t* out_opening_day)
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

static int kbo_season_calendar_file_exists_nonempty(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    return path != NULL
        && path[0] != '\0'
        && GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)
        && (attrs.nFileSizeHigh != 0u || attrs.nFileSizeLow != 0u);
}

static int kbo_season_calendar_load_opening_day_no_lock(
    uint32_t league_id,
    uint32_t season,
    uint32_t* out_opening_day,
    uint32_t* out_observed_date,
    char* out_source,
    size_t out_source_size)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (out_observed_date != NULL) {
        *out_observed_date = 0u;
    }
    if (out_source != NULL && out_source_size > 0u) {
        out_source[0] = '\0';
    }
    if (out_opening_day == NULL || season < 1982u || season > 2200u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_season_calendar_path(path, sizeof(path))) {
        return 0;
    }

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    char line[256] = {0};
    uint32_t found_opening_day = 0u;
    uint32_t found_observed_date = 0u;
    char found_source[64] = {0};
    while (fgets(line, sizeof(line), file) != NULL) {
        unsigned int row_league_id = 0u;
        unsigned int row_season = 0u;
        unsigned int row_opening_day = 0u;
        unsigned int row_observed_date = 0u;
        char row_source[64] = {0};
        int parsed = sscanf(
            line,
            "%u,%u,%u,%u,%63[^,\r\n]",
            &row_league_id,
            &row_season,
            &row_opening_day,
            &row_observed_date,
            row_source);
        if (parsed >= 3
                && (league_id == 0u || (uint32_t)row_league_id == league_id)
                && (uint32_t)row_season == season
                && kbo_season_calendar_opening_day_valid(season, (uint32_t)row_opening_day)) {
            found_opening_day = (uint32_t)row_opening_day;
            found_observed_date = parsed >= 4 ? (uint32_t)row_observed_date : 0u;
            snprintf(found_source, sizeof(found_source), "%s", parsed >= 5 ? row_source : "");
        }
    }
    fclose(file);

    if (found_opening_day == 0u) {
        return 0;
    }
    *out_opening_day = found_opening_day;
    if (out_observed_date != NULL) {
        *out_observed_date = found_observed_date;
    }
    if (out_source != NULL && out_source_size > 0u) {
        snprintf(out_source, out_source_size, "%s", found_source);
    }
    return 1;
}

int kbo_season_calendar_load_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t* out_opening_day)
{
    AcquireSRWLockShared(&g_kbo_season_calendar_lock);
    int ok = kbo_season_calendar_load_opening_day_no_lock(
        league_id,
        season,
        out_opening_day,
        NULL,
        NULL,
        0u);
    ReleaseSRWLockShared(&g_kbo_season_calendar_lock);
    return ok;
}

int kbo_season_calendar_store_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t opening_day,
    uint32_t observed_date,
    const char* source)
{
    if (league_id == 0u || !kbo_season_calendar_opening_day_valid(season, opening_day)) {
        return 0;
    }
    if (observed_date == 0u) {
        (void)kbo_current_date_tick_latest_published_date(&observed_date);
    }

    char path[MAX_PATH] = {0};
    if (!kbo_season_calendar_path(path, sizeof(path))) {
        return 0;
    }

    char safe_source[64] = {0};
    kbo_season_calendar_sanitize_source(source, safe_source, sizeof(safe_source));

    AcquireSRWLockExclusive(&g_kbo_season_calendar_lock);
    uint32_t cached_opening_day = 0u;
    uint32_t cached_observed_date = 0u;
    char cached_source[64] = {0};
    if (kbo_season_calendar_load_opening_day_no_lock(
            league_id,
            season,
            &cached_opening_day,
            &cached_observed_date,
            cached_source,
            sizeof(cached_source))
            && cached_opening_day == opening_day) {
        ReleaseSRWLockExclusive(&g_kbo_season_calendar_lock);
        return 1;
    }

    int write_header = !kbo_season_calendar_file_exists_nonempty(path);
    FILE* file = fopen(path, "a");
    if (file == NULL) {
        ReleaseSRWLockExclusive(&g_kbo_season_calendar_lock);
        kbo_log_runtimef(
            "KBO season calendar opening day store skipped league=%u season=%u opening_day=%u reason=open_failed path=%s",
            league_id,
            season,
            opening_day,
            path);
        return 0;
    }

    if (write_header) {
        fprintf(file, "league_id,season,opening_day,observed_date,source\n");
    }
    fprintf(file, "%u,%u,%u,%u,%s\n", league_id, season, opening_day, observed_date, safe_source);
    fclose(file);
    ReleaseSRWLockExclusive(&g_kbo_season_calendar_lock);

    if (cached_opening_day != 0u && cached_opening_day != opening_day) {
        kbo_log_runtimef(
            "KBO season calendar opening day updated league=%u season=%u previous=%u previous_observed=%u previous_source=%s opening_day=%u observed_date=%u source=%s path=%s",
            league_id,
            season,
            cached_opening_day,
            cached_observed_date,
            cached_source,
            opening_day,
            observed_date,
            safe_source,
            path);
    } else {
        kbo_log_runtimef(
            "KBO season calendar opening day stored league=%u season=%u opening_day=%u observed_date=%u source=%s path=%s",
            league_id,
            season,
            opening_day,
            observed_date,
            safe_source,
            path);
    }
    return 1;
}

int kbo_season_calendar_resolve_opening_day_with_league_ptr(
    uint32_t league_id,
    uint32_t season,
    uint32_t today,
    uintptr_t league_ptr,
    const char* source,
    uint32_t* out_opening_day)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (out_opening_day == NULL || season < 1982u || season > 2200u) {
        return 0;
    }
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (today == 0u) {
        (void)kbo_current_date_tick_latest_published_date(&today);
    }

    uint32_t opening_day = 0u;
    if (league_ptr == 0u && league_id != 0u) {
        league_ptr = kbo_find_league_ptr_from_id(league_id);
    }
    if (league_ptr != 0u
            && kbo_season_calendar_read_league_opening_day(league_ptr, &opening_day)
            && opening_day / 10000u == season) {
        if (league_id != 0u) {
            (void)kbo_season_calendar_store_opening_day(
                league_id,
                season,
                opening_day,
                today,
                source != NULL ? source : "memory");
        }
        *out_opening_day = opening_day;
        return 1;
    }

    if (kbo_season_calendar_load_opening_day(league_id, season, &opening_day)) {
        *out_opening_day = opening_day;
        return 1;
    }

    if (kbo_season_calendar_load_schedule_opening_day(season, &opening_day)
            && opening_day / 10000u == season) {
        if (league_id != 0u) {
            (void)kbo_season_calendar_store_opening_day(
                league_id,
                season,
                opening_day,
                today,
                "schedule_file");
        }
        *out_opening_day = opening_day;
        return 1;
    }

    return 0;
}

int kbo_season_calendar_resolve_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t today,
    uint32_t* out_opening_day)
{
    return kbo_season_calendar_resolve_opening_day_with_league_ptr(
        league_id,
        season,
        today,
        0u,
        "season_calendar_resolve",
        out_opening_day);
}
