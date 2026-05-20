/* Global LocalAppData\OOTP-KBO data directory and file helpers. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../core_save_paths.h"
#include "../core_save_paths_internal.h"
#include "../../../product/ootp_product.h"

int kbo_get_global_data_dir(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char local_app_data[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_localappdata_utf8(local_app_data, sizeof(local_app_data))) {
        return 0;
    }

    snprintf(out, out_size, "%s\\" KBO_PRODUCT_LOCAL_DATA_DIR, local_app_data);
    if (out[0] == '\0') {
        return 0;
    }
    kbo_create_directory_utf8(out);
    return 1;
}

int kbo_get_global_data_file(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char dir[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_global_data_dir(dir, sizeof(dir))) {
        return 0;
    }

    snprintf(out, out_size, "%s\\%s", dir, file_name);
    return out[0] != '\0';
}

int kbo_get_global_data_subdir(const char* dir_name, char* out, size_t out_size)
{
    if (dir_name == NULL || dir_name[0] == '\0' || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char root[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_global_data_dir(root, sizeof(root))) {
        return 0;
    }

    snprintf(out, out_size, "%s\\%s", root, dir_name);
    if (out[0] == '\0') {
        return 0;
    }
    kbo_create_directory_utf8(out);
    return 1;
}
