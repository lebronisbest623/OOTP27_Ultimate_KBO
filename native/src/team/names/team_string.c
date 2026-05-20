#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include "team_string.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/abi/ootp_typedefs.h"
#include "../../build_verify/build_verify.h"
#include "../../core/dates/core_text_date.h"
#include "../../runtime_memory/runtime_memory.h"
/* String helpers (from league_team_rules) */

static int kbo_memory_protect_allows_read(DWORD protect)
{
    if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0) {
        return 0;
    }
    protect &= 0xffu;
    return protect == PAGE_READONLY
        || protect == PAGE_READWRITE
        || protect == PAGE_WRITECOPY
        || protect == PAGE_EXECUTE_READ
        || protect == PAGE_EXECUTE_READWRITE
        || protect == PAGE_EXECUTE_WRITECOPY;
}

static size_t kbo_readable_prefix_bytes(const char* source, size_t max_bytes)
{
    if (source == NULL || max_bytes == 0u) {
        return 0u;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(source, &mbi, sizeof(mbi)) == 0
            || mbi.State != MEM_COMMIT
            || !kbo_memory_protect_allows_read(mbi.Protect)) {
        return 0u;
    }

    uintptr_t start = (uintptr_t)source;
    uintptr_t region_end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    if (region_end <= start) {
        return 0u;
    }
    size_t available = (size_t)(region_end - start);
    return available < max_bytes ? available : max_bytes;
}

int copy_limited_ascii_string(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    size_t readable = kbo_readable_prefix_bytes(source, 512u);
    if (readable == 0u) {
        return 0;
    }

    size_t len = 0;
    for (; len + 1 < out_size && len < readable; len++) {
        unsigned char c = (unsigned char)source[len];
        if (c == '\0') {
            break;
        }
        if (c < 0x20u || c > 0x7eu) {
            break;
        }
        out[len] = (char)c;
    }

    out[len] = '\0';
    return len > 0;
}

int copy_limited_ootp_internal_string(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    size_t readable = kbo_readable_prefix_bytes(source, 512u);
    if (readable == 0u) {
        return 0;
    }

    size_t len = 0;
    for (; len + 1 < out_size && len < readable; len++) {
        unsigned char c = (unsigned char)source[len];
        if (c == '\0') {
            break;
        }
        if (c < 0x20u && c != 0x01u) {
            break;
        }
        out[len] = (char)c;
    }

    out[len] = '\0';
    return len > 0;
}

static int kbo_ootp_hex_value(unsigned char c)
{
    if (c >= '0' && c <= '9') {
        return (int)(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return (int)(c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
        return (int)(c - 'A' + 10);
    }
    return -1;
}

static int kbo_append_utf8_codepoint(char* out, size_t out_size, size_t* pos, uint32_t cp)
{
    if (out == NULL || out_size == 0u || pos == NULL || cp == 0u
            || cp > 0x10ffffu || (cp >= 0xd800u && cp <= 0xdfffu)) {
        return 0;
    }
    if (cp < 0x80u) {
        if (*pos + 1u >= out_size) {
            return 0;
        }
        out[(*pos)++] = (char)cp;
        return 1;
    }
    if (cp < 0x800u) {
        if (*pos + 2u >= out_size) {
            return 0;
        }
        out[(*pos)++] = (char)(0xc0u | (cp >> 6));
        out[(*pos)++] = (char)(0x80u | (cp & 0x3fu));
        return 1;
    }
    if (cp < 0x10000u) {
        if (*pos + 3u >= out_size) {
            return 0;
        }
        out[(*pos)++] = (char)(0xe0u | (cp >> 12));
        out[(*pos)++] = (char)(0x80u | ((cp >> 6) & 0x3fu));
        out[(*pos)++] = (char)(0x80u | (cp & 0x3fu));
        return 1;
    }
    if (*pos + 4u >= out_size) {
        return 0;
    }
    out[(*pos)++] = (char)(0xf0u | (cp >> 18));
    out[(*pos)++] = (char)(0x80u | ((cp >> 12) & 0x3fu));
    out[(*pos)++] = (char)(0x80u | ((cp >> 6) & 0x3fu));
    out[(*pos)++] = (char)(0x80u | (cp & 0x3fu));
    return 1;
}

int copy_limited_ootp_display_string(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    size_t readable = kbo_readable_prefix_bytes(source, 512u);
    if (readable == 0u) {
        return 0;
    }

    size_t pos = 0u;
    int unicode_mode = 0;
    for (size_t i = 0u; i < readable && pos + 1u < out_size; ) {
        unsigned char c = (unsigned char)source[i];
        if (c == '\0') {
            break;
        }
        if (c == 0x01u) {
            unicode_mode = !unicode_mode;
            i++;
            continue;
        }
        if (unicode_mode && c == ',') {
            if (i + 5u > readable) {
                break;
            }
            int h0 = kbo_ootp_hex_value((unsigned char)source[i + 1u]);
            int h1 = kbo_ootp_hex_value((unsigned char)source[i + 2u]);
            int h2 = kbo_ootp_hex_value((unsigned char)source[i + 3u]);
            int h3 = kbo_ootp_hex_value((unsigned char)source[i + 4u]);
            if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) {
                uint32_t cp = ((uint32_t)h0 << 12)
                    | ((uint32_t)h1 << 8)
                    | ((uint32_t)h2 << 4)
                    | (uint32_t)h3;
                if (!kbo_append_utf8_codepoint(out, out_size, &pos, cp)) {
                    break;
                }
                i += 5u;
                continue;
            }
        }
        if (c < 0x20u) {
            break;
        }
        out[pos++] = (char)c;
        i++;
    }

    out[pos] = '\0';
    return pos > 0u;
}

int kbo_ootp_text_has_non_ascii(const char* text)
{
    if (text == NULL) {
        return 0;
    }
    for (const unsigned char* p = (const unsigned char*)text; *p != '\0'; p++) {
        if (*p >= 0x80u) {
            return 1;
        }
    }
    return 0;
}

int copy_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size)
{
    uint8_t* string_object = object_base + string_offset;
    uint8_t* text_slot     = string_object + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET;
    if (!memory_range_readable(text_slot, sizeof(char*))) {
        return 0;
    }

    const char* text = *(const char**)text_slot;
    return copy_limited_ootp_display_string(text, out, out_size);
}

