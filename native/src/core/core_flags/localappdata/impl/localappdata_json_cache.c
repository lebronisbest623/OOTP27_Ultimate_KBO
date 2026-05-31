#include "../localappdata_internal.h"

#include <string.h>

SRWLOCK g_kbo_localappdata_json_cache_lock = SRWLOCK_INIT;
KboLocalappdataJsonCacheEntry g_kbo_localappdata_json_cache[KBO_LOCALAPPDATA_JSON_CACHE_SLOTS];

static int kbo_filetime_equal(FILETIME a, FILETIME b)
{
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

static uint32_t kbo_localappdata_json_cache_slot(const WCHAR* path)
{
    uint32_t hash = 2166136261u;
    if (path == NULL) {
        return 0u;
    }
    for (const WCHAR* p = path; *p != L'\0'; p++) {
        hash ^= (uint32_t)*p;
        hash *= 16777619u;
    }
    return hash % KBO_LOCALAPPDATA_JSON_CACHE_SLOTS;
}

void kbo_localappdata_json_cache_clear_entry(KboLocalappdataJsonCacheEntry* entry)
{
    if (entry == NULL) {
        return;
    }
    if (entry->buffer != NULL) {
        HeapFree(GetProcessHeap(), 0, entry->buffer);
    }
    memset(entry, 0, sizeof(*entry));
}

KboLocalappdataJsonCacheEntry* kbo_localappdata_json_cache_find_locked(const WCHAR* path)
{
    if (path == NULL || path[0] == L'\0') {
        return NULL;
    }
    for (int i = 0; i < KBO_LOCALAPPDATA_JSON_CACHE_SLOTS; i++) {
        KboLocalappdataJsonCacheEntry* entry = &g_kbo_localappdata_json_cache[i];
        if (entry->valid && wcscmp(entry->path, path) == 0) {
            return entry;
        }
    }
    return NULL;
}

KboLocalappdataJsonCacheEntry* kbo_localappdata_json_cache_slot_locked(const WCHAR* path)
{
    KboLocalappdataJsonCacheEntry* existing = kbo_localappdata_json_cache_find_locked(path);
    if (existing != NULL) {
        return existing;
    }
    for (int i = 0; i < KBO_LOCALAPPDATA_JSON_CACHE_SLOTS; i++) {
        if (!g_kbo_localappdata_json_cache[i].valid) {
            return &g_kbo_localappdata_json_cache[i];
        }
    }
    return &g_kbo_localappdata_json_cache[kbo_localappdata_json_cache_slot(path)];
}

int kbo_localappdata_json_cache_entry_matches(
    const KboLocalappdataJsonCacheEntry* entry,
    DWORD size,
    FILETIME last_write_time)
{
    return entry != NULL
        && entry->valid
        && entry->buffer != NULL
        && entry->size == size
        && kbo_filetime_equal(entry->last_write_time, last_write_time);
}

int kbo_localappdata_json_find_cached_span(
    const KboLocalappdataJsonCacheEntry* entry,
    const char* key,
    const char** out_value,
    const char** out_end)
{
    if (entry == NULL || !entry->values_valid) {
        if (out_value != NULL) { *out_value = NULL; }
        if (out_end != NULL) { *out_end = NULL; }
        return 0;
    }
    return kbo_localappdata_json_find_span_in_table(
        entry->buffer,
        entry->size,
        entry->values,
        entry->value_count,
        key,
        out_value,
        out_end);
}

