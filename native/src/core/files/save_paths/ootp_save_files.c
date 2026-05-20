/* Helpers for files stored inside an OOTP .lg save directory. */

#include "core_save_paths.h"

#include <stdio.h>

#include "../../product/ootp_product.h"

int kbo_get_ootp_save_file_path(const char* save_path, const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (save_path == NULL || save_path[0] == '\0' || file_name == NULL || file_name[0] == '\0') {
        return 0;
    }

    int written = snprintf(out, out_size, "%s\\%s", save_path, file_name);
    return written > 0 && (size_t)written < out_size;
}

int kbo_get_ootp_save_description_file_path(const char* save_path, char* out, size_t out_size)
{
    return kbo_get_ootp_save_file_path(save_path, KBO_OOTP_SAVE_DESCRIPTION_FILE, out, out_size);
}

int kbo_get_ootp_save_started_file_path(const char* save_path, char* out, size_t out_size)
{
    return kbo_get_ootp_save_file_path(save_path, KBO_OOTP_SAVE_STARTED_FILE, out, out_size);
}

int kbo_get_ootp_save_completed_file_path(const char* save_path, char* out, size_t out_size)
{
    return kbo_get_ootp_save_file_path(save_path, KBO_OOTP_SAVE_COMPLETED_FILE, out, out_size);
}
