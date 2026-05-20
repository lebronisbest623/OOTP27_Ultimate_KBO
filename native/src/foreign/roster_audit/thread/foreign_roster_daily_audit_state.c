#include "../internal/foreign_roster_audit_internal.h"
#include "../../../core/dates/constants/kbo_date_constants.h"
#include "../../../core/core_flags/json/json_bool_parser.h"

#define KBO_FOREIGN_ROSTER_DAILY_AUDIT_STATE_FILE "kbo_daily_audit_state.json"

static int kbo_foreign_roster_daily_audit_state_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_FOREIGN_ROSTER_DAILY_AUDIT_STATE_FILE,
        out,
        out_size);
}

uint32_t kbo_foreign_roster_daily_load_last_audit_date(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_foreign_roster_daily_audit_state_path(path, sizeof(path))) {
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

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > KBO_FLAGS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return 0u;
    }

    char* json = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (json == NULL) {
        CloseHandle(file);
        return 0u;
    }

    DWORD read = 0u;
    int ok = ReadFile(file, json, size, &read, NULL) && read == size;
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, json);
        return 0u;
    }

    int value = 0;
    uint32_t result = 0u;
    if (kbo_find_int_value_in_json(json, read, "last_audit_date", &value)
            && value >= (int)KBO_SEASON_DATE_MIN
            && value <= (int)KBO_SIM_DATE_MAX) {
        result = (uint32_t)value;
    } else {
        kbo_log_runtimef(
            "foreign roster daily audit state ignored source=%s reason=invalid_last_audit_date path=%s",
            source != NULL ? source : "",
            path);
    }
    HeapFree(GetProcessHeap(), 0, json);
    return result;
}

void kbo_foreign_roster_daily_persist_last_audit_date(uint32_t today, const char* source)
{
    if (today == 0u) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_foreign_roster_daily_audit_state_path(path, sizeof(path))) {
        return;
    }

    char json[128] = {0};
    int len = snprintf(
        json,
        sizeof(json),
        "{\r\n  \"last_audit_date\": %u\r\n}\r\n",
        today);
    if (len <= 0 || (size_t)len >= sizeof(json)) {
        return;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "foreign roster daily audit state persist skipped source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            today,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    DWORD written = 0u;
    DWORD write_len = (DWORD)len;
    if (!WriteFile(file, json, write_len, &written, NULL) || written != write_len) {
        kbo_log_runtimef(
            "foreign roster daily audit state persist failed source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            today,
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}
