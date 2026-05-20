/* Save-scoped data directory and file helpers. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../core_save_paths.h"
#include "../core_save_paths_internal.h"
#include "../../../product/ootp_product.h"

static void kbo_sanitize_path_component(const char* text, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (text == NULL || text[0] == '\0') {
        snprintf(out, out_size, "unknown_save");
        return;
    }

    size_t used = 0;
    for (const char* p = text; *p != '\0' && used + 1u < out_size; p++) {
        char ch = *p;
        if ((ch >= 'A' && ch <= 'Z')
                || (ch >= 'a' && ch <= 'z')
                || (ch >= '0' && ch <= '9')
                || ch == '-' || ch == '_' || ch == '.') {
            out[used++] = ch;
        } else {
            out[used++] = '_';
        }
    }
    out[used] = '\0';
    if (out[0] == '\0') {
        snprintf(out, out_size, "unknown_save");
    }
}

static char g_cached_save_scoped_dir_key[KBO_UTF8_PATH_BYTES] = {0};
static char g_cached_save_scoped_dir[KBO_UTF8_PATH_BYTES] = {0};
static volatile LONG g_cached_save_scoped_dir_valid = 0;

int kbo_get_save_scoped_data_dir(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char save_path[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    /* Fast path: return cached dir when save path unchanged. Benign race on
     * the string reads — worst case is a double-compute, not incorrect data. */
    if (InterlockedCompareExchange(&g_cached_save_scoped_dir_valid, 0, 0) != 0
            && g_cached_save_scoped_dir[0] != '\0'
            && strcmp(g_cached_save_scoped_dir_key, save_path) == 0) {
        snprintf(out, out_size, "%s", g_cached_save_scoped_dir);
        return out[0] != '\0';
    }

    char local_app_data[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_localappdata_utf8(local_app_data, sizeof(local_app_data))) {
        return 0;
    }

    char root_dir[KBO_UTF8_PATH_BYTES] = {0};
    snprintf(root_dir, sizeof(root_dir), "%s\\" KBO_PRODUCT_LOCAL_DATA_DIR, local_app_data);
    kbo_create_directory_utf8(root_dir);

    char saves_dir[KBO_UTF8_PATH_BYTES] = {0};
    snprintf(saves_dir, sizeof(saves_dir), "%s\\saves", root_dir);
    kbo_create_directory_utf8(saves_dir);

    char save_name[96] = {0};
    const char* base = strrchr(save_path, '\\');
    const char* slash = strrchr(save_path, '/');
    if (slash != NULL && (base == NULL || slash > base)) {
        base = slash;
    }
    kbo_sanitize_path_component(base != NULL ? base + 1 : save_path, save_name, sizeof(save_name));

    ULONGLONG save_identity = 0ull;
    WIN32_FILE_ATTRIBUTE_DATA save_attrs;
    if (kbo_get_file_attributes_ex_utf8(save_path, &save_attrs)) {
        FILETIME ft = save_attrs.ftCreationTime;
        if (ft.dwLowDateTime == 0u && ft.dwHighDateTime == 0u) {
            ft = save_attrs.ftLastWriteTime;
        }
        save_identity = ((ULONGLONG)ft.dwHighDateTime << 32) | (ULONGLONG)ft.dwLowDateTime;
    }

    char scoped_save_name[128] = {0};
    if (save_identity != 0ull) {
        snprintf(
            scoped_save_name,
            sizeof(scoped_save_name),
            "%s_%08lx%08lx",
            save_name,
            (unsigned long)(save_identity >> 32),
            (unsigned long)(save_identity & 0xffffffffull));
    } else {
        snprintf(scoped_save_name, sizeof(scoped_save_name), "%s", save_name);
    }

    snprintf(out, out_size, "%s\\%s", saves_dir, scoped_save_name);
    kbo_create_directory_utf8(out);

    /* Update cache. */
    snprintf(g_cached_save_scoped_dir_key, sizeof(g_cached_save_scoped_dir_key), "%s", save_path);
    snprintf(g_cached_save_scoped_dir, sizeof(g_cached_save_scoped_dir), "%s", out);
    InterlockedExchange(&g_cached_save_scoped_dir_valid, 1);

    return out[0] != '\0';
}

int kbo_get_save_scoped_data_file(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char dir[MAX_PATH] = {0};
    if (!kbo_get_save_scoped_data_dir(dir, sizeof(dir))) {
        return 0;
    }
    snprintf(out, out_size, "%s\\%s", dir, file_name);
    return out[0] != '\0';
}
