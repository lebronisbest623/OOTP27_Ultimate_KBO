#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include "../api/league_context_lookup.h"
#include "../../files/save_paths/core_save_paths.h"
#include "../../logging/core_log.h"
#include "../../product/ootp_product.h"

uint32_t kbo_read_kbo_league_id_override(void)
{
    static uint32_t cached_value = 0u;
    static ULONGLONG last_checked_ms = 0u;
    static LONG logged_value = 0;

    ULONGLONG now_ms = GetTickCount64();
    if (cached_value != 0u && now_ms - last_checked_ms < 5000u) {
        return cached_value;
    }
    last_checked_ms = now_ms;

    char path[MAX_PATH] = {0};
    if (!kbo_get_global_data_file(KBO_PRODUCT_LEAGUE_ID_FILE, path, sizeof(path))) {
        static LONG missing_logged = 0;
        if (InterlockedCompareExchange(&missing_logged, 1, 0) == 0) {
            kbo_log_runtime_line("KBO league id override: global data path unavailable");
        }
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        static LONG missing_file_logged = 0;
        if (InterlockedCompareExchange(&missing_file_logged, 1, 0) == 0) {
            kbo_log_runtimef("KBO league id override file missing=%s", path);
        }
        cached_value = 0u;
        return 0;
    }

    char raw[64] = {0};
    DWORD read = 0;
    ReadFile(file, raw, sizeof(raw) - 1, &read, NULL);
    CloseHandle(file);
    if (read == 0) {
        kbo_log_runtimef("KBO league id override read failed: empty file=%s", path);
        cached_value = 0u;
        return 0;
    }

    raw[sizeof(raw) - 1] = '\0';

    uint32_t value = 0;
    size_t idx = 0;
    if (read >= 3u && (unsigned char)raw[0] == 0xEFu && (unsigned char)raw[1] == 0xBBu && (unsigned char)raw[2] == 0xBFu) {
        idx = 3u;
    }

    if (idx + 2u < (size_t)read && (unsigned char)raw[idx] == 0xFFu && (unsigned char)raw[idx + 1u] == 0xFEu) {
        idx += 2u;
        while (idx + 1u < (size_t)read) {
            uint16_t ch = (uint16_t)((unsigned char)raw[idx]) | ((uint16_t)(unsigned char)raw[idx + 1u] << 8);
            if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t') {
                idx += 2u;
                continue;
            }
            if (ch >= '0' && ch <= '9') {
                value = (uint32_t)(value * 10u + (unsigned)(ch - '0'));
                idx += 2u;
                while (idx + 1u < (size_t)read) {
                    ch = (uint16_t)((unsigned char)raw[idx]) | ((uint16_t)(unsigned char)raw[idx + 1u] << 8);
                    if (ch >= '0' && ch <= '9') {
                        value = (uint32_t)(value * 10u + (unsigned)(ch - '0'));
                        idx += 2u;
                        continue;
                    }
                    break;
                }
            }
            break;
        }
        if (value == 0) {
            kbo_log_runtimef("KBO league id override parse failed (utf16) file=%s read=%lu", path, read);
            cached_value = 0u;
            return 0;
        }
        cached_value = value;
        if ((uint32_t)InterlockedCompareExchange(&logged_value, (LONG)value, (LONG)value) != value) {
            InterlockedExchange(&logged_value, (LONG)value);
            kbo_log_runtimef("KBO league id override file=%s utf16 value=%u", path, value);
        }
        return value;
    }

    while (idx < (size_t)read && (raw[idx] == '\r' || raw[idx] == '\n' || raw[idx] == ' ' || raw[idx] == '\t')) {
        ++idx;
    }
    if (idx >= (size_t)read) {
        kbo_log_runtimef("KBO league id override parse failed: no body in file=%s", path);
        cached_value = 0u;
        return 0;
    }

    while (idx < (size_t)read && raw[idx] >= '0' && raw[idx] <= '9') {
        value = (uint32_t)(value * 10u + (unsigned)(raw[idx] - '0'));
        ++idx;
    }
    if (value == 0) {
        kbo_log_runtimef("KBO league id override parse failed file=%s", path);
        cached_value = 0u;
        return 0;
    }

    cached_value = value;
    if ((uint32_t)InterlockedCompareExchange(&logged_value, (LONG)value, (LONG)value) != value) {
        InterlockedExchange(&logged_value, (LONG)value);
        kbo_log_runtimef("KBO league id override file=%s value=%u", path, value);
    }
    return value;
}
