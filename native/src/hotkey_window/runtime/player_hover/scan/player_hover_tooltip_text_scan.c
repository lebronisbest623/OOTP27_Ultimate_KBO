#include "../capture/player_hover_manager_probe_internal.h"

void kbo_tooltip_appendf(char* out, size_t out_size, size_t* pos, const char* fmt, ...)
{
    if (out == NULL || out_size == 0u || pos == NULL || *pos >= out_size) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(out + *pos, out_size - *pos, fmt, args);
    va_end(args);
    if (written <= 0) {
        return;
    }

    size_t advance = (size_t)written;
    if (advance >= out_size - *pos) {
        *pos = out_size - 1u;
        out[*pos] = '\0';
        return;
    }

    *pos += advance;
}

static int kbo_tooltip_is_ascii_text_byte(uint8_t ch)
{
    return ch == '\t' || ch == ' ' || (ch >= 0x21u && ch <= 0x7eu);
}

static void kbo_tooltip_append_ascii_string(
    char* out,
    size_t out_size,
    size_t* pos,
    const char* label,
    size_t offset,
    const uint8_t* text,
    size_t max_len)
{
    char scratch[KBO_TOOLTIP_SCAN_STRING_MAX + 1u] = {0};
    size_t len = 0u;
    while (len < KBO_TOOLTIP_SCAN_STRING_MAX && len < max_len && kbo_tooltip_is_ascii_text_byte(text[len])) {
        scratch[len] = (char)text[len];
        len += 1u;
    }
    scratch[len] = '\0';
    if (len >= 3u) {
        kbo_tooltip_appendf(out, out_size, pos, "%s+0x%04Ix: %s\n", label, offset, scratch);
    }
}

static void kbo_tooltip_append_utf16_string(
    char* out,
    size_t out_size,
    size_t* pos,
    const char* label,
    size_t offset,
    const uint8_t* text,
    size_t max_bytes)
{
    char scratch[KBO_TOOLTIP_SCAN_STRING_MAX + 1u] = {0};
    size_t len = 0u;
    while ((len + 1u) * sizeof(uint16_t) <= max_bytes && len < KBO_TOOLTIP_SCAN_STRING_MAX) {
        uint16_t ch = *(const uint16_t*)(text + len * sizeof(uint16_t));
        if (ch == 0u || ch > 0x7eu || !kbo_tooltip_is_ascii_text_byte((uint8_t)ch)) {
            break;
        }
        scratch[len] = (char)ch;
        len += 1u;
    }
    scratch[len] = '\0';
    if (len >= 3u) {
        kbo_tooltip_appendf(out, out_size, pos, "%s+0x%04Ix utf16: %s\n", label, offset, scratch);
    }
}

void kbo_tooltip_scan_inline_strings(
    const char* label,
    uint8_t* base,
    size_t bytes,
    char* out,
    size_t out_size,
    size_t* pos)
{
    if (base == NULL || !memory_range_readable(base, bytes)) {
        return;
    }

    for (size_t i = 0u; i + 3u < bytes;) {
        if (kbo_tooltip_is_ascii_text_byte(base[i])
                && kbo_tooltip_is_ascii_text_byte(base[i + 1u])
                && kbo_tooltip_is_ascii_text_byte(base[i + 2u])) {
            kbo_tooltip_append_ascii_string(out, out_size, pos, label, i, base + i, bytes - i);
            while (i < bytes && kbo_tooltip_is_ascii_text_byte(base[i])) {
                i += 1u;
            }
            continue;
        }
        if (i + 6u < bytes
                && kbo_tooltip_is_ascii_text_byte(base[i])
                && base[i + 1u] == 0u
                && kbo_tooltip_is_ascii_text_byte(base[i + 2u])
                && base[i + 3u] == 0u
                && kbo_tooltip_is_ascii_text_byte(base[i + 4u])
                && base[i + 5u] == 0u) {
            kbo_tooltip_append_utf16_string(out, out_size, pos, label, i, base + i, bytes - i);
            while (i + 1u < bytes && base[i] != 0u && base[i + 1u] == 0u) {
                i += 2u;
            }
            continue;
        }
        i += 1u;
    }
}

