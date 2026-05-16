#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../core/files/save_paths/core_save_paths.h"

static int kbo_independent_acquisition_decision_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INDEPENDENT_ACQUISITION_DECISION_FILE,
        out,
        out_size);
}

static int kbo_independent_acquisition_json_u32(
    const char* line,
    const char* key,
    uint32_t* out)
{
    if (line == NULL || key == NULL || out == NULL) {
        return 0;
    }

    char token[64] = {0};
    snprintf(token, sizeof(token), "\"%s\":", key);
    const char* p = strstr(line, token);
    if (p == NULL) {
        return 0;
    }
    p += strlen(token);
    char* end = NULL;
    unsigned long value = strtoul(p, &end, 10);
    if (end == p) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static int kbo_independent_acquisition_parse_decision_line(
    const char* line,
    uint32_t* out_season,
    uint32_t* out_seller_team_id,
    uint32_t* out_player_id,
    uint32_t* out_transferred)
{
    if (line == NULL) {
        return 0;
    }

    uint32_t season = 0u;
    uint32_t seller_team_id = 0u;
    uint32_t player_id = 0u;
    uint32_t transferred = 0u;
    if (!kbo_independent_acquisition_json_u32(line, "season", &season)
            || !kbo_independent_acquisition_json_u32(line, "seller_team_id", &seller_team_id)
            || !kbo_independent_acquisition_json_u32(line, "player_id", &player_id)) {
        return 0;
    }
    kbo_independent_acquisition_json_u32(line, "transferred", &transferred);

    if (out_season != NULL) { *out_season = season; }
    if (out_seller_team_id != NULL) { *out_seller_team_id = seller_team_id; }
    if (out_player_id != NULL) { *out_player_id = player_id; }
    if (out_transferred != NULL) { *out_transferred = transferred; }
    return season != 0u && seller_team_id != 0u && player_id != 0u;
}

int kbo_independent_acquisition_decision_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_decision_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 4u * 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    int exists = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';
        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            char saved = *line_end;
            *line_end = '\0';

            uint32_t row_season = 0u;
            uint32_t row_seller_team_id = 0u;
            uint32_t row_player_id = 0u;
            if (kbo_independent_acquisition_parse_decision_line(
                    cursor,
                    &row_season,
                    &row_seller_team_id,
                    &row_player_id,
                    NULL)
                    && row_season == season
                    && row_seller_team_id == seller_team_id
                    && row_player_id == player_id) {
                exists = 1;
                *line_end = saved;
                break;
            }

            *line_end = saved;
            while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
                line_end++;
            }
            cursor = line_end;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    return exists;
}

int kbo_independent_acquisition_transferred_count(
    uint32_t season,
    uint32_t seller_team_id)
{
    if (season == 0u || seller_team_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_decision_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 4u * 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    int count = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';
        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            char saved = *line_end;
            *line_end = '\0';

            uint32_t row_season = 0u;
            uint32_t row_seller_team_id = 0u;
            uint32_t transferred = 0u;
            if (kbo_independent_acquisition_json_u32(cursor, "season", &row_season)
                    && kbo_independent_acquisition_json_u32(cursor, "seller_team_id", &row_seller_team_id)
                    && kbo_independent_acquisition_json_u32(cursor, "transferred", &transferred)
                    && row_season == season
                    && row_seller_team_id == seller_team_id
                    && transferred != 0u) {
                count++;
            }

            *line_end = saved;
            while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
                line_end++;
            }
            cursor = line_end;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    return count;
}

int kbo_independent_acquisition_buyer_transferred_count(
    uint32_t season,
    uint32_t buyer_team_id)
{
    if (season == 0u || buyer_team_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_decision_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 4u * 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    int count = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';
        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            char saved = *line_end;
            *line_end = '\0';

            uint32_t row_season = 0u;
            uint32_t row_buyer_team_id = 0u;
            uint32_t transferred = 0u;
            if (kbo_independent_acquisition_json_u32(cursor, "season", &row_season)
                    && kbo_independent_acquisition_json_u32(cursor, "buyer_team_id", &row_buyer_team_id)
                    && kbo_independent_acquisition_json_u32(cursor, "transferred", &transferred)
                    && row_season == season
                    && row_buyer_team_id == buyer_team_id
                    && transferred != 0u) {
                count++;
            }

            *line_end = saved;
            while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
                line_end++;
            }
            cursor = line_end;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    return count;
}

uint32_t kbo_independent_acquisition_last_transfer_date(
    uint32_t season,
    uint32_t seller_team_id)
{
    if (season == 0u || seller_team_id == 0u) {
        return 0u;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_decision_path(path, sizeof(path))) {
        return 0u;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0u;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 4u * 1024u * 1024u) {
        CloseHandle(file);
        return 0u;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0u;
    }

    DWORD read = 0u;
    uint32_t last_date = 0u;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';
        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            char saved = *line_end;
            *line_end = '\0';

            uint32_t row_date = 0u;
            uint32_t row_season = 0u;
            uint32_t row_seller_team_id = 0u;
            uint32_t transferred = 0u;
            if (kbo_independent_acquisition_json_u32(cursor, "date", &row_date)
                    && kbo_independent_acquisition_json_u32(cursor, "season", &row_season)
                    && kbo_independent_acquisition_json_u32(cursor, "seller_team_id", &row_seller_team_id)
                    && kbo_independent_acquisition_json_u32(cursor, "transferred", &transferred)
                    && row_season == season
                    && row_seller_team_id == seller_team_id
                    && transferred != 0u
                    && row_date > last_date) {
                last_date = row_date;
            }

            *line_end = saved;
            while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
                line_end++;
            }
            cursor = line_end;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    return last_date;
}
