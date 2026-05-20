#include "foreign_waiver_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../core/files/save_paths/core_save_paths.h"

static HANDLE kbo_open_foreign_policy_config_file(const char* file_name)
{
    if (file_name == NULL || file_name[0] == '\0') {
        return INVALID_HANDLE_VALUE;
    }

    char path[MAX_PATH] = {0};
    if (kbo_get_save_scoped_data_file(file_name, path, sizeof(path))) {
        HANDLE file = CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (file != INVALID_HANDLE_VALUE) {
            return file;
        }
    }

    path[0] = '\0';
    if (kbo_get_global_data_file(file_name, path, sizeof(path))) {
        return CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    }

    return INVALID_HANDLE_VALUE;
}

uint32_t kbo_read_u32_leading_number_from_foreign_policy_file(const char* file_name)
{
    if (file_name == NULL) {
        return 0;
    }

    HANDLE file = kbo_open_foreign_policy_config_file(file_name);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char buf[128] = {0};
    DWORD read = 0;
    if (!ReadFile(file, buf, sizeof(buf) - 1, &read, NULL)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);

    const char* cursor = buf;
    while (*cursor == ' ' || *cursor == '\r' || *cursor == '\n' || *cursor == '\t' || *cursor == ',') {
        cursor++;
    }
    if (*cursor == '\0') {
        return 0;
    }

    char* tail = NULL;
    unsigned long long raw = strtoull(cursor, &tail, 10);
    if (raw == 0ULL || raw > UINT32_MAX) {
        return 0;
    }
    return (uint32_t)raw;
}

uint32_t kbo_get_foreign_waiver_auto_target_team_id(void)
{
    return kbo_read_u32_leading_number_from_foreign_policy_file(KBO_FOREIGN_POLICY_AI_TARGET_TEAM_FILE);
}

int kbo_is_forced_foreign_candidate_id(uint32_t player_id)
{
    if (player_id == 0) {
        return 0;
    }

    HANDLE file = kbo_open_foreign_policy_config_file(KBO_FOREIGN_POLICY_FORCED_PLAYER_IDS_FILE);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char buf[4096] = {0};
    DWORD read = 0;
    if (!ReadFile(file, buf, sizeof(buf) - 1, &read, NULL)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);

    const char* cursor = buf;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\r' || *cursor == '\n' || *cursor == '\t' || *cursor == ',') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        char* tail = NULL;
        unsigned long long raw = strtoull(cursor, &tail, 10);
        if (raw == 0ULL && cursor == tail) {
            break;
        }

        if (raw == (unsigned long long)player_id) {
            return 1;
        }

        cursor = tail;
        while (*cursor != '\0' && *cursor != ',' && *cursor != '\n' && *cursor != '\r') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        while (*cursor == ',' || *cursor == '\r' || *cursor == '\n') {
            cursor++;
        }
    }

    return 0;
}
