#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../foreign_injury_scanner_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../../core/files/save_paths/core_save_paths.h"

#define KBO_FOREIGN_INJURY_MESSAGE_EVIDENCE_CACHE_SIZE 512

typedef struct KboForeignInjuryMessageEvidenceCacheEntry {
    char save_path[MAX_PATH];
    uint32_t player_id;
    uint32_t game_date_yyyymmdd;
    int min_days;
    DWORD file_count;
    FILETIME latest_write;
    int found;
    int days;
    uint8_t valid;
} KboForeignInjuryMessageEvidenceCacheEntry;

typedef struct KboForeignInjuryMessageSignatureCacheEntry {
    char save_path[MAX_PATH];
    DWORD tick;
    DWORD file_count;
    FILETIME latest_write;
    uint8_t valid;
} KboForeignInjuryMessageSignatureCacheEntry;

static KboForeignInjuryMessageEvidenceCacheEntry
    g_kbo_foreign_injury_message_evidence_cache[KBO_FOREIGN_INJURY_MESSAGE_EVIDENCE_CACHE_SIZE];
static KboForeignInjuryMessageSignatureCacheEntry g_kbo_foreign_injury_message_signature_cache;
static KboLock g_kbo_foreign_injury_message_evidence_cache_lock = KBO_LOCK_INIT;

static void kbo_foreign_injury_message_cache_lock(void)
{
    kbo_lock_enter(&g_kbo_foreign_injury_message_evidence_cache_lock);
}

static void kbo_foreign_injury_message_cache_unlock(void)
{
    kbo_lock_leave(&g_kbo_foreign_injury_message_evidence_cache_lock);
}

static uint32_t kbo_foreign_injury_message_cache_slot(uint32_t player_id, int min_days)
{
    uint32_t h = player_id * 2654435761u;
    h ^= (uint32_t)min_days * 2246822519u;
    h ^= h >> 16;
    return h & (KBO_FOREIGN_INJURY_MESSAGE_EVIDENCE_CACHE_SIZE - 1u);
}

static int kbo_foreign_injury_message_filetime_equal(FILETIME a, FILETIME b)
{
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

static int kbo_foreign_injury_message_signature(
    const char* save_path,
    DWORD* out_file_count,
    FILETIME* out_latest_write)
{
    if (out_file_count != NULL) {
        *out_file_count = 0u;
    }
    if (out_latest_write != NULL) {
        out_latest_write->dwLowDateTime = 0u;
        out_latest_write->dwHighDateTime = 0u;
    }
    if (save_path == NULL || save_path[0] == '\0') {
        return 0;
    }

    DWORD now = GetTickCount();
    kbo_foreign_injury_message_cache_lock();
    KboForeignInjuryMessageSignatureCacheEntry cached =
        g_kbo_foreign_injury_message_signature_cache;
    if (cached.valid
            && strcmp(cached.save_path, save_path) == 0
            && now - cached.tick <= 1000u) {
        kbo_foreign_injury_message_cache_unlock();
        if (out_file_count != NULL) {
            *out_file_count = cached.file_count;
        }
        if (out_latest_write != NULL) {
            *out_latest_write = cached.latest_write;
        }
        return cached.file_count > 0u;
    }
    kbo_foreign_injury_message_cache_unlock();

    char pattern[1024] = {0};
    snprintf(pattern, sizeof(pattern), "%s\\messages\\message*.txt", save_path);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD count = 0u;
    FILETIME latest = {0u, 0u};
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        count++;
        if (CompareFileTime(&find_data.ftLastWriteTime, &latest) > 0) {
            latest = find_data.ftLastWriteTime;
        }
    } while (FindNextFileA(find, &find_data));
    FindClose(find);

    if (out_file_count != NULL) {
        *out_file_count = count;
    }
    if (out_latest_write != NULL) {
        *out_latest_write = latest;
    }

    kbo_foreign_injury_message_cache_lock();
    snprintf(g_kbo_foreign_injury_message_signature_cache.save_path,
        sizeof(g_kbo_foreign_injury_message_signature_cache.save_path),
        "%s",
        save_path);
    g_kbo_foreign_injury_message_signature_cache.tick = now;
    g_kbo_foreign_injury_message_signature_cache.file_count = count;
    g_kbo_foreign_injury_message_signature_cache.latest_write = latest;
    g_kbo_foreign_injury_message_signature_cache.valid = 1u;
    kbo_foreign_injury_message_cache_unlock();
    return count > 0u;
}

static int kbo_foreign_injury_message_cache_get(
    const char* save_path,
    uint32_t player_id,
    uint32_t game_date_yyyymmdd,
    int min_days,
    DWORD file_count,
    FILETIME latest_write,
    int* out_days)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    uint32_t slot = kbo_foreign_injury_message_cache_slot(player_id, min_days);
    kbo_foreign_injury_message_cache_lock();
    KboForeignInjuryMessageEvidenceCacheEntry cached =
        g_kbo_foreign_injury_message_evidence_cache[slot];
    kbo_foreign_injury_message_cache_unlock();

    if (!cached.valid
            || cached.player_id != player_id
            || cached.game_date_yyyymmdd != game_date_yyyymmdd
            || cached.min_days != min_days
            || cached.file_count != file_count
            || !kbo_foreign_injury_message_filetime_equal(cached.latest_write, latest_write)
            || strcmp(cached.save_path, save_path) != 0) {
        return 0;
    }

    if (out_days != NULL) {
        *out_days = cached.days;
    }
    return cached.found ? 1 : -1;
}

