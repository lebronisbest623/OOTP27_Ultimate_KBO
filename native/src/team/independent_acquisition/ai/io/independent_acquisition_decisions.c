#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"
#include "cache/independent_acquisition_decision_cache.h"

#include <stdlib.h>
#include <string.h>

int kbo_independent_acquisition_decision_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }
    int cached_exists = 0;
    if (kbo_independent_acquisition_decision_cache_exists(
            season,
            seller_team_id,
            player_id,
            &cached_exists)) {
        return cached_exists;
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

int kbo_independent_acquisition_load_decision_keys(
    uint32_t season,
    KboIndependentAcquisitionDecisionKey* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0) {
        return -1;
    }

    kbo_independent_acquisition_decision_cache_lock_init();
    EnterCriticalSection(&g_kbo_independent_acquisition_decision_cache_lock);
    int ok = kbo_independent_acquisition_decision_cache_ensure_locked();
    int count = 0;
    if (ok) {
        for (int i = 0; i < g_kbo_independent_acquisition_decision_cache.count; i++) {
            const KboIndependentAcquisitionDecisionRecord* record =
                &g_kbo_independent_acquisition_decision_cache.records[i];
            if (record->season != season) {
                continue;
            }
            if (count >= max_count) {
                break;
            }
            out[count].season = record->season;
            out[count].seller_team_id = record->seller_team_id;
            out[count].player_id = record->player_id;
            out[count].transferred = record->transferred;
            count++;
        }
    } else {
        count = -1;
    }
    LeaveCriticalSection(&g_kbo_independent_acquisition_decision_cache_lock);
    return count;
}

int kbo_independent_acquisition_transferred_count(
    uint32_t season,
    uint32_t seller_team_id)
{
    if (season == 0u || seller_team_id == 0u) {
        return 0;
    }
    int cached_count = 0;
    if (kbo_independent_acquisition_decision_cache_transferred_count(
            season,
            seller_team_id,
            1,
            &cached_count)) {
        return cached_count;
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
    int cached_count = 0;
    if (kbo_independent_acquisition_decision_cache_transferred_count(
            season,
            buyer_team_id,
            0,
            &cached_count)) {
        return cached_count;
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
    uint32_t cached_last_date = 0u;
    if (kbo_independent_acquisition_decision_cache_last_transfer_date(
            season,
            seller_team_id,
            &cached_last_date)) {
        return cached_last_date;
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