int copy_ootp_string_object_raw_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size)
{
    uint8_t* string_object = object_base + string_offset;
    uint8_t* text_slot     = string_object + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET;
    if (!memory_range_readable(text_slot, sizeof(char*))) {
        if (out != NULL && out_size > 0) {
            out[0] = '\0';
        }
        return 0;
    }

    const char* text = *(const char**)text_slot;
    return copy_limited_ootp_internal_string(text, out, out_size);
}

static OotpCoreStringAssignFn get_ootp_string_assign_fn(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    OotpCoreStringAssignFn assign_string =
        (OotpCoreStringAssignFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PISD_STRING_ASSIGN_RVA);
    if (!memory_range_readable((void*)assign_string, 16)) {
        return NULL;
    }

    return assign_string;
}

int assign_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, const char* text)
{
    if (object_base == NULL || text == NULL || text[0] == '\0') {
        return 0;
    }

    uint8_t* string_object = object_base + string_offset;
    if (!memory_range_readable(string_object, 0x18)) {
        return 0;
    }

    OotpCoreStringAssignFn assign_string = get_ootp_string_assign_fn();
    if (assign_string == NULL) {
        return 0;
    }

    assign_string(string_object, text);
    return 1;
}

int assign_ootp_string_object_text_if_different(uint8_t* object_base, uint32_t string_offset, const char* text)
{
    char current[128] = {0};
    if (copy_ootp_string_object_text(object_base, string_offset, current, sizeof(current))
            && ascii_equals_ignore_case(current, text)) {
        return 0;
    }

    return assign_ootp_string_object_text(object_base, string_offset, text);
}

int team_has_ootp_string_text(uint8_t* team, const char* expected)
{
    if (team == NULL || expected == NULL || expected[0] == '\0') {
        return 0;
    }

    static const uint32_t string_offsets[] = OOTP27_KBO_TEAM_SEARCH_STRING_OFFSETS;

    char text[96] = {0};
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        text[0] = '\0';
        if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))
                && ascii_equals_ignore_case(text, expected)) {
            return 1;
        }
    }

    return 0;
}

static int kbo_string_contains_ootp_text(const char* text, const char* expected)
{
    if (text == NULL || expected == NULL || expected[0] == '\0') {
        return 0;
    }
    if (strstr(text, expected) != NULL) {
        return 1;
    }

    size_t expected_len = strlen(expected);
    for (const char* p = text; *p != '\0'; p++) {
        size_t i = 0u;
        for (; i < expected_len; i++) {
            char a = p[i];
            char b = expected[i];
            if (a == '\0') {
                return 0;
            }
            if (a >= 'A' && a <= 'Z') {
                a = (char)(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = (char)(b - 'A' + 'a');
            }
            if (a != b) {
                break;
            }
        }
        if (i == expected_len) {
            return 1;
        }
    }

    return 0;
}

int team_contains_ootp_string_text(uint8_t* team, const char* expected)
{
    if (team == NULL || expected == NULL || expected[0] == '\0') {
        return 0;
    }

    static const uint32_t string_offsets[] = OOTP27_KBO_TEAM_SEARCH_STRING_OFFSETS;

    char text[128] = {0};
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        text[0] = '\0';
        if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))
                && kbo_string_contains_ootp_text(text, expected)) {
            return 1;
        }
    }

    return 0;
}


