#ifndef KBO_CORE_FLAGS_LOCALAPPDATA_INTERNAL_H
#define KBO_CORE_FLAGS_LOCALAPPDATA_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

#include "../json/json_bool_parser.h"
#include "../../files/save_paths/platform/core_path_io.h"
#include "localappdata_reader.h"

typedef int (*KboLocalappdataJsonParseFn)(const char* json, DWORD json_size, void* context);
typedef int (*KboLocalappdataJsonValueParseFn)(const char* value, const char* end, void* context);

typedef struct KboLocalappdataJsonValueSpan {
    DWORD key_start;
    DWORD key_size;
    DWORD value_start;
    DWORD value_size;
} KboLocalappdataJsonValueSpan;

typedef struct KboLocalappdataJsonCacheEntry {
    WCHAR path[KBO_WIDE_PATH_CHARS];
    FILETIME last_write_time;
    DWORD size;
    char* buffer;
    KboLocalappdataJsonValueSpan values[256];
    DWORD value_count;
    uint8_t values_valid;
    uint8_t valid;
} KboLocalappdataJsonCacheEntry;

enum {
    KBO_LOCALAPPDATA_JSON_CACHE_SLOTS = 8
};

extern SRWLOCK g_kbo_localappdata_json_cache_lock;
extern KboLocalappdataJsonCacheEntry g_kbo_localappdata_json_cache[KBO_LOCALAPPDATA_JSON_CACHE_SLOTS];

void kbo_localappdata_json_cache_clear_entry(KboLocalappdataJsonCacheEntry* entry);
KboLocalappdataJsonCacheEntry* kbo_localappdata_json_cache_find_locked(const WCHAR* path);
KboLocalappdataJsonCacheEntry* kbo_localappdata_json_cache_slot_locked(const WCHAR* path);
int kbo_localappdata_json_cache_entry_matches(
    const KboLocalappdataJsonCacheEntry* entry,
    DWORD size,
    FILETIME last_write_time);
int kbo_localappdata_json_find_span_in_table(
    const char* buffer,
    DWORD size,
    const KboLocalappdataJsonValueSpan* spans,
    DWORD span_count,
    const char* key,
    const char** out_value,
    const char** out_end);
int kbo_localappdata_json_find_cached_span(
    const KboLocalappdataJsonCacheEntry* entry,
    const char* key,
    const char** out_value,
    const char** out_end);
int kbo_localappdata_json_parse_value_spans(
    const char* json,
    DWORD size,
    KboLocalappdataJsonValueSpan* out_values,
    DWORD* out_count);
void kbo_invalidate_localappdata_named_json_cache_path(const WCHAR* path);
int kbo_get_localappdata_named_json_path_w(const char* file_name, WCHAR* out, DWORD out_count);
int kbo_create_parent_directory_w(const WCHAR* path);
int kbo_read_localappdata_named_json_cached(
    const char* file_name,
    const char* key,
    KboLocalappdataJsonParseFn parse,
    KboLocalappdataJsonValueParseFn parse_value,
    void* context);

#endif
