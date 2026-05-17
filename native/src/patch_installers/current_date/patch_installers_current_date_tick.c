#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../build_verify/build_verify.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../hook_stubs/current_date_tick/hook_stubs_current_date_tick.h"
#include "../../patch_helpers/patch_helpers.h"
#include "../../runtime_memory/runtime_memory.h"
#include "patch_installers_current_date_tick.h"

typedef uint8_t* (*KboCurrentDateTickStubBuilder)(void* patch_site, uint32_t site_rva);

static int install_kbo_current_date_tick_capture_site(
    HMODULE exe,
    const char* label,
    uint32_t rva,
    const uint8_t* expected,
    size_t expected_size,
    KboCurrentDateTickStubBuilder build_stub)
{
    uint8_t* direct_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, rva);
    if (memory_range_readable(direct_target, 5u) && direct_target[0] == 0xE8) {
        kbo_log_runtimef("%s already installed target=%p", label, direct_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        rva,
        expected,
        expected_size,
        label);
    if (target == NULL) {
        return 0;
    }
    if (!memory_range_readable(target, expected_size)) {
        kbo_log_runtimef("%s skipped target=%p reason=unreadable", label, target);
        return 0;
    }
    if (target[0] == 0xE8) {
        kbo_log_runtimef("%s already installed target=%p", label, target);
        return 1;
    }
    if (memcmp(target, expected, expected_size) != 0) {
        log_patch_bytes_mismatch(
            label,
            target,
            expected_size);
        return 0;
    }

    uint8_t* stub = build_stub(
        target,
        rva);
    if (stub == NULL) {
        kbo_log_runtimef("%s skipped target=%p reason=stub_alloc_failed", label, target);
        return 0;
    }

    intptr_t rel = (intptr_t)stub - ((intptr_t)target + 5);
    if (rel < INT32_MIN || rel > INT32_MAX) {
        kbo_log_runtimef("%s skipped target=%p stub=%p reason=stub_too_far", label, target, stub);
        return 0;
    }

    uint8_t patch[32] = {0};
    if (expected_size > sizeof(patch)) {
        kbo_log_runtimef("%s skipped target=%p reason=patch_too_large size=%u", label, target, (unsigned)expected_size);
        return 0;
    }
    patch[0] = 0xE8;
    write_u32(&patch[1], (uint32_t)(int32_t)rel);
    for (size_t i = 5; i < expected_size; i++) {
        patch[i] = 0x90;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(target, expected_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef(
            "%s VirtualProtect failed target=%p error=%lu",
            label,
            target,
            GetLastError());
        return 0;
    }

    memcpy(target, patch, expected_size);
    FlushInstructionCache(GetCurrentProcess(), target, expected_size);

    DWORD ignored = 0;
    VirtualProtect(target, expected_size, old_protect, &ignored);

    kbo_log_runtimef(
        "%s installed target=%p stub=%p rva=0x%x",
        label,
        target,
        stub,
        rva);
    return 1;
}

static int install_kbo_current_date_tick_date_add_hook(HMODULE exe)
{
    const char* label = "KBO current date tick capture date-add";
    const size_t stolen_len = 15u;
    const uint8_t expected[] = {
        0x40, 0x57,                                     /* rex; push rdi */
        0x44, 0x0F, 0xB6, 0x49, 0x0A,                  /* movzx r9d, byte ptr [rcx+0xa] */
        0x8B, 0xFA,                                     /* mov edi, edx */
        0x4C, 0x8B, 0xD9,                               /* mov r11, rcx */
        0x45, 0x84, 0xC9                                /* test r9b, r9b */
    };

    uint8_t* direct_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_CURRENT_DATE_DATE_ADD_MUTATE_RVA);
    if ((memory_range_readable(direct_target, 12u) && is_rax_absolute_jump_patch(direct_target))
            || (memory_range_readable(direct_target, 13u)
                && direct_target[0] == 0x40
                && is_rax_absolute_jump_patch(direct_target + 1u))) {
        kbo_log_runtimef("%s already installed target=%p", label, direct_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_CURRENT_DATE_DATE_ADD_MUTATE_RVA,
        expected,
        sizeof(expected),
        label);
    if (target == NULL) {
        return 0;
    }
    if ((memory_range_readable(target, 12u) && is_rax_absolute_jump_patch(target))
            || (memory_range_readable(target, 13u)
                && target[0] == 0x40
                && is_rax_absolute_jump_patch(target + 1u))) {
        kbo_log_runtimef("%s already installed target=%p", label, target);
        return 1;
    }
    if (!memory_range_readable(target, sizeof(expected))) {
        kbo_log_runtimef("%s skipped target=%p reason=unreadable", label, target);
        return 0;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch(label, target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_current_date_tick_date_add_trampoline(
        target,
        stolen_len);
    if (trampoline == NULL) {
        kbo_log_runtimef("%s skipped target=%p reason=trampoline_alloc_failed", label, target);
        return 0;
    }

    uint8_t* stub = build_kbo_current_date_tick_date_add_detour_stub(
        trampoline,
        OOTP27_CURRENT_DATE_DATE_ADD_MUTATE_RVA);
    if (stub == NULL) {
        kbo_log_runtimef("%s skipped target=%p reason=stub_alloc_failed", label, target);
        return 0;
    }

    uint8_t patch[15] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef(
            "%s VirtualProtect failed target=%p error=%lu",
            label,
            target,
            GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "%s installed target=%p stub=%p trampoline=%p rva=0x%x",
        label,
        target,
        stub,
        trampoline,
        OOTP27_CURRENT_DATE_DATE_ADD_MUTATE_RVA);
    return 1;
}

int install_kbo_current_date_tick_capture_hook(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("KBO current date tick capture skipped reason=no_exe");
        return 0;
    }

    const uint8_t copy_expected[] = {
        0x48, 0x8B, 0x8B, 0xD8, 0x02, 0x00, 0x00,
        0x48, 0x81, 0xC1, 0xB0, 0x01, 0x00, 0x00,
        0xBA, 0x05, 0x00, 0x00, 0x00
    };
    const uint8_t normalize_expected[] = {
        0x49, 0x8B, 0x8C, 0x24, 0xD8, 0x02, 0x00, 0x00
    };
    const uint8_t series_copy_expected[] = {
        0x44, 0x8B, 0xFF,
        0x41, 0x39, 0xBC, 0x24, 0xFC, 0x01, 0x00, 0x00
    };
    const uint8_t year_sync_expected[] = {
        0x48, 0x8B, 0x8D, 0x38, 0x22, 0x00, 0x00,
        0x33, 0xFF,
        0x48, 0x89, 0x5C, 0x24, 0x60
    };
    const uint8_t sim_loop_expected[] = {
        0x48, 0x8B, 0x90, 0xD8, 0x02, 0x00, 0x00
    };
    const uint8_t sim_loop_post_advance_expected[] = {
        0x48, 0x8B, 0x05, 0x7D, 0xC4, 0xC0, 0x01
    };

    int installed = 0;
    int enable_early_sources = read_kbo_localappdata_flag_file(
        "enable_kbo_current_date_tick_early_sources.txt");
    if (enable_early_sources) {
        installed += install_kbo_current_date_tick_date_add_hook(exe);
        installed += install_kbo_current_date_tick_capture_site(
            exe,
            "KBO current date tick capture copy",
            OOTP27_CURRENT_DATE_COPY_POST_WRITE_RVA,
            copy_expected,
            sizeof(copy_expected),
            build_kbo_current_date_tick_capture_stub);
        installed += install_kbo_current_date_tick_capture_site(
            exe,
            "KBO current date tick capture normalize",
            OOTP27_CURRENT_DATE_NORMALIZE_POST_WRITE_RVA,
            normalize_expected,
            sizeof(normalize_expected),
            build_kbo_current_date_tick_capture_stub_global_r12);
        installed += install_kbo_current_date_tick_capture_site(
            exe,
            "KBO current date tick capture series-copy",
            OOTP27_CURRENT_DATE_SERIES_COPY_POST_WRITE_RVA,
            series_copy_expected,
            sizeof(series_copy_expected),
            build_kbo_current_date_tick_capture_stub_direct_r12);
        installed += install_kbo_current_date_tick_capture_site(
            exe,
            "KBO current date tick capture year-sync",
            OOTP27_CURRENT_DATE_YEAR_SYNC_POST_VALIDATE_RVA,
            year_sync_expected,
            sizeof(year_sync_expected),
            build_kbo_current_date_tick_capture_stub_direct_r9);
        installed += install_kbo_current_date_tick_capture_site(
            exe,
            "KBO current date tick capture sim-loop",
            OOTP27_CURRENT_DATE_SIM_LOOP_GLOBAL_READ_RVA,
            sim_loop_expected,
            sizeof(sim_loop_expected),
            build_kbo_current_date_tick_capture_stub_global_rax);
    } else {
        kbo_log_runtime_line(
            "KBO current date tick early capture sites skipped: using sim-loop-post-advance as primary SSOT");
    }
    installed += install_kbo_current_date_tick_capture_site(
        exe,
        "KBO current date tick capture sim-loop-post-advance",
        OOTP27_CURRENT_DATE_SIM_LOOP_POST_ADVANCE_RVA,
        sim_loop_post_advance_expected,
        sizeof(sim_loop_post_advance_expected),
        build_kbo_current_date_tick_capture_stub_live_date_rbp_0x200_global_rax);

    kbo_log_runtimef("KBO current date tick capture hooks install complete ok=%d", installed);
    return installed > 0;
}
