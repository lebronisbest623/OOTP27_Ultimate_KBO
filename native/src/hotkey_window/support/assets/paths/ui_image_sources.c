#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/sync/lock.h"
#include "ui_image_sources.h"

enum {
    KBO_WEBVIEW_IMAGE_SRC_CACHE_MAX = 256,
    KBO_WEBVIEW_IMAGE_SRC_CACHE_MAX_BYTES = 128 * 1024
};

typedef struct KboWebviewImageSrcCacheEntry {
    char path[MAX_PATH];
    char* src;
    size_t src_len;
} KboWebviewImageSrcCacheEntry;

static KboLock g_kbo_webview_image_src_cache_lock = KBO_LOCK_INIT;
static KboWebviewImageSrcCacheEntry g_kbo_webview_image_src_cache[KBO_WEBVIEW_IMAGE_SRC_CACHE_MAX];

static void kbo_webview_append_file_url(KboWindowTextBuffer* buffer, const char* path)
{
    if (buffer == NULL || path == NULL || path[0] == '\0') {
        return;
    }

    kbo_window_text_append_raw(buffer, "file:///", 8u);
    for (const char* p = path; *p != '\0'; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\\') {
            kbo_window_text_append_char(buffer, '/');
        } else if (ch == ' ') {
            kbo_window_text_append_raw(buffer, "%20", 3u);
        } else if (ch == '#') {
            kbo_window_text_append_raw(buffer, "%23", 3u);
        } else if (ch == '%') {
            kbo_window_text_append_raw(buffer, "%25", 3u);
        } else {
            kbo_window_text_append_char(buffer, (char)ch);
        }
    }
}

