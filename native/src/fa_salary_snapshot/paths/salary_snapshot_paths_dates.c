#include "salary_snapshot_paths_dates.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/sql/save_state/save_state_sqlite.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../core/season/season_calendar.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../state/salary_snapshot_state.h"
#include "../sql/fa_salary_snapshot_sql_store.h"
#include "../../core/dates/constants/kbo_date_constants.h"

int kbo_fa_salary_snapshot_path(uint32_t season, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0 || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    return kbo_save_state_db_path(out, out_size);
}

int kbo_fa_salary_snapshot_file_exists(uint32_t season)
{
    LONG cached_season = InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, 0, 0);
    LONG cached_value = InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, -1, -1);
    if (cached_season == (LONG)season && cached_value != -1) {
        return cached_value == 1;
    }

    int exists = kbo_fa_salary_snapshot_sql_exists(season);
    InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, (LONG)season);
    InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, exists ? 1 : 0);
    return exists;
}

static uint32_t kbo_fa_salary_snapshot_team_league_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }

    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

uint32_t kbo_fa_salary_snapshot_resolve_ranking_team(uint32_t league_id, uint32_t current_team_id, uint32_t active_team_id)
{
    if (active_team_id != 0u && kbo_fa_salary_snapshot_team_league_id(active_team_id) == league_id) {
        return active_team_id;
    }
    if (current_team_id != 0u && kbo_fa_salary_snapshot_team_league_id(current_team_id) == league_id) {
        return current_team_id;
    }
    return 0u;
}

int kbo_fa_salary_snapshot_read_opening_day(uintptr_t league_ptr, uint32_t* out_date)
{
    return kbo_season_calendar_read_league_opening_day(league_ptr, out_date);
}

int kbo_fa_salary_snapshot_current_date_in_opening_window(uint32_t date, uint32_t opening_day)
{
    if (date == 0u || opening_day == 0u) {
        return 0;
    }

    uint32_t year = date / 10000u;
    uint32_t month = (date / 100u) % 100u;
    uint32_t day = date % 100u;
    uint32_t opening_year = opening_day / 10000u;
    uint32_t opening_month = (opening_day / 100u) % 100u;
    uint32_t opening_day_of_month = opening_day % 100u;
    uint32_t current_serial = kbo_date_serial(year, month, day);
    uint32_t opening_serial = kbo_date_serial(opening_year, opening_month, opening_day_of_month);
    if (current_serial == 0u || opening_serial == 0u || current_serial < opening_serial) {
        return 0;
    }
    return current_serial - opening_serial <= (uint32_t)kbo_runtime_tuning_policy()->fa_salary_snapshot_opening_window_days;
}

int kbo_fa_salary_snapshot_load_schedule_opening_day(uint32_t season, uint32_t* out_opening_day)
{
    return kbo_season_calendar_load_schedule_opening_day(season, out_opening_day);
}

static int kbo_fa_salary_snapshot_file_contains_opening_day_message(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 16384u) {
        CloseHandle(file);
        return 0;
    }

    char* data = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (data == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    BOOL ok = ReadFile(file, data, size, &read, NULL);
    CloseHandle(file);
    int found = ok
        && read > 0
        && strstr(data, "Breaking KBO News: Opening Day") != NULL
        && strstr(data, "Korean Baseball Organization") != NULL;
    HeapFree(GetProcessHeap(), 0, data);
    return found;
}

int kbo_fa_salary_snapshot_today_has_opening_day_message(uint32_t date)
{
    if (date == 0u) {
        return 0;
    }

    uint32_t month = (date / 100u) % 100u;
    uint32_t day = date % 100u;
    if (month < 3u || month > 4u || day < 1u || day > 31u) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    char pattern[1024] = {0};
    if (snprintf(pattern, sizeof(pattern), "%s\\messages\\message*.txt", save_path) <= 0) {
        return 0;
    }

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int found = 0;
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        char path[1024] = {0};
        if (snprintf(path, sizeof(path), "%s\\messages\\%s", save_path, find_data.cFileName) <= 0) {
            continue;
        }
        if (kbo_fa_salary_snapshot_file_contains_opening_day_message(path)) {
            found = 1;
            break;
        }
    } while (FindNextFileA(find, &find_data));

    FindClose(find);
    return found;
}
