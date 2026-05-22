#include "ui_js_string.h"

#include <stdio.h>
#include <string.h>

void kbo_webview_append_js_string(KboWindowTextBuffer* buffer, const char* text)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_append_char(buffer, '\'');
    if (text != NULL) {
        const char* segment_start = text;
        for (const char* p = text; *p != '\0'; p++) {
            unsigned char ch = (unsigned char)*p;
            if (ch == '\\' || ch == '\'') {
                kbo_window_text_append_raw(buffer, segment_start, (size_t)(p - segment_start));
                kbo_window_text_append_char(buffer, '\\');
                kbo_window_text_append_char(buffer, (char)ch);
                segment_start = p + 1;
            } else if (ch == '\n') {
                kbo_window_text_append_raw(buffer, segment_start, (size_t)(p - segment_start));
                kbo_window_text_append_raw(buffer, "\\n", 2u);
                segment_start = p + 1;
            } else if (ch == '\r') {
                kbo_window_text_append_raw(buffer, segment_start, (size_t)(p - segment_start));
                kbo_window_text_append_raw(buffer, "\\r", 2u);
                segment_start = p + 1;
            } else if (ch < 0x20u) {
                char escaped[5] = {0};
                snprintf(escaped, sizeof(escaped), "\\x%02x", (unsigned int)ch);
                kbo_window_text_append_raw(buffer, segment_start, (size_t)(p - segment_start));
                kbo_window_text_append_raw(buffer, escaped, 4u);
                segment_start = p + 1;
            }
        }
        kbo_window_text_append_raw(buffer, segment_start, strlen(segment_start));
    }
    kbo_window_text_append_char(buffer, '\'');
}
