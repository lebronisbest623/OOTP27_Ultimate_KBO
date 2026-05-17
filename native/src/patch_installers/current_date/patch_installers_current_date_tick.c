#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
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

    int installed = 0;
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

    kbo_log_runtimef("KBO current date tick capture hooks install complete ok=%d", installed);
    return installed > 0;
}
