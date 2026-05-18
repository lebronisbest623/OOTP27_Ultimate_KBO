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

typedef struct KboIndependentAcquisitionDecisionRecord {
    uint32_t date;
    uint32_t season;
    uint32_t seller_team_id;
    uint32_t buyer_team_id;
    uint32_t player_id;
    uint32_t transferred;
} KboIndependentAcquisitionDecisionRecord;

typedef struct KboIndependentAcquisitionDecisionCache {
    char path[MAX_PATH];
    FILETIME last_write_time;
    DWORD file_size;
    int has_file;
    int valid;
    KboIndependentAcquisitionDecisionRecord* records;
    int count;
    int capacity;
} KboIndependentAcquisitionDecisionCache;

static CRITICAL_SECTION g_kbo_independent_acquisition_decision_cache_lock;
static volatile LONG g_kbo_independent_acquisition_decision_cache_lock_state = 0;
static KboIndependentAcquisitionDecisionCache g_kbo_independent_acquisition_decision_cache;

static void kbo_independent_acquisition_decision_cache_lock_init(void)
{
    for (;;) {
        LONG state = InterlockedCompareExchange(
            &g_kbo_independent_acquisition_decision_cache_lock_state,
            0,
            0);
        if (state == 2) {
            return;
        }
        if (state == 0
                && InterlockedCompareExchange(
                    &g_kbo_independent_acquisition_decision_cache_lock_state,
                    1,
                    0) == 0) {
            InitializeCriticalSection(&g_kbo_independent_acquisition_decision_cache_lock);
            InterlockedExchange(&g_kbo_independent_acquisition_decision_cache_lock_state, 2);
            return;
        }
        SwitchToThread();
    }
}

static void kbo_independent_acquisition_decision_cache_free_locked(void)
{
    if (g_kbo_independent_acquisition_decision_cache.records != NULL) {
        HeapFree(GetProcessHeap(), 0, g_kbo_independent_acquisition_decision_cache.records);
    }
    memset(&g_kbo_independent_acquisition_decision_cache, 0, sizeof(g_kbo_independent_acquisition_decision_cache));
}

static int kbo_independent_acquisition_decision_file_attrs(
    const char* path,
    DWORD* out_size,
    FILETIME* out_last_write_time)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    memset(&data, 0, sizeof(data));
    if (path == NULL
            || path[0] == '\0'
            || !GetFileAttributesExA(path, GetFileExInfoStandard, &data)
            || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
            || data.nFileSizeHigh != 0u
            || data.nFileSizeLow > 4u * 1024u * 1024u) {
        return 0;
    }
    if (out_size != NULL) {
        *out_size = data.nFileSizeLow;
    }
    if (out_last_write_time != NULL) {
        *out_last_write_time = data.ftLastWriteTime;
    }
    return 1;
}

static int kbo_independent_acquisition_decision_cache_matches_locked(
    const char* path,
    int has_file,
    DWORD file_size,
    const FILETIME* last_write_time)
{
    if (!g_kbo_independent_acquisition_decision_cache.valid
            || path == NULL
            || strcmp(g_kbo_independent_acquisition_decision_cache.path, path) != 0
            || g_kbo_independent_acquisition_decision_cache.has_file != has_file) {
        return 0;
    }
    if (!has_file) {
        return 1;
    }
    return g_kbo_independent_acquisition_decision_cache.file_size == file_size
        && last_write_time != NULL
        && CompareFileTime(
            &g_kbo_independent_acquisition_decision_cache.last_write_time,
            last_write_time) == 0;
}

static int kbo_independent_acquisition_decision_cache_append_locked(
    const KboIndependentAcquisitionDecisionRecord* record)
{
    if (record == NULL) {
        return 0;
    }
    if (g_kbo_independent_acquisition_decision_cache.count
            >= g_kbo_independent_acquisition_decision_cache.capacity) {
        int new_capacity = g_kbo_independent_acquisition_decision_cache.capacity > 0
            ? g_kbo_independent_acquisition_decision_cache.capacity * 2
            : 64;
        KboIndependentAcquisitionDecisionRecord* grown = NULL;
        if (g_kbo_independent_acquisition_decision_cache.records != NULL) {
            grown = (KboIndependentAcquisitionDecisionRecord*)HeapReAlloc(
                GetProcessHeap(),
                HEAP_ZERO_MEMORY,
                g_kbo_independent_acquisition_decision_cache.records,
                (SIZE_T)new_capacity * sizeof(KboIndependentAcquisitionDecisionRecord));
        } else {
            grown = (KboIndependentAcquisitionDecisionRecord*)HeapAlloc(
                GetProcessHeap(),
                HEAP_ZERO_MEMORY,
                (SIZE_T)new_capacity * sizeof(KboIndependentAcquisitionDecisionRecord));
        }
        if (grown == NULL) {
            return 0;
        }
        g_kbo_independent_acquisition_decision_cache.records = grown;
        g_kbo_independent_acquisition_decision_cache.capacity = new_capacity;
    }
    g_kbo_independent_acquisition_decision_cache.records[
        g_kbo_independent_acquisition_decision_cache.count++] = *record;
    return 1;
}

