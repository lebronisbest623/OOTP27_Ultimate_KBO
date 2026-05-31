#include "../localappdata_internal.h"

#include "../../../product/ootp_product.h"

#include <stdio.h>
#include <string.h>

void kbo_invalidate_localappdata_named_json_cache_path(const WCHAR* path)
{
    if (path == NULL || path[0] == L'\0') {
        return;
    }
    AcquireSRWLockExclusive(&g_kbo_localappdata_json_cache_lock);
    KboLocalappdataJsonCacheEntry* entry = kbo_localappdata_json_cache_find_locked(path);
    if (entry != NULL) {
        kbo_localappdata_json_cache_clear_entry(entry);
    }
    ReleaseSRWLockExclusive(&g_kbo_localappdata_json_cache_lock);
}

int kbo_get_localappdata_named_json_path_w(const char* file_name, WCHAR* out, DWORD out_count)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_count == 0) {
        return 0;
    }
    out[0] = L'\0';

    WCHAR local_app_data[KBO_WIDE_PATH_CHARS] = {0};
    DWORD got = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, KBO_WIDE_PATH_CHARS);
    if (got == 0 || got >= KBO_WIDE_PATH_CHARS) {
        return 0;
    }

    WCHAR wide_file_name[260] = {0};
    int converted = MultiByteToWideChar(CP_UTF8, 0, file_name, -1, wide_file_name, (int)(sizeof(wide_file_name) / sizeof(wide_file_name[0])));
    if (converted <= 0) {
        return 0;
    }

    int written = _snwprintf(out, out_count, L"%ls\\" KBO_PRODUCT_LOCAL_DATA_DIR_W L"\\%ls", local_app_data, wide_file_name);
    return written > 0 && (DWORD)written < out_count;
}

int kbo_create_parent_directory_w(const WCHAR* path)
{
    if (path == NULL || path[0] == L'\0') {
        return 0;
    }

    WCHAR dir[KBO_WIDE_PATH_CHARS] = {0};
    _snwprintf(dir, KBO_WIDE_PATH_CHARS, L"%ls", path);
    WCHAR* slash = wcsrchr(dir, L'\\');
    if (slash == NULL) {
        return 0;
    }

    *slash = L'\0';
    return CreateDirectoryW(dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

