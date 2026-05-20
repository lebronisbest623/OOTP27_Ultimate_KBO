#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../independent_acquisition_ai_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "../../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../../core/logging/core_log.h"

static int kbo_independent_acquisition_decision_append_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INDEPENDENT_ACQUISITION_DECISION_FILE,
        out,
        out_size);
}

static void kbo_independent_acquisition_json_append_escaped(
    char* out,
    size_t out_size,
    const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (text == NULL) {
        text = "";
    }

    size_t used = strlen(out);
    for (const unsigned char* p = (const unsigned char*)text; *p != '\0' && used + 2u < out_size; p++) {
        unsigned char ch = *p;
        if (ch == '"' || ch == '\\') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = (char)ch;
        } else if (ch == '\r') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = 'r';
        } else if (ch == '\n') {
            if (used + 3u >= out_size) { break; }
            out[used++] = '\\';
            out[used++] = 'n';
        } else if (ch < 0x20u) {
            if (used + 7u >= out_size) { break; }
            int len = snprintf(out + used, out_size - used, "\\u%04x", (unsigned int)ch);
            if (len <= 0 || used + (size_t)len >= out_size) { break; }
            used += (size_t)len;
        } else {
            out[used++] = (char)ch;
        }
    }
    out[used] = '\0';
}

int kbo_independent_acquisition_append_decision(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int transferred,
    int32_t old_cash,
    int32_t new_cash,
    int32_t seller_transfer_fee,
    int32_t seller_old_cash,
    int32_t seller_new_cash,
    const char* source)
{
    if (today == 0u || request == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_decision_append_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "independent acquisition seller AI decision skipped source=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            (unsigned long)GetLastError(),
            path);
        return 0;
    }

    char request_score_text[32] = {0};
    snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)request->request_score);

    char line[1024] = {0};
    snprintf(
        line,
        sizeof(line),
        "{\"date\":%u,\"season\":%u,\"seller_team_id\":%u,\"player_id\":%u,\"buyer_team_id\":%u,\"request_score\":%s,\"value_score\":%d,\"cash_cost\":%d,\"old_cash\":%d,\"new_cash\":%d,\"seller_transfer_fee\":%d,\"seller_old_cash\":%d,\"seller_new_cash\":%d,\"transferred\":%u,\"source\":\"",
        today,
        request->season,
        request->seller_team_id,
        request->player_id,
        request->buyer_team_id,
        request_score_text,
        request->value_score,
        request->cash_cost,
        old_cash,
        new_cash,
        seller_transfer_fee,
        seller_old_cash,
        seller_new_cash,
        transferred ? 1u : 0u);
    kbo_independent_acquisition_json_append_escaped(line, sizeof(line), source != NULL ? source : "");
    size_t used = strlen(line);
    snprintf(line + used, sizeof(line) - used, "\"}\r\n");

    DWORD written = 0u;
    DWORD len = (DWORD)strlen(line);
    int ok = len > 0u && WriteFile(file, line, len, &written, NULL) && written == len;
    CloseHandle(file);
    return ok;
}
