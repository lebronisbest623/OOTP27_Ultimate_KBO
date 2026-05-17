#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/logging/core_log.h"
#include "../allocation/hook_stubs_near_code.h"
#include "../../patch_helpers/patch_helpers.h"
#include "../../runtime_memory/runtime_memory.h"
#include "hook_stubs_current_date_tick.h"

typedef void* (*KboOotpDateAddFn)(void* date_object, int32_t days);

static volatile LONG g_kbo_current_date_tick_date_add_call_count = 0;
static volatile LONG g_kbo_current_date_tick_date_add_match_count = 0;

static uintptr_t kbo_current_date_tick_live_date_object_address(void)
{
    uintptr_t global = get_ootp_cached_global_database();
    if (global == 0u) {
        global = get_ootp_global_database();
    }
    if (global == 0u
            || !memory_range_readable(
                (void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET),
                sizeof(uintptr_t))) {
        return 0u;
    }

    uintptr_t current_date = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    if (current_date == 0u
            || !memory_range_readable(
                (void*)current_date,
                OOTP27_CURRENT_DATE_DAY_OFFSET + sizeof(uint8_t))) {
        return 0u;
    }

    return current_date + OOTP27_LIVE_DATE_OBJECT_OFFSET;
}

void* kbo_current_date_tick_date_add_wrapper(
    void* date_object,
    int32_t days,
    KboOotpDateAddFn original,
    uint32_t site_rva)
{
    void* result = date_object;
    if (original != NULL) {
        result = original(date_object, days);
    }

    uintptr_t object = (uintptr_t)date_object;
    LONG call_no = InterlockedIncrement(&g_kbo_current_date_tick_date_add_call_count);
    uintptr_t live_object = kbo_current_date_tick_live_date_object_address();
    if (object == 0u || object != live_object) {
        if (call_no <= 12 || (call_no % 100000) == 0) {
            kbo_log_runtimef(
                "KBO current date tick date-add sample call=%ld object=%p live=%p days=%ld result=%p",
                (long)call_no,
                (void*)object,
                (void*)live_object,
                (long)days,
                result);
        }
        return result;
    }
    if (!memory_range_readable((void*)object, OOTP27_LIVE_DATE_OBJECT_READABLE_BYTES)) {
        if (call_no <= 12 || (call_no % 100000) == 0) {
            kbo_log_runtimef(
                "KBO current date tick date-add live unreadable call=%ld object=%p days=%ld result=%p",
                (long)call_no,
                (void*)object,
                (long)days,
                result);
        }
        return result;
    }

    const uintptr_t year_offset =
        OOTP27_CURRENT_DATE_YEAR_OFFSET - OOTP27_LIVE_DATE_OBJECT_OFFSET;
    const uintptr_t month_offset =
        OOTP27_CURRENT_DATE_MONTH_OFFSET - OOTP27_LIVE_DATE_OBJECT_OFFSET;
    const uintptr_t day_offset =
        OOTP27_CURRENT_DATE_DAY_OFFSET - OOTP27_LIVE_DATE_OBJECT_OFFSET;
    uint32_t year = *(uint16_t*)(object + year_offset);
    uint32_t month = *(uint8_t*)(object + month_offset);
    uint32_t day = *(uint8_t*)(object + day_offset);

    uint32_t date = year * 10000u + month * 100u + day;
    int published = kbo_current_date_tick_publish(date, site_rva);
    LONG match_no = InterlockedIncrement(&g_kbo_current_date_tick_date_add_match_count);
    if (match_no <= 40 || (match_no % 1000) == 0) {
        kbo_log_runtimef(
            "KBO current date tick date-add live match call=%ld match=%ld object=%p days=%ld date=%u published=%d site=0x%x",
            (long)call_no,
            (long)match_no,
            (void*)object,
            (long)days,
            date,
            published,
            site_rva);
    }
    return result;
}

uint8_t* build_kbo_current_date_tick_date_add_trampoline(
    void* original_address,
    size_t stolen_len)
{
    if (original_address == NULL || stolen_len == 0u || stolen_len > 64u) {
        return NULL;
    }

    size_t code_len = stolen_len + 12u;
    uint8_t* memory = (uint8_t*)VirtualAlloc(
        NULL,
        code_len,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }

    memcpy(memory, original_address, stolen_len);
    memory[stolen_len + 0u] = 0x48; /* mov rax, original + stolen_len */
    memory[stolen_len + 1u] = 0xB8;
    write_u64(
        &memory[stolen_len + 2u],
        (uint64_t)((uintptr_t)original_address + stolen_len));
    memory[stolen_len + 10u] = 0xFF; /* jmp rax */
    memory[stolen_len + 11u] = 0xE0;

    FlushInstructionCache(GetCurrentProcess(), memory, code_len);
    return memory;
}

