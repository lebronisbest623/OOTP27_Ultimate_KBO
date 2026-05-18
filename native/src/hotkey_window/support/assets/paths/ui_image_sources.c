#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../../../../core/dates/core_text_date.h"
#include "ui_image_sources.h"

static void kbo_webview_append_file_url(KboWindowTextBuffer* buffer, const char* path)
{
    if (buffer == NULL || path == NULL || path[0] == '\0') {
        return;
    }

    kbo_window_text_appendf(buffer, "file:///");
    for (const char* p = path; *p != '\0'; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\\') {
            kbo_window_text_appendf(buffer, "/");
        } else if (ch == ' ') {
            kbo_window_text_appendf(buffer, "%%20");
        } else if (ch == '#') {
            kbo_window_text_appendf(buffer, "%%23");
        } else if (ch == '%') {
            kbo_window_text_appendf(buffer, "%%25");
        } else {
            kbo_window_text_appendf(buffer, "%c", ch);
        }
    }
}

void kbo_webview_copy_file_url(const char* path, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (path == NULL || path[0] == '\0') {
        return;
    }

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;
    kbo_webview_append_file_url(&buffer, path);
}

static const char* kbo_webview_image_mime_for_path(const char* path)
{
    if (path == NULL) {
        return NULL;
    }
    size_t len = strlen(path);
    if (len >= 4 && ascii_equals_ignore_case(path + len - 4, ".png")) {
        return "image/png";
    }
    if (len >= 3 && ascii_equals_ignore_case(path + len - 3, ".oi")) {
        return "image/png";
    }
    if (len >= 4 && ascii_equals_ignore_case(path + len - 4, ".jpg")) {
        return "image/jpeg";
    }
    if (len >= 5 && ascii_equals_ignore_case(path + len - 5, ".jpeg")) {
        return "image/jpeg";
    }
    return NULL;
}

void kbo_webview_append_image_src(KboWindowTextBuffer* buffer, const char* path)
{
    if (buffer == NULL || path == NULL || path[0] == '\0') {
        return;
    }

    const char* mime = kbo_webview_image_mime_for_path(path);
    if (mime == NULL) {
        kbo_webview_append_file_url(buffer, path);
        return;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return;
    }
    long file_size = ftell(file);
    if (file_size <= 0 || file_size > 1024 * 1024) {
        fclose(file);
        return;
    }

    size_t encoded_size = (((size_t)file_size + 2u) / 3u) * 4u;
    size_t prefix_size = strlen("data:") + strlen(mime) + strlen(";base64,");
    size_t available = buffer->capacity > buffer->length ? buffer->capacity - buffer->length : 0u;
    if (prefix_size + encoded_size >= available) {
        fclose(file);
        return;
    }

    rewind(file);

    unsigned char* data = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)file_size);
    if (data == NULL) {
        fclose(file);
        return;
    }
    size_t read = fread(data, 1, (size_t)file_size, file);
    fclose(file);
    if (read != (size_t)file_size) {
        HeapFree(GetProcessHeap(), 0, data);
        return;
    }

    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    kbo_window_text_appendf(buffer, "data:%s;base64,", mime);
    for (size_t i = 0; i < read; i += 3) {
        unsigned int value = ((unsigned int)data[i]) << 16;
        int remaining = (int)(read - i);
        if (remaining > 1) { value |= ((unsigned int)data[i + 1]) << 8; }
        if (remaining > 2) { value |= ((unsigned int)data[i + 2]); }
        kbo_window_text_appendf(buffer, "%c%c%c%c",
            alphabet[(value >> 18) & 0x3f],
            alphabet[(value >> 12) & 0x3f],
            remaining > 1 ? alphabet[(value >> 6) & 0x3f] : '=',
            remaining > 2 ? alphabet[value & 0x3f] : '=');
    }
    HeapFree(GetProcessHeap(), 0, data);
}

void kbo_webview_copy_image_src(const char* path, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (path == NULL || path[0] == '\0') {
        return;
    }

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;
    kbo_webview_append_image_src(&buffer, path);
    if (out[0] == '\0') {
        kbo_webview_copy_file_url(path, out, out_size);
    }
}

void kbo_webview_append_dropdown_logo(KboWindowTextBuffer* buffer, const char* path)
{
    kbo_window_text_appendf(buffer, "<span class='ddLogo'>");
    if (path != NULL && path[0] != '\0') {
        kbo_window_text_appendf(buffer, "<img src='");
        kbo_webview_append_image_src(buffer, path);
        kbo_window_text_appendf(buffer, "'>");
    }
    kbo_window_text_appendf(buffer, "</span>");
}
