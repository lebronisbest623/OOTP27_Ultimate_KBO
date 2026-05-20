#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../allocation/hook_stubs_near_code.h"
#include "../../patch_helpers/patch_helpers.h"
#include "hook_stubs_current_date_tick.h"

uint8_t* build_kbo_current_date_tick_capture_stub_live_date_rbp_0x200_global_rax(
    void* patch_site,
    uint32_t site_rva)
{
    uint8_t code[280] = {0};
    size_t n = 0;
    int32_t original_rip_rel32 = 0;
    memcpy(&original_rip_rel32, (uint8_t*)patch_site + 3u, sizeof(original_rip_rel32));
    uintptr_t original_global_slot = (uintptr_t)((uint8_t*)patch_site + 7u + original_rip_rel32);
    const uint32_t live_year_offset =
        0x200u + OOTP27_CURRENT_DATE_YEAR_OFFSET - OOTP27_LIVE_DATE_OBJECT_OFFSET;
    const uint32_t live_month_offset =
        0x200u + OOTP27_CURRENT_DATE_MONTH_OFFSET - OOTP27_LIVE_DATE_OBJECT_OFFSET;
    const uint32_t live_day_offset =
        0x200u + OOTP27_CURRENT_DATE_DAY_OFFSET - OOTP27_LIVE_DATE_OBJECT_OFFSET;

    code[n++] = 0x9C; /* pushfq */
    code[n++] = 0x50; /* push rax */
    code[n++] = 0x51; /* push rcx */
    code[n++] = 0x52; /* push rdx */
    code[n++] = 0x41; code[n++] = 0x50; /* push r8 */
    code[n++] = 0x41; code[n++] = 0x51; /* push r9 */
    code[n++] = 0x41; code[n++] = 0x52; /* push r10 */
    code[n++] = 0x41; code[n++] = 0x53; /* push r11 */

    code[n++] = 0x44; code[n++] = 0x0F; code[n++] = 0xB7; code[n++] = 0x95;
    write_u32(&code[n], live_year_offset);
    n += 4;
    code[n++] = 0x45; code[n++] = 0x85; code[n++] = 0xD2; /* test r10d, r10d */

    code[n++] = 0x0F; code[n++] = 0x84; /* je skip_date */
    size_t skip_date_rel32 = n;
    n += 4;

    code[n++] = 0x41; code[n++] = 0x69; code[n++] = 0xD2;
    write_u32(&code[n], 10000u);
    n += 4;

    code[n++] = 0x44; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x85;
    write_u32(&code[n], live_month_offset);
    n += 4;
    code[n++] = 0x45; code[n++] = 0x6B; code[n++] = 0xC0; code[n++] = 100u;
    code[n++] = 0x44; code[n++] = 0x01; code[n++] = 0xC2;

    code[n++] = 0x44; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x85;
    write_u32(&code[n], live_day_offset);
    n += 4;
    code[n++] = 0x44; code[n++] = 0x01; code[n++] = 0xC2;

    code[n++] = 0x8B; code[n++] = 0xCA; /* mov ecx, edx */
    code[n++] = 0xBA;
    write_u32(&code[n], site_rva);
    n += 4;
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xEC; code[n++] = 0x28; /* sub rsp, 0x28 */
    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)&kbo_current_date_tick_publish_and_dispatch);
    n += 8;
    code[n++] = 0xFF; code[n++] = 0xD0; /* call rax */
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xC4; code[n++] = 0x28; /* add rsp, 0x28 */

    size_t skip_date_pos = n;
    int32_t skip_delta = (int32_t)(skip_date_pos - (skip_date_rel32 + 4u));
    write_u32(&code[skip_date_rel32], (uint32_t)skip_delta);

    code[n++] = 0x41; code[n++] = 0x5B; /* pop r11 */
    code[n++] = 0x41; code[n++] = 0x5A; /* pop r10 */
    code[n++] = 0x41; code[n++] = 0x59; /* pop r9 */
    code[n++] = 0x41; code[n++] = 0x58; /* pop r8 */
    code[n++] = 0x5A; /* pop rdx */
    code[n++] = 0x59; /* pop rcx */
    code[n++] = 0x58; /* pop rax */
    code[n++] = 0x9D; /* popfq */

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)original_global_slot);
    n += 8;
    code[n++] = 0x48; code[n++] = 0x8B; code[n++] = 0x00;
    code[n++] = 0xC3;

    uint8_t* memory = kbo_alloc_near_code(patch_site, n);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, n);
    FlushInstructionCache(GetCurrentProcess(), memory, n);
    return memory;
}

