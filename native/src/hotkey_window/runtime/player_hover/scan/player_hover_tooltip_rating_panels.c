#include "../capture/player_hover_manager_probe_internal.h"

static uint16_t kbo_tooltip_read_u16(uint8_t* base, size_t offset)
{
    return memory_range_readable(base + offset, sizeof(uint16_t)) ? *(uint16_t*)(base + offset) : 0u;
}

static int16_t kbo_tooltip_read_i16(uint8_t* base, size_t offset)
{
    return memory_range_readable(base + offset, sizeof(int16_t)) ? *(int16_t*)(base + offset) : 0;
}

static uint8_t kbo_tooltip_read_u8(uint8_t* base, size_t offset)
{
    return memory_range_readable(base + offset, sizeof(uint8_t)) ? *(uint8_t*)(base + offset) : 0u;
}

void kbo_tooltip_scan_rating_panels(
    void* tooltip,
    char* out,
    size_t out_size,
    size_t* pos)
{
    uint8_t* base = (uint8_t*)tooltip;
    if (!memory_range_readable(base, KBO_TOOLTIP_OBJECT_BYTES)) {
        return;
    }

    int index = 0;
    for (size_t offset = KBO_TOOLTIP_RATING_PANEL_FIRST_OFFSET;
            offset <= KBO_TOOLTIP_RATING_PANEL_LAST_OFFSET;
            offset += sizeof(uintptr_t), ++index) {
        uintptr_t ptr = *(uintptr_t*)(base + offset);
        if (ptr < KBO_RUNTIME_MIN_USER_POINTER || !memory_range_readable((uint8_t*)ptr, 0x100u)) {
            kbo_tooltip_appendf(out, out_size, pos, "panel%d tooltip+0x%04Ix ptr=NULL\n", index, offset);
            continue;
        }

        uint8_t* panel = (uint8_t*)ptr;
        uint16_t scale_a = kbo_tooltip_read_u16(panel, 0xe8u);
        uint16_t scale_b = kbo_tooltip_read_u16(panel, 0xeau);
        int16_t shown_a = kbo_tooltip_read_i16(panel, 0xecu);
        int16_t shown_b = kbo_tooltip_read_i16(panel, 0xeeu);
        uint8_t visible_a = kbo_tooltip_read_u8(panel, 0xf1u);
        uint8_t visible_b = kbo_tooltip_read_u8(panel, 0xf2u);
        uint8_t mode = kbo_tooltip_read_u8(panel, 0xf6u);
        kbo_tooltip_appendf(
            out,
            out_size,
            pos,
            "panel%d tooltip+0x%04Ix ptr=%p scale_a=%u shown_a=%d scale_b=%u shown_b=%d visible_a=%u visible_b=%u mode=%u\n",
            index,
            offset,
            (void*)ptr,
            (unsigned int)scale_a,
            (int)shown_a,
            (unsigned int)scale_b,
            (int)shown_b,
            (unsigned int)visible_a,
            (unsigned int)visible_b,
            (unsigned int)mode);
    }
}
