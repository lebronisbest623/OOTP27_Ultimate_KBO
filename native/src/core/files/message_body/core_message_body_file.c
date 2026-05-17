#include "core_message_body_file.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../logging/core_log.h"
#include "../../dates/core_current_date.h"
#include "../save_paths/core_save_paths.h"
#include "../save_paths/core_save_paths_internal.h"
#include "../../dates/core_text_date.h"
#include "../../core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

/* Core message body file persistence. */

static int write_kbo_message_body_chunk(HANDLE file, const char* text, DWORD* total_written)
{
    if (file == INVALID_HANDLE_VALUE || total_written == NULL) {
        return 0;
    }
    if (text == NULL || text[0] == '\0') {
        return 1;
    }

    size_t len = strlen(text);
    if (len == 0 || len > 0xffffffffu) {
        return len == 0;
    }

    DWORD written = 0;
    if (!WriteFile(file, text, (DWORD)len, &written, NULL)) {
        return 0;
    }
    *total_written += written;
    return written == (DWORD)len;
}

int write_kbo_message_body_file(uint32_t message_id, const char* title, const char* body, const char* source)
{
    if (message_id == 0 || title == NULL || title[0] == '\0') {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        kbo_log_runtimef("league news body file skipped source=%s title=%s reason=no_save_path", source != NULL ? source : "", title);
        return 0;
    }

    char message_dir[MAX_PATH] = {0};
    snprintf(message_dir, sizeof(message_dir), "%s\\messages", save_path);
    kbo_create_directory_utf8(message_dir);

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\message%u.txt", message_dir, message_id);

    WCHAR wide_path[KBO_WIDE_PATH_CHARS] = {0};
    if (!kbo_utf8_to_wide_path(path, wide_path, KBO_WIDE_PATH_CHARS)) {
        kbo_log_runtimef(
            "league news body file skipped source=%s title=%s id=%u reason=path_convert_failed path=%s",
            source != NULL ? source : "",
            title,
            message_id,
            path);
        return 0;
    }

    HANDLE file = CreateFileW(wide_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "league news body file skipped source=%s title=%s id=%u reason=create_failed gle=%lu path=%s",
            source != NULL ? source : "",
            title,
            message_id,
            GetLastError(),
            path);
        return 0;
    }

    DWORD written = 0;
    const char* body_text = body != NULL ? body : "";
    int ok = write_kbo_message_body_chunk(file, title, &written)
        && write_kbo_message_body_chunk(file, "\r\n", &written)
        && write_kbo_message_body_chunk(file, body_text, &written)
        && write_kbo_message_body_chunk(file, "\r\n", &written);
    CloseHandle(file);

    kbo_log_runtimef(
        "league news body file write source=%s title=%s id=%u ok=%u bytes=%lu encoding=%s path=%s",
        source != NULL ? source : "",
        title,
        message_id,
        ok ? 1u : 0u,
        written,
        "plain",
        path);
    return ok && written > 0;
}
