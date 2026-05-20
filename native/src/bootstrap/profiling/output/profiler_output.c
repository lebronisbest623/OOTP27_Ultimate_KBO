#include "profiler_output.h"

#include <stdio.h>
#include <string.h>

#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/product/ootp_product.h"

static volatile LONG g_kbo_profiler_header_written = 0;

static int kbo_profiler_get_output_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    if (!kbo_get_global_data_subdir(KBO_PRODUCT_PERF_DIR, dir, sizeof(dir))) {
        return 0;
    }

    snprintf(out, out_size, "%s\\" KBO_PRODUCT_PERF_FILE_FORMAT, dir, GetCurrentProcessId());
    return 1;
}

void kbo_profiler_write_bytes(const char* data, DWORD size)
{
    if (data == NULL || size == 0) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_profiler_get_output_path(path, sizeof(path))) {
        return;
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
        return;
    }

    DWORD written = 0;
    WriteFile(file, data, size, &written, NULL);
    CloseHandle(file);
}

void kbo_profiler_write_header_if_needed(void)
{
    if (InterlockedCompareExchange(&g_kbo_profiler_header_written, 1, 0) != 0) {
        return;
    }

    const char* header =
        "timestamp,pid,thread,zone,total_calls,delta_calls,total_us,delta_us,"
        "avg_us,max_us,total_slow_calls,delta_slow_calls,window_ms\r\n";
    kbo_profiler_write_bytes(header, (DWORD)strlen(header));
}