static char* kbo_webview_make_file_url_alloc(const char* path, size_t* out_len)
{
    if (out_len != NULL) {
        *out_len = 0u;
    }
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    size_t cap = strlen(path) * 3u + 16u;
    char* out = (char*)HeapAlloc(GetProcessHeap(), 0, cap);
    if (out == NULL) {
        return NULL;
    }
    out[0] = '\0';
    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = cap;
    buffer.length = 0;
    kbo_webview_append_file_url(&buffer, path);
    if (out_len != NULL) {
        *out_len = buffer.length;
    }
    return out;
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

static int kbo_webview_try_append_cached_image_src(KboWindowTextBuffer* buffer, const char* path)
{
    if (buffer == NULL || path == NULL || path[0] == '\0' || strlen(path) >= MAX_PATH) {
        return 0;
    }

    char* src = NULL;
    size_t src_len = 0u;
    kbo_lock_enter(&g_kbo_webview_image_src_cache_lock);
    for (int i = 0; i < KBO_WEBVIEW_IMAGE_SRC_CACHE_MAX; i++) {
        KboWebviewImageSrcCacheEntry* entry = &g_kbo_webview_image_src_cache[i];
        if (entry->src != NULL && strcmp(entry->path, path) == 0) {
            src = entry->src;
            src_len = entry->src_len;
            break;
        }
    }
    kbo_lock_leave(&g_kbo_webview_image_src_cache_lock);

    if (src == NULL || src_len == 0u) {
        return 0;
    }
    kbo_window_text_append_raw(buffer, src, src_len);
    return 1;
}

static int kbo_webview_cache_image_src_take(const char* path, char* src, size_t src_len)
{
    if (path == NULL || path[0] == '\0' || src == NULL || src_len == 0u
            || src_len > KBO_WEBVIEW_IMAGE_SRC_CACHE_MAX_BYTES || strlen(path) >= MAX_PATH) {
        return 0;
    }

    int stored = 0;
    kbo_lock_enter(&g_kbo_webview_image_src_cache_lock);
    for (int i = 0; i < KBO_WEBVIEW_IMAGE_SRC_CACHE_MAX; i++) {
        KboWebviewImageSrcCacheEntry* entry = &g_kbo_webview_image_src_cache[i];
        if (entry->src != NULL && strcmp(entry->path, path) == 0) {
            stored = 0;
            goto done;
        }
    }
    for (int i = 0; i < KBO_WEBVIEW_IMAGE_SRC_CACHE_MAX; i++) {
        KboWebviewImageSrcCacheEntry* entry = &g_kbo_webview_image_src_cache[i];
        if (entry->src == NULL) {
            snprintf(entry->path, sizeof(entry->path), "%s", path);
            entry->src = src;
            entry->src_len = src_len;
            stored = 1;
            goto done;
        }
    }

done:
    kbo_lock_leave(&g_kbo_webview_image_src_cache_lock);
    return stored;
}

static char* kbo_webview_make_image_src_alloc(const char* path, size_t* out_len)
{
    if (out_len != NULL) {
        *out_len = 0u;
    }
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    const char* mime = kbo_webview_image_mime_for_path(path);
    if (mime == NULL) {
        return kbo_webview_make_file_url_alloc(path, out_len);
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long file_size = ftell(file);
    if (file_size <= 0 || file_size > 1024 * 1024) {
        fclose(file);
        return NULL;
    }

    size_t encoded_size = (((size_t)file_size + 2u) / 3u) * 4u;
    size_t prefix_size = strlen("data:") + strlen(mime) + strlen(";base64,");
    size_t cap = prefix_size + encoded_size + 1u;
    char* src = (char*)HeapAlloc(GetProcessHeap(), 0, cap);
    unsigned char* data = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)file_size);
    if (src == NULL || data == NULL) {
        if (src != NULL) { HeapFree(GetProcessHeap(), 0, src); }
        if (data != NULL) { HeapFree(GetProcessHeap(), 0, data); }
        fclose(file);
        return NULL;
    }

    rewind(file);
    size_t read = fread(data, 1, (size_t)file_size, file);
    fclose(file);
    if (read != (size_t)file_size) {
        HeapFree(GetProcessHeap(), 0, data);
        HeapFree(GetProcessHeap(), 0, src);
        return NULL;
    }

    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    KboWindowTextBuffer buffer;
    buffer.data = src;
    buffer.capacity = cap;
    buffer.length = 0;
    kbo_window_text_appendf(&buffer, "data:%s;base64,", mime);
    for (size_t i = 0; i < read; i += 3) {
        unsigned int value = ((unsigned int)data[i]) << 16;
        int remaining = (int)(read - i);
        if (remaining > 1) { value |= ((unsigned int)data[i + 1]) << 8; }
        if (remaining > 2) { value |= ((unsigned int)data[i + 2]); }
        char encoded[4] = {
            alphabet[(value >> 18) & 0x3f],
            alphabet[(value >> 12) & 0x3f],
            remaining > 1 ? alphabet[(value >> 6) & 0x3f] : '=',
            remaining > 2 ? alphabet[value & 0x3f] : '='
        };
        kbo_window_text_append_raw(&buffer, encoded, sizeof(encoded));
    }
    HeapFree(GetProcessHeap(), 0, data);
    if (out_len != NULL) {
        *out_len = buffer.length;
    }
    return src;
}

void kbo_webview_append_image_src(KboWindowTextBuffer* buffer, const char* path)
{
    if (buffer == NULL || path == NULL || path[0] == '\0') {
        return;
    }

    if (kbo_webview_try_append_cached_image_src(buffer, path)) {
        return;
    }

    size_t src_len = 0u;
    char* src = kbo_webview_make_image_src_alloc(path, &src_len);
    if (src == NULL || src_len == 0u) {
        if (src != NULL) { HeapFree(GetProcessHeap(), 0, src); }
        return;
    }

    kbo_window_text_append_raw(buffer, src, src_len);
    if (!kbo_webview_cache_image_src_take(path, src, src_len)) {
        HeapFree(GetProcessHeap(), 0, src);
    }
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
