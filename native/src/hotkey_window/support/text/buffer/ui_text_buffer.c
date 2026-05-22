#include "ui_text_buffer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void kbo_window_text_append_raw(KboWindowTextBuffer* buffer, const char* text, size_t length)
{
    if (buffer == NULL || buffer->data == NULL || buffer->capacity == 0
            || buffer->length >= buffer->capacity - 1 || text == NULL || length == 0) {
        return;
    }

    size_t available = buffer->capacity - buffer->length - 1u;
    size_t copy_len = length <= available ? length : available;
    if (copy_len == 0) {
        return;
    }

    memcpy(buffer->data + buffer->length, text, copy_len);
    buffer->length += copy_len;
    buffer->data[buffer->length] = '\0';
}

void kbo_window_text_append_char(KboWindowTextBuffer* buffer, char ch)
{
    if (buffer == NULL || buffer->data == NULL || buffer->capacity == 0
            || buffer->length >= buffer->capacity - 1) {
        return;
    }

    buffer->data[buffer->length++] = ch;
    buffer->data[buffer->length] = '\0';
}

void kbo_window_text_appendf(KboWindowTextBuffer* buffer, const char* format, ...)
{
    if (buffer == NULL || buffer->data == NULL || buffer->capacity == 0
            || buffer->length >= buffer->capacity - 1 || format == NULL) {
        return;
    }

    va_list args;
    va_start(args, format);
    int wrote = vsnprintf(buffer->data + buffer->length, buffer->capacity - buffer->length, format, args);
    va_end(args);

    if (wrote <= 0) {
        return;
    }

    size_t available = buffer->capacity - buffer->length;
    if ((size_t)wrote >= available) {
        buffer->length = buffer->capacity - 1;
        buffer->data[buffer->length] = '\0';
        return;
    }

    buffer->length += (size_t)wrote;
}

void kbo_html_append_escaped(KboWindowTextBuffer* buffer, const char* text)
{
    if (buffer == NULL || text == NULL) {
        return;
    }
    const char* segment_start = text;
    for (const char* p = text; *p != '\0'; p++) {
        const char* escaped = NULL;
        size_t escaped_len = 0u;
        switch (*p) {
        case '&': escaped = "&amp;"; escaped_len = 5u; break;
        case '<': escaped = "&lt;"; escaped_len = 4u; break;
        case '>': escaped = "&gt;"; escaped_len = 4u; break;
        case '"': escaped = "&quot;"; escaped_len = 6u; break;
        case '\'': escaped = "&#39;"; escaped_len = 5u; break;
        default: break;
        }
        if (escaped != NULL) {
            kbo_window_text_append_raw(buffer, segment_start, (size_t)(p - segment_start));
            kbo_window_text_append_raw(buffer, escaped, escaped_len);
            segment_start = p + 1;
        }
    }
    kbo_window_text_append_raw(buffer, segment_start, strlen(segment_start));
}
