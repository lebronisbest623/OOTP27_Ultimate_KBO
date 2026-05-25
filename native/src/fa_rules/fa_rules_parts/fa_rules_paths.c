#include "fa_rules_paths.h"
#include <stdio.h>
#include <string.h>
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/logging/core_log.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/files/save_paths/platform/core_path_io.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"

static void kbo_fa_rules_paths_copy_text(const char* value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }
    size_t len = strlen(value);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    if (len > 0u) {
        memcpy(out, value, len);
    }
    out[len] = '\0';
}

int kbo_fa_rules_get_localappdata_file_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    return kbo_get_global_data_file(file_name, out, out_size);
}

int kbo_fa_rules_resolve_existing_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char path[KBO_UTF8_PATH_BYTES] = {0};
    if (kbo_get_save_scoped_data_file("config\\fa_rules.json", path, sizeof(path))
            && kbo_get_file_attributes_utf8(path) != INVALID_FILE_ATTRIBUTES) {
        kbo_fa_rules_paths_copy_text(path, out, out_size);
        return 1;
    }

    path[0] = '\0';
    if (kbo_fa_rules_get_localappdata_file_path("fa_rules.json", path, sizeof(path))
            && kbo_get_file_attributes_utf8(path) != INVALID_FILE_ATTRIBUTES) {
        kbo_fa_rules_paths_copy_text(path, out, out_size);
        return 1;
    }

    return 0;
}
