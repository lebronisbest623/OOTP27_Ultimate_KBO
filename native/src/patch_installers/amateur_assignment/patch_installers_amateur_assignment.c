#include "patch_installers_amateur_assignment.h"
#include <string.h>
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../build_verify/build_verify.h"
#include "../../core/logging/core_log.h"
#include "../../hook_stubs/amateur_assignment/hook_stubs_amateur_assignment.h"
#include "../../patch_helpers/patch_helpers.h"
#include "../../runtime_memory/runtime_memory.h"

int install_kbo_amateur_assignment_batch_probe_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO amateur assignment batch probe patch");
        return 0;
    }

    const uint8_t expected[OOTP27_AMATEUR_ASSIGNMENT_BATCH_PREP_STOLEN_LEN] = {
        0x4C, 0x8B, 0x65, 0xC8,
        0x44, 0x8B, 0x75, 0xD4,
        0x45, 0x84, 0xFF,
        0x0F, 0x85, 0xCF, 0x00, 0x00, 0x00
    };
    uint8_t* rva_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_AMATEUR_ASSIGNMENT_BATCH_PREP_RVA);
    if (memory_range_readable(rva_target, sizeof(expected)) && is_rax_absolute_jump_patch(rva_target)) {
        kbo_log_runtimef("KBO amateur assignment batch probe patch already installed target=%p", rva_target);
        return 1;
    }
    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_AMATEUR_ASSIGNMENT_BATCH_PREP_RVA,
        expected,
        sizeof(expected),
        "KBO amateur assignment batch probe patch");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO amateur assignment batch probe patch already installed target=%p", target);
        return 1;
    }

    uint8_t* stub = build_kbo_amateur_assignment_batch_probe_stub(
        target + OOTP27_AMATEUR_ASSIGNMENT_BATCH_PREP_STOLEN_LEN);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO amateur assignment batch probe stub");
        return 0;
    }

    uint8_t patch[OOTP27_AMATEUR_ASSIGNMENT_BATCH_PREP_STOLEN_LEN] = {
        0x48, 0xB8,
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,
        0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO amateur assignment batch probe patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO amateur assignment batch probe patch target=%p stub=%p probe=%p",
        target,
        stub,
        &ootp_kbo_amateur_assignment_batch_probe);
    return 1;
}
