#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../paths/foreign_waiver_paths.h"
#include "../policy/foreign_player_policy.h"
#include "foreign_waiver_player_eval.h"

#define KBO_ASIAN_QUOTA_NATION_MAX 64

static uint32_t g_kbo_asian_quota_nation_ids[KBO_ASIAN_QUOTA_NATION_MAX] = {0};
static LONG g_kbo_asian_quota_nation_count = -1;

static void kbo_add_asian_quota_nation_id(uint32_t nation_id, int* count)
{
    if (count == NULL || *count >= KBO_ASIAN_QUOTA_NATION_MAX
            || nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        if (g_kbo_asian_quota_nation_ids[i] == nation_id) {
            return;
        }
    }
    g_kbo_asian_quota_nation_ids[(*count)++] = nation_id;
}

int kbo_load_asian_quota_nation_ids_once(void)
{
    LONG cached = InterlockedCompareExchange(&g_kbo_asian_quota_nation_count, -1, -1);
    if (cached >= 0) {
        return (int)cached;
    }

    LONG marker = InterlockedCompareExchange(&g_kbo_asian_quota_nation_count, -2, -1);
    if (marker != -1) {
        while ((cached = InterlockedCompareExchange(&g_kbo_asian_quota_nation_count, -2, -2)) == -2) {
            SwitchToThread();
        }
        return cached > 0 ? (int)cached : 0;
    }

    int count = 0;
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    for (int i = 0; i < policy->asian_quota_nation_count; i++) {
        kbo_add_asian_quota_nation_id(policy->asian_quota_nation_ids[i], &count);
    }

    char path[MAX_PATH] = {0};
    if (get_kbo_asian_quota_nation_ids_path(path, sizeof(path))) {
        HANDLE file = CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (file != INVALID_HANDLE_VALUE) {
            char raw[4096] = {0};
            DWORD read = 0;
            if (ReadFile(file, raw, sizeof(raw) - 1, &read, NULL) && read > 0) {
                raw[read < sizeof(raw) ? read : sizeof(raw) - 1] = '\0';
                char* cursor = raw;
                while (*cursor != '\0' && count < KBO_ASIAN_QUOTA_NATION_MAX) {
                    while (*cursor != '\0' && (*cursor < '0' || *cursor > '9')) {
                        cursor++;
                    }
                    if (*cursor == '\0') {
                        break;
                    }
                    char* tail = cursor;
                    unsigned long value = strtoul(cursor, &tail, 10);
                    if (tail == cursor) {
                        break;
                    }
                    if (value > 0ul && value <= 1000000ul) {
                        kbo_add_asian_quota_nation_id((uint32_t)value, &count);
                    }
                    cursor = tail;
                }
            }
            CloseHandle(file);
        }
    }

    kbo_log_runtimef("asian quota: loaded nation_ids=%d path=%s", count, path);
    InterlockedExchange(&g_kbo_asian_quota_nation_count, (LONG)count);
    return count;
}

int kbo_nation_is_asian_quota_candidate(uint32_t nation_id)
{
    if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
        return 0;
    }

    int count = kbo_load_asian_quota_nation_ids_once();
    for (int i = 0; i < count; i++) {
        if (g_kbo_asian_quota_nation_ids[i] == nation_id) {
            return 1;
        }
    }
    return 0;
}