uint8_t* build_kbo_current_date_tick_capture_stub_direct_r9(
    void* patch_site,
    uint32_t site_rva)
{
    uint8_t code[240] = {0};
    size_t n = 0;

    code[n++] = 0x9C; /* pushfq */
    code[n++] = 0x50; /* push rax */
    code[n++] = 0x51; /* push rcx */
    code[n++] = 0x52; /* push rdx */
    code[n++] = 0x41; code[n++] = 0x50; /* push r8 */
    code[n++] = 0x41; code[n++] = 0x52; /* push r10 */

    code[n++] = 0x4D; code[n++] = 0x85; code[n++] = 0xC9; /* test r9, r9 */
    code[n++] = 0x0F; code[n++] = 0x84; /* je skip_date */
    size_t null_date_rel32 = n;
    n += 4;

    code[n++] = 0x45; code[n++] = 0x0F; code[n++] = 0xB7; code[n++] = 0x91;
    write_u32(&code[n], OOTP27_CURRENT_DATE_YEAR_OFFSET);
    n += 4;
    code[n++] = 0x45; code[n++] = 0x85; code[n++] = 0xD2; /* test r10d, r10d */

    code[n++] = 0x0F; code[n++] = 0x84; /* je skip_date */
    size_t skip_date_rel32 = n;
    n += 4;

    code[n++] = 0x41; code[n++] = 0x69; code[n++] = 0xD2;
    write_u32(&code[n], 10000u);
    n += 4;

    code[n++] = 0x45; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x81;
    write_u32(&code[n], OOTP27_CURRENT_DATE_MONTH_OFFSET);
    n += 4;
    code[n++] = 0x45; code[n++] = 0x6B; code[n++] = 0xC0; code[n++] = 100u;
    code[n++] = 0x44; code[n++] = 0x01; code[n++] = 0xC2;

    code[n++] = 0x45; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x81;
    write_u32(&code[n], OOTP27_CURRENT_DATE_DAY_OFFSET);
    n += 4;
    code[n++] = 0x44; code[n++] = 0x01; code[n++] = 0xC2;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)&g_kbo_current_date_tick_last_published_date);
    n += 8;
    code[n++] = 0x3B; code[n++] = 0x10; /* cmp edx, dword ptr [rax] */
    code[n++] = 0x0F; code[n++] = 0x84; /* je skip_date */
    size_t duplicate_date_rel32 = n;
    n += 4;
    code[n++] = 0x89; code[n++] = 0x10; /* mov dword ptr [rax], edx */

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)&g_kbo_current_date_tick_event_write_cursor);
    n += 8;
    code[n++] = 0xB9;
    write_u32(&code[n], 1u);
    n += 4;
    code[n++] = 0xF0; code[n++] = 0x0F; code[n++] = 0xC1; code[n++] = 0x08;
    code[n++] = 0x81; code[n++] = 0xE1;
    write_u32(&code[n], KBO_CURRENT_DATE_TICK_EVENT_RING_MASK);
    n += 4;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)g_kbo_current_date_tick_event_dates);
    n += 8;
    code[n++] = 0x89; code[n++] = 0x14; code[n++] = 0x88;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)g_kbo_current_date_tick_event_site_rvas);
    n += 8;
    code[n++] = 0xC7; code[n++] = 0x04; code[n++] = 0x88;
    write_u32(&code[n], site_rva);
    n += 4;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)&g_kbo_current_date_tick_event_published_sequence);
    n += 8;
    code[n++] = 0xF0; code[n++] = 0xFF; code[n++] = 0x00;

    size_t skip_date_pos = n;
    int32_t null_delta = (int32_t)(skip_date_pos - (null_date_rel32 + 4u));
    write_u32(&code[null_date_rel32], (uint32_t)null_delta);
    int32_t skip_delta = (int32_t)(skip_date_pos - (skip_date_rel32 + 4u));
    write_u32(&code[skip_date_rel32], (uint32_t)skip_delta);
    int32_t duplicate_delta = (int32_t)(skip_date_pos - (duplicate_date_rel32 + 4u));
    write_u32(&code[duplicate_date_rel32], (uint32_t)duplicate_delta);

    code[n++] = 0x41; code[n++] = 0x5A; /* pop r10 */
    code[n++] = 0x41; code[n++] = 0x58; /* pop r8 */
    code[n++] = 0x5A; /* pop rdx */
    code[n++] = 0x59; /* pop rcx */
    code[n++] = 0x58; /* pop rax */
    code[n++] = 0x9D; /* popfq */

    code[n++] = 0x48; code[n++] = 0x8B; code[n++] = 0x8D;
    write_u32(&code[n], 0x00002238u);
    n += 4;
    code[n++] = 0x33; code[n++] = 0xFF;
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0x5C; code[n++] = 0x24; code[n++] = 0x60;
    code[n++] = 0xC3;

    uint8_t* memory = kbo_alloc_near_code(patch_site, n);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, n);
    FlushInstructionCache(GetCurrentProcess(), memory, n);
    return memory;
}

