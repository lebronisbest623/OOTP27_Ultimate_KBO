#include "core_atomic_file.h"

#include "../save_paths/platform/core_path_io.h"

#include <stdio.h>

HANDLE kbo_atomic_open_tmp(const char* dest_path, char* out_tmp, size_t tmp_size)
{
    if (dest_path == NULL || out_tmp == NULL || tmp_size == 0) {
        return INVALID_HANDLE_VALUE;
    }

    snprintf(out_tmp, tmp_size, "%s.tmp", dest_path);
    HANDLE file = kbo_create_file_utf8(
        out_tmp,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL);
    if (file == INVALID_HANDLE_VALUE) {
        out_tmp[0] = '\0';
    }
    return file;
}

int kbo_atomic_commit(HANDLE file, const char* tmp_path, const char* dest_path)
{
    if (file == INVALID_HANDLE_VALUE || tmp_path == NULL
            || tmp_path[0] == '\0' || dest_path == NULL) {
        return 0;
    }

    CloseHandle(file);
    if (!kbo_move_file_replace_utf8(tmp_path, dest_path)) {
        kbo_delete_file_utf8(tmp_path);
        return 0;
    }
    return 1;
}

void kbo_atomic_abort(HANDLE file, const char* tmp_path)
{
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    if (tmp_path != NULL && tmp_path[0] != '\0') {
        kbo_delete_file_utf8(tmp_path);
    }
}