void kbo_tooltip_scan_pointer_strings(
    void* tooltip,
    char* out,
    size_t out_size,
    size_t* pos)
{
    uint8_t* base = (uint8_t*)tooltip;
    if (!memory_range_readable(base, KBO_TOOLTIP_OBJECT_BYTES)) {
        return;
    }

    for (size_t i = 0u; i + sizeof(uintptr_t) <= KBO_TOOLTIP_OBJECT_BYTES; i += sizeof(uintptr_t)) {
        uintptr_t ptr = *(uintptr_t*)(base + i);
        if (ptr < KBO_RUNTIME_MIN_USER_POINTER) {
            continue;
        }
        uint8_t* target = (uint8_t*)ptr;
        if (memory_range_readable(target, 96u)) {
            if (kbo_tooltip_is_ascii_text_byte(target[0]) && kbo_tooltip_is_ascii_text_byte(target[1])
                    && kbo_tooltip_is_ascii_text_byte(target[2])) {
                kbo_tooltip_append_ascii_string(out, out_size, pos, "ptr", i, target, 96u);
            } else if (target[1] == 0u && target[3] == 0u && target[5] == 0u
                    && kbo_tooltip_is_ascii_text_byte(target[0])
                    && kbo_tooltip_is_ascii_text_byte(target[2])
                    && kbo_tooltip_is_ascii_text_byte(target[4])) {
                kbo_tooltip_append_utf16_string(out, out_size, pos, "ptr", i, target, 96u);
            }
        }
    }
}

int kbo_tooltip_seen_ptr(uintptr_t* seen, size_t seen_count, uintptr_t ptr)
{
    for (size_t i = 0u; i < seen_count; ++i) {
        if (seen[i] == ptr) {
            return 1;
        }
    }
    return 0;
}

void kbo_tooltip_scan_nested_pointer_strings(
    void* tooltip,
    char* out,
    size_t out_size,
    size_t* pos)
{
    uint8_t* base = (uint8_t*)tooltip;
    uintptr_t seen[KBO_TOOLTIP_POINTER_SCAN_LIMIT] = {0};
    size_t seen_count = 0u;
    if (!memory_range_readable(base, KBO_TOOLTIP_OBJECT_BYTES)) {
        return;
    }

    for (size_t i = 0u; i + sizeof(uintptr_t) <= KBO_TOOLTIP_OBJECT_BYTES
            && seen_count < KBO_TOOLTIP_POINTER_SCAN_LIMIT; i += sizeof(uintptr_t)) {
        uintptr_t ptr = *(uintptr_t*)(base + i);
        if (ptr < KBO_RUNTIME_MIN_USER_POINTER || kbo_tooltip_seen_ptr(seen, seen_count, ptr)) {
            continue;
        }

        uint8_t* child = (uint8_t*)ptr;
        if (!memory_range_readable(child, KBO_TOOLTIP_POINTER_SCAN_BYTES)) {
            continue;
        }
        seen[seen_count++] = ptr;

        char label[64] = {0};
        snprintf(label, sizeof(label), "child[0x%04llx]", (unsigned long long)i);
        kbo_tooltip_scan_inline_strings(label, child, KBO_TOOLTIP_POINTER_SCAN_BYTES, out, out_size, pos);

        for (size_t j = 0u; j + sizeof(uintptr_t) <= KBO_TOOLTIP_POINTER_SCAN_BYTES
                && seen_count < KBO_TOOLTIP_POINTER_SCAN_LIMIT; j += sizeof(uintptr_t)) {
            uintptr_t nested_ptr = *(uintptr_t*)(child + j);
            if (nested_ptr < KBO_RUNTIME_MIN_USER_POINTER || kbo_tooltip_seen_ptr(seen, seen_count, nested_ptr)) {
                continue;
            }

            uint8_t* nested = (uint8_t*)nested_ptr;
            if (!memory_range_readable(nested, 160u)) {
                continue;
            }
            seen[seen_count++] = nested_ptr;

            if (kbo_tooltip_is_ascii_text_byte(nested[0])
                    && kbo_tooltip_is_ascii_text_byte(nested[1])
                    && kbo_tooltip_is_ascii_text_byte(nested[2])) {
                snprintf(label, sizeof(label), "child[0x%04llx]->0x%04llx", (unsigned long long)i, (unsigned long long)j);
                kbo_tooltip_append_ascii_string(out, out_size, pos, label, 0u, nested, 160u);
            } else if (nested[1] == 0u && nested[3] == 0u && nested[5] == 0u
                    && kbo_tooltip_is_ascii_text_byte(nested[0])
                    && kbo_tooltip_is_ascii_text_byte(nested[2])
                    && kbo_tooltip_is_ascii_text_byte(nested[4])) {
                snprintf(label, sizeof(label), "child[0x%04llx]->0x%04llx", (unsigned long long)i, (unsigned long long)j);
                kbo_tooltip_append_utf16_string(out, out_size, pos, label, 0u, nested, 160u);
            }
        }
    }
}