uint8_t* build_kbo_current_date_tick_capture_stub_direct_r12(
    void* patch_site,
    uint32_t site_rva)
{
    uint8_t code[240] = {0};
    size_t n = 0;

    code[n++] = 0x9C; /* pushfq */
    code[n++] = 0x50; /* push rax */
    code[n++] = 0x51; /* push rcx */
    code[n++] = 0x52; /* push rdx */
    code[n++] = 0x41; code[n++] = 0x50; /* push r8 */
    code[n++] = 0x41; code[n++] = 0x52; /* push r10 */

    code[n++] = 0x45; code[n++] = 0x0F; code[n++] = 0xB7; code[n++] = 0x94; code[n++] = 0x24;
    write_u32(&code[n], OOTP27_CURRENT_DATE_YEAR_OFFSET);
    n += 4;
    code[n++] = 0x45; code[n++] = 0x85; code[n++] = 0xD2; /* test r10d, r10d */

    code[n++] = 0x0F; code[n++] = 0x84; /* je skip_date */
    size_t skip_date_rel32 = n;
    n += 4;

    code[n++] = 0x41; code[n++] = 0x69; code[n++] = 0xD2;
    write_u32(&code[n], 10000u);
    n += 4;

    code[n++] = 0x45; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x84; code[n++] = 0x24;
    write_u32(&code[n], OOTP27_CURRENT_DATE_MONTH_OFFSET);
    n += 4;
    code[n++] = 0x45; code[n++] = 0x6B; code[n++] = 0xC0; code[n++] = 100u;
    code[n++] = 0x44; code[n++] = 0x01; code[n++] = 0xC2;

    code[n++] = 0x45; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x84; code[n++] = 0x24;
    write_u32(&code[n], OOTP27_CURRENT_DATE_DAY_OFFSET);
    n += 4;
    code[n++] = 0x44; code[n++] = 0x01; code[n++] = 0xC2;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)&g_kbo_current_date_tick_last_published_date);
    n += 8;
    code[n++] = 0x3B; code[n++] = 0x10; /* cmp edx, dword ptr [rax] */
    code[n++] = 0x0F; code[n++] = 0x84; /* je skip_date */
    size_t duplicate_date_rel32 = n;
    n += 4;
    code[n++] = 0x89; code[n++] = 0x10; /* mov dword ptr [rax], edx */

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)&g_kbo_current_date_tick_event_write_cursor);
    n += 8;
    code[n++] = 0xB9;
    write_u32(&code[n], 1u);
    n += 4;
    code[n++] = 0xF0; code[n++] = 0x0F; code[n++] = 0xC1; code[n++] = 0x08;
    code[n++] = 0x81; code[n++] = 0xE1;
    write_u32(&code[n], KBO_CURRENT_DATE_TICK_EVENT_RING_MASK);
    n += 4;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)g_kbo_current_date_tick_event_dates);
    n += 8;
    code[n++] = 0x89; code[n++] = 0x14; code[n++] = 0x88;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)g_kbo_current_date_tick_event_site_rvas);
    n += 8;
    code[n++] = 0xC7; code[n++] = 0x04; code[n++] = 0x88;
    write_u32(&code[n], site_rva);
    n += 4;

    code[n++] = 0x48; code[n++] = 0xB8;
    write_u64(&code[n], (uint64_t)(uintptr_t)&g_kbo_current_date_tick_event_published_sequence);
    n += 8;
    code[n++] = 0xF0; code[n++] = 0xFF; code[n++] = 0x00;

    size_t skip_date_pos = n;
    int32_t skip_delta = (int32_t)(skip_date_pos - (skip_date_rel32 + 4u));
    write_u32(&code[skip_date_rel32], (uint32_t)skip_delta);
    int32_t duplicate_delta = (int32_t)(skip_date_pos - (duplicate_date_rel32 + 4u));
    write_u32(&code[duplicate_date_rel32], (uint32_t)duplicate_delta);

    code[n++] = 0x41; code[n++] = 0x5A; /* pop r10 */
    code[n++] = 0x41; code[n++] = 0x58; /* pop r8 */
    code[n++] = 0x5A; /* pop rdx */
    code[n++] = 0x59; /* pop rcx */
    code[n++] = 0x58; /* pop rax */
    code[n++] = 0x9D; /* popfq */

    code[n++] = 0x44; code[n++] = 0x8B; code[n++] = 0xFF;
    code[n++] = 0x41; code[n++] = 0x39; code[n++] = 0xBC; code[n++] = 0x24;
    write_u32(&code[n], 0x000001FCu);
    n += 4;
    code[n++] = 0xC3;

    uint8_t* memory = kbo_alloc_near_code(patch_site, n);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, n);
    FlushInstructionCache(GetCurrentProcess(), memory, n);
    return memory;
}
