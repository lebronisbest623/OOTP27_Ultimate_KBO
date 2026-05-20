#include "no_minor_demand_write_floor_patches.h"
#include <stdio.h>
#include <string.h>
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../build_verify/build_verify.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/dates/core_current_date.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../patch_helpers/patch_helpers.h"
#include "../../../../hook_stubs/foreign_signability_stubs/demand_floor/offer_demand_floor_stubs.h"

int install_kbo_no_minor_contract_demand_write_floor_aab739_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    const uint8_t expected[13] = {
        0x89, 0x83, 0x84, 0x0D, 0x00, 0x00,             /* mov [rbx+0xd84],eax */
        0xE8, 0xFC, 0xF2, 0x20, 0x01,                   /* call 0x141cbaa40 */
        0xEB, 0x4E                                      /* jmp 0x140aab794 */
    };
    const uint8_t expected_mask[13] = {
        1,1,1,1,1,1, 1,0,0,0,0, 1,1
    };
    const uint8_t context[64] = {
        0xD1, 0xF8, 0x80, 0xF9, 0x0B, 0x49, 0x8D, 0x8D,
        0xB8, 0x4A, 0x00, 0x00, 0x41, 0x0F, 0x46, 0xC0,
        0x89, 0x83, 0x84, 0x0D, 0x00, 0x00, 0xE8, 0xFC,
        0xF2, 0x20, 0x01, 0xEB, 0x4E, 0x80, 0xBE, 0xA9,
        0x0B, 0x00, 0x00, 0x00, 0x74, 0x45, 0x48, 0x63,
        0x86, 0xAC, 0x0B, 0x00, 0x00, 0x85, 0xC0, 0x7E,
        0x3A, 0x48, 0x8B, 0x15, 0xBF, 0xEC, 0x73, 0x02,
        0x3B, 0x82, 0xFC, 0x00, 0x00, 0x00, 0x7F, 0x2B
    };
    const uint8_t context_mask[64] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,0, 0,0,0,1,1,1,1,1,
        1,1,1,1,1,0,1,1, 1,1,1,1,1,1,1,1,
        0,1,1,1,0,0,0,0, 1,1,1,1,1,1,1,0
    };

    uint8_t* target = resolve_patch_target_by_rva_or_masked_context_and_expected_pattern(
        exe,
        OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_RVA,
        expected,
        expected_mask,
        sizeof(expected),
        context,
        context_mask,
        sizeof(context),
        16u,
        "KBO no-minor demand write floor AAB739");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO no-minor demand write floor AAB739 already installed target=%p", target);
        return 1;
    }
    if (!kbo_memory_matches_masked_pattern(target, expected, expected_mask, sizeof(expected))) {
        log_patch_bytes_mismatch("KBO no-minor demand write floor AAB739", target, sizeof(expected));
        return 0;
    }

    void* notify_target = resolve_relative_call_target(target + 6);
    if (notify_target == NULL) {
        kbo_log_runtime_line("failed to resolve KBO no-minor demand write floor AAB739 notify call");
        return 0;
    }
    uint8_t* done_target = (uint8_t*)kbo_resolve_build_specific_rva_related_ptr(
        exe,
        target,
        OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_RVA,
        OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_DONE_RVA);
    if (done_target == NULL) {
        kbo_log_runtime_line("failed to resolve KBO no-minor demand write floor AAB739 done target");
        return 0;
    }

    uint8_t* stub = build_kbo_no_minor_demand_write_floor_aab739_stub(
        (uint32_t)((uintptr_t)target - (uintptr_t)exe),
        notify_target,
        done_target);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO no-minor demand write floor AAB739 detour stub");
        return 0;
    }

    uint8_t patch[13] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO no-minor demand write floor AAB739 error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef("installed KBO no-minor demand write floor AAB739 target=%p stub=%p", target, stub);
    return 1;
}

int install_kbo_no_minor_contract_demand_write_floor_1077952_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    const uint8_t expected[13] = {
        0x89, 0x81, 0x84, 0x0D, 0x00, 0x00,             /* mov [rcx+0xd84],eax */
        0x41, 0x8B, 0x97, 0x20, 0x01, 0x00, 0x00        /* mov edx,[r15+0x120] */
    };
    const uint8_t context[64] = {
        0x48, 0x8B, 0x15, 0xBF, 0xED, 0x17, 0x02, 0xE8,
        0x22, 0x51, 0x47, 0xFF, 0x48, 0x8B, 0x4E, 0x08,
        0x89, 0x81, 0x84, 0x0D, 0x00, 0x00, 0x41, 0x8B,
        0x97, 0x20, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x0D,
        0xBA, 0x2A, 0x17, 0x02, 0xE8, 0x25, 0x6B, 0x4C,
        0xFF, 0x48, 0x8B, 0x4E, 0x08, 0x8B, 0x90, 0x74,
        0x03, 0x00, 0x00, 0x39, 0x91, 0x84, 0x0D, 0x00,
        0x00, 0x7E, 0x0A, 0x89, 0x91, 0x84, 0x0D, 0x00
    };
    const uint8_t context_mask[64] = {
        1,1,1,0,0,0,0,1, 0,0,0,0,1,1,1,1,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        0,0,0,0,1,0,0,0, 0,1,1,1,1,1,1,1,
        0,1,1,1,1,1,1,1, 1,1,0,1,1,1,1,1
    };

    uint8_t* target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_1077952_RVA,
        expected,
        sizeof(expected),
        context,
        context_mask,
        sizeof(context),
        16u,
        "KBO no-minor demand write floor 1077952");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO no-minor demand write floor 1077952 already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor demand write floor 1077952", target, sizeof(expected));
        return 0;
    }

    uint8_t* stub = build_kbo_no_minor_demand_write_floor_1077952_stub(
        (uint32_t)((uintptr_t)target - (uintptr_t)exe),
        target + sizeof(expected));
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO no-minor demand write floor 1077952 detour stub");
        return 0;
    }

    uint8_t patch[13] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO no-minor demand write floor 1077952 error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef("installed KBO no-minor demand write floor 1077952 target=%p stub=%p", target, stub);
    return 1;
}
