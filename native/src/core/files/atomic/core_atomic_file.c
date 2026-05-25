#include "core_atomic_file.h"

#include "../save_paths/platform/core_path_io.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define KBO_ATOMIC_COMPARE_BUFFER_BYTES 32768u

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

static uint64_t kbo_atomic_file_size(const WIN32_FILE_ATTRIBUTE_DATA* attrs)
{
    if (attrs == NULL) {
        return 0u;
    }
    return ((uint64_t)attrs->nFileSizeHigh << 32) | (uint64_t)attrs->nFileSizeLow;
}

static int kbo_atomic_files_equal(const char* left_path, const char* right_path)
{
    WIN32_FILE_ATTRIBUTE_DATA left_attrs;
    WIN32_FILE_ATTRIBUTE_DATA right_attrs;
    if (!kbo_get_file_attributes_ex_utf8(left_path, &left_attrs)
            || !kbo_get_file_attributes_ex_utf8(right_path, &right_attrs)) {
        return 0;
    }
    if (kbo_atomic_file_size(&left_attrs) != kbo_atomic_file_size(&right_attrs)) {
        return 0;
    }

    HANDLE left = kbo_create_file_utf8(
        left_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL);
    if (left == INVALID_HANDLE_VALUE) {
        return 0;
    }

    HANDLE right = kbo_create_file_utf8(
        right_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL);
    if (right == INVALID_HANDLE_VALUE) {
        CloseHandle(left);
        return 0;
    }

    char left_buffer[KBO_ATOMIC_COMPARE_BUFFER_BYTES];
    char right_buffer[KBO_ATOMIC_COMPARE_BUFFER_BYTES];
    int equal = 1;
    for (;;) {
        DWORD left_read = 0;
        DWORD right_read = 0;
        if (!ReadFile(left, left_buffer, sizeof(left_buffer), &left_read, NULL)
                || !ReadFile(right, right_buffer, sizeof(right_buffer), &right_read, NULL)) {
            equal = 0;
            break;
        }
        if (left_read != right_read || memcmp(left_buffer, right_buffer, left_read) != 0) {
            equal = 0;
            break;
        }
        if (left_read == 0) {
            break;
        }
    }

    CloseHandle(right);
    CloseHandle(left);
    return equal;
}

int kbo_atomic_commit(HANDLE file, const char* tmp_path, const char* dest_path)
{
    if (file == INVALID_HANDLE_VALUE || tmp_path == NULL
            || tmp_path[0] == '\0' || dest_path == NULL) {
        return 0;
    }

    CloseHandle(file);
    if (kbo_atomic_files_equal(tmp_path, dest_path)) {
        kbo_delete_file_utf8(tmp_path);
        return 1;
    }

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