static int kbo_independent_acquisition_decision_cache_reload_locked(
    const char* path,
    int has_file,
    DWORD file_size,
    const FILETIME* last_write_time)
{
    kbo_independent_acquisition_decision_cache_free_locked();
    snprintf(
        g_kbo_independent_acquisition_decision_cache.path,
        sizeof(g_kbo_independent_acquisition_decision_cache.path),
        "%s",
        path != NULL ? path : "");
    g_kbo_independent_acquisition_decision_cache.valid = 1;
    g_kbo_independent_acquisition_decision_cache.has_file = has_file;
    g_kbo_independent_acquisition_decision_cache.file_size = file_size;
    if (last_write_time != NULL) {
        g_kbo_independent_acquisition_decision_cache.last_write_time = *last_write_time;
    }
    if (!has_file || file_size == 0u) {
        return 1;
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
        kbo_independent_acquisition_decision_cache_free_locked();
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)file_size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        kbo_independent_acquisition_decision_cache_free_locked();
        return 0;
    }

    DWORD read = 0u;
    int ok = ReadFile(file, buffer, file_size, &read, NULL);
    CloseHandle(file);
    if (!ok || read == 0u) {
        HeapFree(GetProcessHeap(), 0, buffer);
        if (!ok) {
            kbo_independent_acquisition_decision_cache_free_locked();
        }
        return ok ? 1 : 0;
    }
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

        KboIndependentAcquisitionDecisionRecord record;
        memset(&record, 0, sizeof(record));
        if (kbo_independent_acquisition_parse_decision_line(
                cursor,
                &record.season,
                &record.seller_team_id,
                &record.player_id,
                &record.transferred)) {
            kbo_independent_acquisition_json_u32(cursor, "date", &record.date);
            kbo_independent_acquisition_json_u32(cursor, "buyer_team_id", &record.buyer_team_id);
            if (!kbo_independent_acquisition_decision_cache_append_locked(&record)) {
                HeapFree(GetProcessHeap(), 0, buffer);
                kbo_independent_acquisition_decision_cache_free_locked();
                return 0;
            }
        }

        *line_end = saved;
        while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
            line_end++;
        }
        cursor = line_end;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return 1;
}

static int kbo_independent_acquisition_decision_cache_ensure_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_decision_path(path, sizeof(path))) {
        return 0;
    }

    DWORD file_size = 0u;
    FILETIME last_write_time;
    memset(&last_write_time, 0, sizeof(last_write_time));
    int has_file = kbo_independent_acquisition_decision_file_attrs(
        path,
        &file_size,
        &last_write_time);
    if (kbo_independent_acquisition_decision_cache_matches_locked(
            path,
            has_file,
            file_size,
            has_file ? &last_write_time : NULL)) {
        return 1;
    }
    return kbo_independent_acquisition_decision_cache_reload_locked(
        path,
        has_file,
        file_size,
        has_file ? &last_write_time : NULL);
}

static int kbo_independent_acquisition_decision_cache_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id,
    int* out_exists)
{
    if (out_exists != NULL) {
        *out_exists = 0;
    }
    kbo_independent_acquisition_decision_cache_lock_init();
    EnterCriticalSection(&g_kbo_independent_acquisition_decision_cache_lock);
    int ok = kbo_independent_acquisition_decision_cache_ensure_locked();
    if (ok) {
        for (int i = 0; i < g_kbo_independent_acquisition_decision_cache.count; i++) {
            const KboIndependentAcquisitionDecisionRecord* record =
                &g_kbo_independent_acquisition_decision_cache.records[i];
            if (record->season == season
                    && record->seller_team_id == seller_team_id
                    && record->player_id == player_id) {
                if (out_exists != NULL) {
                    *out_exists = 1;
                }
                break;
            }
        }
    }
    LeaveCriticalSection(&g_kbo_independent_acquisition_decision_cache_lock);
    return ok;
}

static int kbo_independent_acquisition_decision_cache_transferred_count(
    uint32_t season,
    uint32_t team_id,
    int seller_side,
    int* out_count)
{
    if (out_count != NULL) {
        *out_count = 0;
    }
    kbo_independent_acquisition_decision_cache_lock_init();
    EnterCriticalSection(&g_kbo_independent_acquisition_decision_cache_lock);
    int ok = kbo_independent_acquisition_decision_cache_ensure_locked();
    if (ok) {
        int count = 0;
        for (int i = 0; i < g_kbo_independent_acquisition_decision_cache.count; i++) {
            const KboIndependentAcquisitionDecisionRecord* record =
                &g_kbo_independent_acquisition_decision_cache.records[i];
            uint32_t row_team_id = seller_side ? record->seller_team_id : record->buyer_team_id;
            if (record->season == season
                    && row_team_id == team_id
                    && record->transferred != 0u) {
                count++;
            }
        }
        if (out_count != NULL) {
            *out_count = count;
        }
    }
    LeaveCriticalSection(&g_kbo_independent_acquisition_decision_cache_lock);
    return ok;
}

static int kbo_independent_acquisition_decision_cache_last_transfer_date(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t* out_last_date)
{
    if (out_last_date != NULL) {
        *out_last_date = 0u;
    }
    kbo_independent_acquisition_decision_cache_lock_init();
    EnterCriticalSection(&g_kbo_independent_acquisition_decision_cache_lock);
    int ok = kbo_independent_acquisition_decision_cache_ensure_locked();
    if (ok) {
        uint32_t last_date = 0u;
        for (int i = 0; i < g_kbo_independent_acquisition_decision_cache.count; i++) {
            const KboIndependentAcquisitionDecisionRecord* record =
                &g_kbo_independent_acquisition_decision_cache.records[i];
            if (record->season == season
                    && record->seller_team_id == seller_team_id
                    && record->transferred != 0u
                    && record->date > last_date) {
                last_date = record->date;
            }
        }
        if (out_last_date != NULL) {
            *out_last_date = last_date;
        }
    }
    LeaveCriticalSection(&g_kbo_independent_acquisition_decision_cache_lock);
    return ok;
}

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
