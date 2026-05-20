#include "no_minor_submit_salary_floor_patch.h"
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
#include "../../../../hook_stubs/foreign_signability_stubs/demand_floor/submit_salary_floor_stub.h"

int install_kbo_no_minor_contract_submit_salary_floor_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    const uint8_t expected[26] = {
        0x38, 0x9F, 0xCC, 0x00, 0x00, 0x00,             /* cmp [rdi+0xcc], bl */
        0x0F, 0x84, 0xDD, 0x01, 0x00, 0x00,             /* je 0x1413b08fb */
        0x8B, 0x87, 0xC0, 0x00, 0x00, 0x00,             /* mov eax, [rdi+0xc0] */
        0x85, 0xC0,                                     /* test eax, eax */
        0x0F, 0x85, 0x8F, 0x00, 0x00, 0x00              /* jne 0x1413b07bb */
    };
    const uint8_t context[64] = {
        0x8D, 0x55, 0xC8, 0x48, 0x8B, 0xCF, 0xE8, 0x13,
        0x86, 0xCB, 0xFF, 0xE9, 0x6B, 0x03, 0x00, 0x00,
        0x38, 0x9F, 0xCC, 0x00, 0x00, 0x00, 0x0F, 0x84,
        0xDD, 0x01, 0x00, 0x00, 0x8B, 0x87, 0xC0, 0x00,
        0x00, 0x00, 0x85, 0xC0, 0x0F, 0x85, 0x8F, 0x00,
        0x00, 0x00, 0x48, 0x8D, 0x45, 0xE0, 0x48, 0x89,
        0x45, 0x40, 0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05,
        0xFA, 0xDE, 0x80, 0x01, 0x41, 0x8D, 0x51, 0x01
    };
    const uint8_t context_mask[64] = {
        1,1,1,1,1,1,1,0, 0,0,0,1,0,0,0,0,
        1,1,1,1,1,1,1,1, 0,0,0,0,1,1,1,1,
        1,1,1,1,1,1,0,0, 0,0,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1, 0,0,0,0,1,1,1,1
    };

    uint8_t* target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_NO_MINOR_CONTRACT_SUBMIT_SALARY_FLOOR_13B0712_RVA,
        expected,
        sizeof(expected),
        context,
        context_mask,
        sizeof(context),
        16u,
        "KBO no-minor submit salary floor");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO no-minor submit salary floor already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO no-minor submit salary floor", target, sizeof(expected));
        return 0;
    }

    uint8_t* zero_error_target = (uint8_t*)kbo_resolve_build_specific_rva_related_ptr(
        exe,
        target,
        OOTP27_NO_MINOR_CONTRACT_SUBMIT_SALARY_FLOOR_13B0712_RVA,
        OOTP27_NO_MINOR_CONTRACT_SUBMIT_ZERO_ERROR_13B072C_RVA);
    uint8_t* min_check_target = (uint8_t*)kbo_resolve_build_specific_rva_related_ptr(
        exe,
        target,
        OOTP27_NO_MINOR_CONTRACT_SUBMIT_SALARY_FLOOR_13B0712_RVA,
        OOTP27_NO_MINOR_CONTRACT_SUBMIT_MIN_CHECK_13B07BB_RVA);
    uint8_t* after_min_check_target = (uint8_t*)kbo_resolve_build_specific_rva_related_ptr(
        exe,
        target,
        OOTP27_NO_MINOR_CONTRACT_SUBMIT_SALARY_FLOOR_13B0712_RVA,
        OOTP27_NO_MINOR_CONTRACT_SUBMIT_AFTER_MIN_CHECK_13B0860_RVA);
    if (zero_error_target == NULL || min_check_target == NULL || after_min_check_target == NULL) {
        kbo_log_runtime_line("KBO no-minor submit salary floor skipped: branch targets unresolved");
        return 0;
    }

    uint8_t* stub = build_kbo_no_minor_contract_submit_salary_floor_stub(
        zero_error_target,
        min_check_target,
        after_min_check_target);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO no-minor submit salary floor detour stub");
        return 0;
    }

    uint8_t patch[26] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO no-minor submit salary floor error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO no-minor submit salary floor target=%p stub=%p floor_source=[r14+0x278]",
        target,
        stub);
    return 1;
}
