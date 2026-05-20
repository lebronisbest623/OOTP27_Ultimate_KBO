#include "../capture/player_hover_manager_probe_internal.h"

static int kbo_tooltip_child_contains_any_rating_label(uint8_t* child, size_t bytes)
{
    if (child == NULL || !memory_range_readable(child, bytes)) {
        return 0;
    }

    for (size_t i = 0u; i + 4u < bytes; ++i) {
        if (memcmp(child + i, "OVR", 3u) == 0) {
            return 1;
        } else if (memcmp(child + i, "POT", 3u) == 0) {
            return 1;
        } else if (memcmp(child + i, "STU", 3u) == 0
                || memcmp(child + i, "MOV", 3u) == 0
                || memcmp(child + i, "CON", 3u) == 0
                || memcmp(child + i, "STA", 3u) == 0) {
            return 1;
        }
    }
    return 0;
}

static int kbo_tooltip_child_contains_ascii(uint8_t* child, size_t bytes, const char* needle)
{
    size_t needle_len = 0u;
    if (child == NULL || needle == NULL || !memory_range_readable(child, bytes)) {
        return 0;
    }

    needle_len = strlen(needle);
    if (needle_len == 0u || needle_len > bytes) {
        return 0;
    }

    for (size_t i = 0u; i + needle_len <= bytes; ++i) {
        if (memcmp(child + i, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static size_t kbo_tooltip_find_ascii(uint8_t* bytes, size_t byte_count, const char* needle)
{
    size_t needle_len = 0u;
    if (bytes == NULL || needle == NULL || !memory_range_readable(bytes, byte_count)) {
        return SIZE_MAX;
    }

    needle_len = strlen(needle);
    if (needle_len == 0u || needle_len > byte_count) {
        return SIZE_MAX;
    }

    for (size_t i = 0u; i + needle_len <= byte_count; ++i) {
        if (memcmp(bytes + i, needle, needle_len) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

static int kbo_tooltip_parse_aligned_text_record_number(
    uint8_t* child,
    size_t bytes,
    size_t offset,
    int* out_value,
    int* out_decimal)
{
    int value = 0;
    size_t i = offset;
    int digits = 0;
    if (out_value != NULL) { *out_value = 0; }
    if (out_decimal != NULL) { *out_decimal = 0; }
    if (child == NULL || out_value == NULL || offset >= bytes || !memory_range_readable(child, bytes)) {
        return 0;
    }

    if ((offset & 0x0fu) != 0u || !isdigit((unsigned char)child[offset])) {
        return 0;
    }

    while (i < bytes && isdigit((unsigned char)child[i]) && digits < 4) {
        value = (value * 10) + (int)(child[i] - (uint8_t)'0');
        ++i;
        ++digits;
    }

    if (i < bytes && child[i] == '.') {
        if (out_decimal != NULL) { *out_decimal = 1; }
        return 0;
    }

    if (digits == 0 || digits > 3 || (i < bytes && isdigit((unsigned char)child[i]))) {
        return 0;
    }
    if (i < bytes && child[i] != 0u) {
        return 0;
    }
    if (value <= 0) {
        return 0;
    }

    *out_value = value;
    return 1;
}

static int kbo_tooltip_collect_numbers_after_label(
    uint8_t* child,
    size_t bytes,
    const char* label,
    int* out_values,
    size_t out_values_count)
{
    size_t label_pos = SIZE_MAX;
    size_t scan_start = 0u;
    size_t scan_end = 0u;
    size_t count = 0u;
    if (child == NULL || label == NULL || out_values == NULL || out_values_count == 0u) {
        return 0;
    }

    label_pos = kbo_tooltip_find_ascii(child, bytes, label);
    if (label_pos == SIZE_MAX) {
        return 0;
    }

    scan_start = (label_pos + strlen(label) + 0x0fu) & ~(size_t)0x0f;
    scan_end = label_pos + 0x240u;
    if (scan_end > bytes) {
        scan_end = bytes;
    }

    for (size_t i = scan_start; i + 4u <= scan_end && count < out_values_count; i += 0x10u) {
        int value = 0;
        int decimal = 0;
        if (!kbo_tooltip_parse_aligned_text_record_number(child, bytes, i, &value, &decimal)) {
            if (decimal && count > 0u) {
                break;
            }
            continue;
        }
        out_values[count++] = value;
    }
    return (int)count;
}

static int kbo_tooltip_extract_overall_potential_from_child(
    uint8_t* child,
    size_t bytes,
    int* out_overall,
    int* out_potential)
{
    int ovr_values[2] = {0};
    int pot_values[2] = {0};
    int ovr_count = 0;
    int pot_count = 0;
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_potential != NULL) { *out_potential = 0; }
    if (child == NULL || out_overall == NULL || out_potential == NULL) {
        return 0;
    }

    ovr_count = kbo_tooltip_collect_numbers_after_label(
        child,
        bytes,
        "OVR",
        ovr_values,
        sizeof(ovr_values) / sizeof(ovr_values[0]));
    pot_count = kbo_tooltip_collect_numbers_after_label(
        child,
        bytes,
        "POT",
        pot_values,
        sizeof(pot_values) / sizeof(pot_values[0]));

    if (ovr_count >= 1 && pot_count >= 1) {
        *out_overall = ovr_values[0];
        *out_potential = pot_values[0];
        return 1;
    }

    if (pot_count >= 2) {
        *out_overall = pot_values[0];
        *out_potential = pot_values[1];
        return 1;
    }

    if (ovr_count >= 2) {
        *out_overall = ovr_values[0];
        *out_potential = ovr_values[1];
        return 1;
    }

    return 0;
}

int kbo_tooltip_extract_overall_potential(
    void* tooltip,
    int* out_overall,
    int* out_potential)
{
    uint8_t* base = (uint8_t*)tooltip;
    uintptr_t seen[KBO_TOOLTIP_POINTER_SCAN_LIMIT] = {0};
    size_t seen_count = 0u;
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_potential != NULL) { *out_potential = 0; }
    if (tooltip == NULL || out_overall == NULL || out_potential == NULL
            || !memory_range_readable(base, KBO_TOOLTIP_OBJECT_BYTES)) {
        return 0;
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
        if (kbo_tooltip_extract_overall_potential_from_child(
                    child,
                    KBO_TOOLTIP_POINTER_SCAN_BYTES,
                    out_overall,
                    out_potential)) {
            return 1;
        }

        for (size_t j = 0u; j + sizeof(uintptr_t) <= KBO_TOOLTIP_POINTER_SCAN_BYTES; j += sizeof(uintptr_t)) {
            uintptr_t nested_ptr = *(uintptr_t*)(child + j);
            if (nested_ptr < KBO_RUNTIME_MIN_USER_POINTER || kbo_tooltip_seen_ptr(seen, seen_count, nested_ptr)) {
                continue;
            }
            uint8_t* nested = (uint8_t*)nested_ptr;
            if (!memory_range_readable(nested, KBO_TOOLTIP_POINTER_SCAN_BYTES)) {
                continue;
            }
            if (seen_count < KBO_TOOLTIP_POINTER_SCAN_LIMIT) {
                seen[seen_count++] = nested_ptr;
            }
            if (kbo_tooltip_extract_overall_potential_from_child(
                        nested,
                        KBO_TOOLTIP_POINTER_SCAN_BYTES,
                        out_overall,
                        out_potential)) {
                return 1;
            }
        }
    }
    return 0;
}

void kbo_tooltip_scan_rating_child_values(
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

        int has_rating_label =
            kbo_tooltip_child_contains_any_rating_label(child, KBO_TOOLTIP_POINTER_SCAN_BYTES);
        int has_rating_visual =
            kbo_tooltip_child_contains_ascii(child, KBO_TOOLTIP_POINTER_SCAN_BYTES, "RATING_BAR")
            || kbo_tooltip_child_contains_ascii(child, KBO_TOOLTIP_POINTER_SCAN_BYTES, "rating_panel/images")
            || kbo_tooltip_child_contains_ascii(child, KBO_TOOLTIP_POINTER_SCAN_BYTES, "GRID_TITLE_SMALL_RIGHT");
        if (!has_rating_label && !has_rating_visual) {
            continue;
        }

        kbo_tooltip_appendf(
            out,
            out_size,
            pos,
            "child[0x%04llx] ptr=%p rating_block labels=%d visuals=%d\n",
            (unsigned long long)i,
            (void*)ptr,
            has_rating_label,
            has_rating_visual);

        for (size_t j = 0u; j + 4u < KBO_TOOLTIP_POINTER_SCAN_BYTES; ++j) {
            if (memcmp(child + j, "OVR", 3u) == 0
                    || memcmp(child + j, "POT", 3u) == 0
                    || memcmp(child + j, "STU", 3u) == 0
                    || memcmp(child + j, "MOV", 3u) == 0
                    || memcmp(child + j, "CON", 3u) == 0
                    || memcmp(child + j, "STA", 3u) == 0) {
                char label[4] = {(char)child[j], (char)child[j + 1u], (char)child[j + 2u], '\0'};
                kbo_tooltip_appendf(out, out_size, pos, "  label %s at +0x%04Ix\n", label, j);
                size_t start = j >= 0x30u ? j - 0x30u : 0u;
                size_t end = j + 0x50u < KBO_TOOLTIP_POINTER_SCAN_BYTES ? j + 0x50u : KBO_TOOLTIP_POINTER_SCAN_BYTES;
                for (size_t k = start; k + sizeof(uint16_t) <= end; k += sizeof(uint16_t)) {
                    uint16_t value = *(uint16_t*)(child + k);
                    if (value >= 1u && value <= 250u) {
                        kbo_tooltip_appendf(out, out_size, pos, "    near u16 +0x%04Ix = %u\n", k, (unsigned int)value);
                    }
                }
            }
        }

        for (size_t j = 0u; j + sizeof(uint16_t) <= KBO_TOOLTIP_POINTER_SCAN_BYTES; j += sizeof(uint16_t)) {
            uint16_t value = *(uint16_t*)(child + j);
            if (value >= 1u && value <= 250u) {
                kbo_tooltip_appendf(out, out_size, pos, "  u16 +0x%04Ix = %u\n", j, (unsigned int)value);
            }
        }

        for (size_t j = 0u; j + sizeof(uint32_t) <= KBO_TOOLTIP_POINTER_SCAN_BYTES; j += sizeof(uint32_t)) {
            uint32_t value = *(uint32_t*)(child + j);
            if (value >= 1u && value <= 250u) {
                kbo_tooltip_appendf(out, out_size, pos, "  u32 +0x%04Ix = %u\n", j, (unsigned int)value);
            }
        }

        for (size_t j = 0u; j + sizeof(uintptr_t) <= KBO_TOOLTIP_POINTER_SCAN_BYTES; j += sizeof(uintptr_t)) {
            uintptr_t nested_ptr = *(uintptr_t*)(child + j);
            if (nested_ptr < KBO_RUNTIME_MIN_USER_POINTER || !memory_range_readable((uint8_t*)nested_ptr, 0x80u)) {
                continue;
            }
            uint8_t* nested = (uint8_t*)nested_ptr;
            if (kbo_tooltip_child_contains_ascii(nested, 0x80u, "rating_panel/images")
                    || kbo_tooltip_child_contains_ascii(nested, 0x80u, "RATING_BAR")
                    || kbo_tooltip_child_contains_ascii(nested, 0x80u, "background")) {
                kbo_tooltip_appendf(out, out_size, pos, "  nested +0x%04Ix ptr=%p\n", j, (void*)nested_ptr);
                for (size_t k = 0u; k + sizeof(uint16_t) <= 0x80u; k += sizeof(uint16_t)) {
                    uint16_t value = *(uint16_t*)(nested + k);
                    if (value >= 1u && value <= 250u) {
                        kbo_tooltip_appendf(
                            out,
                            out_size,
                            pos,
                            "    nested u16 +0x%04Ix = %u\n",
                            k,
                            (unsigned int)value);
                    }
                }
            }
        }
    }
}