static void kbo_foreign_injury_message_cache_store(
    const char* save_path,
    uint32_t player_id,
    uint32_t game_date_yyyymmdd,
    int min_days,
    DWORD file_count,
    FILETIME latest_write,
    int found,
    int days)
{
    uint32_t slot = kbo_foreign_injury_message_cache_slot(player_id, min_days);
    kbo_foreign_injury_message_cache_lock();
    KboForeignInjuryMessageEvidenceCacheEntry* entry =
        &g_kbo_foreign_injury_message_evidence_cache[slot];
    entry->valid = 0u;
    snprintf(entry->save_path, sizeof(entry->save_path), "%s", save_path != NULL ? save_path : "");
    entry->player_id = player_id;
    entry->game_date_yyyymmdd = game_date_yyyymmdd;
    entry->min_days = min_days;
    entry->file_count = file_count;
    entry->latest_write = latest_write;
    entry->found = found ? 1 : 0;
    entry->days = days;
    entry->valid = 1u;
    kbo_foreign_injury_message_cache_unlock();
}

static int kbo_foreign_injury_message_ascii_lower(int ch)
{
    return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch;
}

static int kbo_foreign_injury_message_contains_nocase(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    for (const char* p = text; *p != '\0'; ++p) {
        const char* a = p;
        const char* b = needle;
        while (*a != '\0' && *b != '\0'
                && kbo_foreign_injury_message_ascii_lower((unsigned char)*a)
                    == kbo_foreign_injury_message_ascii_lower((unsigned char)*b)) {
            ++a;
            ++b;
        }
        if (*b == '\0') {
            return 1;
        }
    }
    return 0;
}

static int kbo_foreign_injury_message_contains_exact_player_tag(
    const char* text,
    uint32_t player_id)
{
    if (text == NULL || player_id == 0u) {
        return 0;
    }

    char player_tag[32] = {0};
    snprintf(player_tag, sizeof(player_tag), "player#%u", player_id);
    size_t tag_len = strlen(player_tag);
    const char* p = text;
    while ((p = strstr(p, player_tag)) != NULL) {
        char next = p[tag_len];
        if (next < '0' || next > '9') {
            return 1;
        }
        p += tag_len;
    }
    return 0;
}

static int kbo_foreign_injury_message_file_has_long_term_injury(
    const char* path,
    uint32_t player_id,
    int min_days,
    int* out_days)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    if (path == NULL || path[0] == '\0' || player_id == 0u || min_days <= 0) {
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

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 65536u) {
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
    if (!ok || read == 0u) {
        HeapFree(GetProcessHeap(), 0, data);
        return 0;
    }
    data[read < size ? read : size] = '\0';

    int evidence_days = 0;
    int found = kbo_foreign_injury_message_contains_exact_player_tag(data, player_id)
        && (kbo_foreign_injury_message_contains_nocase(data, "injured list")
            || kbo_foreign_injury_message_contains_nocase(data, "injury")
            || kbo_foreign_injury_message_contains_nocase(data, "injured")
            || kbo_foreign_injury_message_contains_nocase(data, "sidelined")
            || kbo_foreign_injury_message_contains_nocase(data, "out of commission"))
        && kbo_foreign_injury_duration_text_meets_minimum(data, min_days, &evidence_days);
    HeapFree(GetProcessHeap(), 0, data);

    if (found && out_days != NULL) {
        *out_days = evidence_days;
    }
    return found;
}

int kbo_foreign_injury_recent_message_has_long_term_injury(
    uint32_t player_id,
    int min_days,
    int* out_days)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    if (player_id == 0u || min_days <= 0) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }
    uint32_t game_date_yyyymmdd = 0u;
    kbo_current_date_tick_latest_published_date(&game_date_yyyymmdd);

    DWORD file_count = 0u;
    FILETIME latest_write = {0u, 0u};
    if (!kbo_foreign_injury_message_signature(save_path, &file_count, &latest_write)) {
        return 0;
    }
    int cached_days = 0;
    int cached = kbo_foreign_injury_message_cache_get(
        save_path,
        player_id,
        game_date_yyyymmdd,
        min_days,
        file_count,
        latest_write,
        &cached_days);
    if (cached != 0) {
        if (out_days != NULL) {
            *out_days = cached_days;
        }
        return cached > 0;
    }

    char pattern[1024] = {0};
    snprintf(pattern, sizeof(pattern), "%s\\messages\\message*.txt", save_path);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int found = 0;
    int best_days = 0;
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        char path[1024] = {0};
        snprintf(path, sizeof(path), "%s\\messages\\%s", save_path, find_data.cFileName);
        int evidence_days = 0;
        if (kbo_foreign_injury_message_file_has_long_term_injury(
                path,
                player_id,
                min_days,
                &evidence_days)) {
            found = 1;
            if (evidence_days > best_days) {
                best_days = evidence_days;
            }
        }
    } while (FindNextFileA(find, &find_data));
    FindClose(find);

    if (out_days != NULL) {
        *out_days = best_days;
    }
    kbo_foreign_injury_message_cache_store(
        save_path,
        player_id,
        game_date_yyyymmdd,
        min_days,
        file_count,
        latest_write,
        found,
        best_days);
    return found;
}