uint8_t* build_kbo_current_date_tick_date_add_detour_stub(
    void* original_trampoline,
    uint32_t site_rva)
{
    uint8_t code[32] = {
        0x49, 0xB8,                                     /* mov r8, original_trampoline */
        0,0,0,0,0,0,0,0,
        0x41, 0xB9,                                     /* mov r9d, site_rva */
        0,0,0,0,
        0x48, 0xB8,                                     /* mov rax, wrapper */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u32(&code[12], site_rva);
    write_u64(&code[18], (uint64_t)(uintptr_t)&kbo_current_date_tick_date_add_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(
        NULL,
        sizeof(code),
        MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_current_date_tick_capture_stub(
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

    code[n++] = 0x48; code[n++] = 0x8B; code[n++] = 0x83;
    write_u32(&code[n], OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    n += 4;
    code[n++] = 0x48; code[n++] = 0x85; code[n++] = 0xC0; /* test rax, rax */

    code[n++] = 0x0F; code[n++] = 0x84; /* je skip_date */
    size_t skip_date_rel32 = n;
    n += 4;

    code[n++] = 0x0F; code[n++] = 0xB7; code[n++] = 0x90;
    write_u32(&code[n], OOTP27_CURRENT_DATE_YEAR_OFFSET);
    n += 4;
    code[n++] = 0x69; code[n++] = 0xD2;
    write_u32(&code[n], 10000u);
    n += 4;

    code[n++] = 0x44; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x80;
    write_u32(&code[n], OOTP27_CURRENT_DATE_MONTH_OFFSET);
    n += 4;
    code[n++] = 0x45; code[n++] = 0x6B; code[n++] = 0xC0; code[n++] = 100u;
    code[n++] = 0x44; code[n++] = 0x01; code[n++] = 0xC2;

    code[n++] = 0x44; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x80;
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

    code[n++] = 0x48; code[n++] = 0x8B; code[n++] = 0x8B;
    write_u32(&code[n], OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    n += 4;
    code[n++] = 0x48; code[n++] = 0x81; code[n++] = 0xC1;
    write_u32(&code[n], OOTP27_LIVE_DATE_OBJECT_OFFSET);
    n += 4;
    code[n++] = 0xBA;
    write_u32(&code[n], 5u);
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

uint8_t* build_kbo_current_date_tick_capture_stub_global_r12(
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

    code[n++] = 0x49; code[n++] = 0x8B; code[n++] = 0x84; code[n++] = 0x24;
    write_u32(&code[n], OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    n += 4;
    code[n++] = 0x48; code[n++] = 0x85; code[n++] = 0xC0; /* test rax, rax */

    code[n++] = 0x0F; code[n++] = 0x84; /* je skip_date */
    size_t skip_date_rel32 = n;
    n += 4;

    code[n++] = 0x0F; code[n++] = 0xB7; code[n++] = 0x90;
    write_u32(&code[n], OOTP27_CURRENT_DATE_YEAR_OFFSET);
    n += 4;
    code[n++] = 0x69; code[n++] = 0xD2;
    write_u32(&code[n], 10000u);
    n += 4;

    code[n++] = 0x44; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x80;
    write_u32(&code[n], OOTP27_CURRENT_DATE_MONTH_OFFSET);
    n += 4;
    code[n++] = 0x45; code[n++] = 0x6B; code[n++] = 0xC0; code[n++] = 100u;
    code[n++] = 0x44; code[n++] = 0x01; code[n++] = 0xC2;

    code[n++] = 0x44; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x80;
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

    code[n++] = 0x41; code[n++] = 0x58; /* pop r8 */
    code[n++] = 0x5A; /* pop rdx */
    code[n++] = 0x59; /* pop rcx */
    code[n++] = 0x58; /* pop rax */
    code[n++] = 0x9D; /* popfq */

    code[n++] = 0x49; code[n++] = 0x8B; code[n++] = 0x8C; code[n++] = 0x24;
    write_u32(&code[n], OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
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

uint8_t* build_kbo_current_date_tick_capture_stub_global_rax(
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

    code[n++] = 0x4C; code[n++] = 0x8B; code[n++] = 0x80;
    write_u32(&code[n], OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    n += 4;
    code[n++] = 0x4D; code[n++] = 0x85; code[n++] = 0xC0; /* test r8, r8 */

    code[n++] = 0x0F; code[n++] = 0x84; /* je skip_date */
    size_t null_date_rel32 = n;
    n += 4;

    code[n++] = 0x45; code[n++] = 0x0F; code[n++] = 0xB7; code[n++] = 0x90;
    write_u32(&code[n], OOTP27_CURRENT_DATE_YEAR_OFFSET);
    n += 4;
    code[n++] = 0x45; code[n++] = 0x85; code[n++] = 0xD2; /* test r10d, r10d */

    code[n++] = 0x0F; code[n++] = 0x84; /* je skip_date */
    size_t skip_date_rel32 = n;
    n += 4;

    code[n++] = 0x41; code[n++] = 0x69; code[n++] = 0xD2;
    write_u32(&code[n], 10000u);
    n += 4;

    code[n++] = 0x41; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x88;
    write_u32(&code[n], OOTP27_CURRENT_DATE_MONTH_OFFSET);
    n += 4;
    code[n++] = 0x6B; code[n++] = 0xC9; code[n++] = 100u;
    code[n++] = 0x01; code[n++] = 0xCA; /* add edx, ecx */

    code[n++] = 0x41; code[n++] = 0x0F; code[n++] = 0xB6; code[n++] = 0x88;
    write_u32(&code[n], OOTP27_CURRENT_DATE_DAY_OFFSET);
    n += 4;
    code[n++] = 0x01; code[n++] = 0xCA; /* add edx, ecx */

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

    code[n++] = 0x48; code[n++] = 0x8B; code[n++] = 0x90;
    write_u32(&code[n], OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
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
    code[n++] = 0x41; code[n++] = 0x52; /* push r10 */

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
