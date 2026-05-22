#ifndef KBOFIX_SRC_HOTKEY_WINDOW_UI_TEXT_BUFFER_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_UI_TEXT_BUFFER_H_

#include <stddef.h>

typedef struct KboWindowTextBuffer {
    char* data;
    size_t capacity;
    size_t length;
} KboWindowTextBuffer;

void kbo_window_text_append_raw(KboWindowTextBuffer* buffer, const char* text, size_t length);
void kbo_window_text_append_char(KboWindowTextBuffer* buffer, char ch);
void kbo_window_text_appendf(KboWindowTextBuffer* buffer, const char* format, ...);
void kbo_html_append_escaped(KboWindowTextBuffer* buffer, const char* text);

#endif
