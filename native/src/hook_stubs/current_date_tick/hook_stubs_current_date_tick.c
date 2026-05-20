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

