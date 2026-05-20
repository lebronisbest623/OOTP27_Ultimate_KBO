#include "no_minor_foreign_fa_baseline_patch.h"
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

int install_kbo_foreign_fa_demand_baseline_prepare_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    const uint8_t expected[14] = {
        0x41, 0x8B, 0xBD, 0x74, 0x03, 0x00, 0x00,       /* mov edi,[r13+0x374] */
        0x0F, 0xB6, 0xC0,                               /* movzx eax,al */
        0x66, 0x0F, 0x6E, 0xCF                          /* movd xmm1,edi */
    };
    const uint8_t context[64] = {
        0x24, 0x20, 0x00, 0x45, 0x33, 0xC0, 0x33, 0xD2,
        0x48, 0x8B, 0xCB, 0xE8, 0x7C, 0x2C, 0xDB, 0xFF,
        0x41, 0x8B, 0xBD, 0x74, 0x03, 0x00, 0x00, 0x0F,
        0xB6, 0xC0, 0x66, 0x0F, 0x6E, 0xCF, 0x66, 0x0F,
        0x6E, 0xC0, 0xF3, 0x0F, 0xE6, 0xC0, 0xF3, 0x0F,
        0xE6, 0xC9, 0xF2, 0x41, 0x0F, 0x5E, 0xC1, 0xF2,
        0x0F, 0x59, 0xC8, 0x66, 0x0F, 0x2F, 0xCF, 0x76,
        0x0A, 0xF2, 0x0F, 0x58, 0xCE, 0xF2, 0x0F, 0x2C
    };
    const uint8_t context_mask[64] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,0,0,0,0,
        1,1,1,1,0,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1, 0,1,1,1,1,1,1,1
    };

    uint8_t* target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_FOREIGN_FA_DEMAND_BASELINE_PREPARE_AAB624_RVA,
        expected,
        sizeof(expected),
        context,
        context_mask,
        sizeof(context),
        16u,
        "KBO foreign FA demand baseline prepare");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO foreign FA demand baseline prepare already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO foreign FA demand baseline prepare", target, sizeof(expected));
        return 0;
    }

    uint8_t* stub = build_kbo_foreign_fa_demand_baseline_prepare_aab624_stub(
        (uint32_t)((uintptr_t)target - (uintptr_t)exe),
        target + sizeof(expected));
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO foreign FA demand baseline prepare detour stub");
        return 0;
    }

    uint8_t patch[14] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO foreign FA demand baseline prepare error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef("installed KBO foreign FA demand baseline prepare target=%p stub=%p", target, stub);
    return 1;
}
