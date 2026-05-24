#include "../patch_installers_foreign_ai_fa_status_internal.h"

#include <string.h>

int kbo_install_foreign_ai_offer_build_probe_patch(HMODULE exe)
{
    const size_t patch_len = 20;
    const uint8_t expected[20] = {
        0xC6, 0x44, 0x24, 0x20, 0x00,                   /* mov byte ptr [rsp+0x20],0 */
        0x4C, 0x8D, 0x4D, 0x99,                         /* lea r9,[rbp-0x67] */
        0x45, 0x33, 0xC0,                               /* xor r8d,r8d */
        0x49, 0x8B, 0xCC,                               /* mov rcx,r12 */
        0xE8, 0xE0, 0x94, 0xDA, 0xFF                    /* call offer builder */
    };

    uint8_t* target = resolve_patch_target_by_rva_existing_rax_or_pattern(
        exe,
        OOTP27_AI_FA_OFFER_BUILD_PREP_RVA,
        expected,
        sizeof(expected),
        "KBO foreign AI offer build probe patch");
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO foreign AI offer build probe patch already installed target=%p", target);
        return 1;
    }

    uint8_t* stub = build_kbo_foreign_ai_offer_build_probe_stub(target + patch_len);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO foreign AI offer build probe stub");
        return 0;
    }

    uint8_t patch[20] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO foreign AI offer build probe patch error=%lu", GetLastError());
        return 0;
    }
    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO foreign AI offer build probe patch target=%p rva=0x%llx stub=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        &ootp_kbo_foreign_ai_offer_build_probe_wrapper);
    return 1;
}

int kbo_install_foreign_ai_offer_terms_build_probe_patch(HMODULE exe)
{
    const size_t patch_len = 47;
    const uint8_t expected[47] = {
        0x44, 0x88, 0x74, 0x24, 0x30,                   /* mov [rsp+0x30],r14b */
        0x44, 0x88, 0x74, 0x24, 0x28,                   /* mov [rsp+0x28],r14b */
        0x44, 0x88, 0x74, 0x24, 0x20,                   /* mov [rsp+0x20],r14b */
        0x45, 0x33, 0xC9,                               /* xor r9d,r9d */
        0x48, 0x8B, 0x85, 0x40, 0x05, 0x00, 0x00,       /* mov rax,[rbp+0x540] */
        0x44, 0x8B, 0x80, 0x50, 0x44, 0x00, 0x00,       /* mov r8d,[rax+0x4450] */
        0x48, 0x8D, 0x95, 0x90, 0x01, 0x00, 0x00,       /* lea rdx,[rbp+0x190] */
        0x49, 0x8B, 0xCC,                               /* mov rcx,r12 */
        0xE8, 0xB1, 0xDB, 0xDA, 0xFF                    /* call offer terms builder */
    };

    uint8_t* target = resolve_patch_target_by_rva_existing_rax_or_pattern(
        exe,
        OOTP27_AI_FA_OFFER_TERMS_BUILD_PREP_RVA,
        expected,
        sizeof(expected),
        "KBO foreign AI offer terms build probe patch");
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO foreign AI offer terms build probe patch already installed target=%p", target);
        return 1;
    }

    uint8_t* stub = build_kbo_foreign_ai_offer_terms_build_probe_stub(target + patch_len);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO foreign AI offer terms build probe stub");
        return 0;
    }

    uint8_t patch[47];
    memset(patch, 0x90, sizeof(patch));
    patch[0] = 0x48;                                    /* mov rax, stub */
    patch[1] = 0xB8;
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);
    patch[10] = 0xFF;                                   /* jmp rax */
    patch[11] = 0xE0;

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO foreign AI offer terms build probe patch error=%lu", GetLastError());
        return 0;
    }
    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO foreign AI offer terms build probe patch target=%p rva=0x%llx stub=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        &ootp_kbo_foreign_ai_offer_terms_build_probe_wrapper);
    return 1;
}

int kbo_install_foreign_ai_offer_final_gate_probe_patch(HMODULE exe)
{
    const size_t patch_len = 19;
    const uint8_t expected[19] = {
        0x49, 0x8B, 0xD4,                               /* mov rdx,r12 */
        0x49, 0x8B, 0xCD,                               /* mov rcx,r13 */
        0xE8, 0xCD, 0x77, 0x00, 0x00,                   /* call final gate */
        0x84, 0xC0,                                     /* test al,al */
        0x0F, 0x84, 0x8F, 0xEF, 0xFF, 0xFF              /* je failure */
    };

    uint8_t* target = resolve_patch_target_by_rva_existing_rax_or_pattern(
        exe,
        OOTP27_AI_FA_OFFER_FINAL_GATE_RVA,
        expected,
        sizeof(expected),
        "KBO foreign AI offer final gate probe patch");
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO foreign AI offer final gate probe patch already installed target=%p", target);
        return 1;
    }

    uintptr_t failure_delta =
        (uintptr_t)OOTP27_AI_FA_OFFER_FINAL_GATE_RVA
        - (uintptr_t)OOTP27_AI_FA_OFFER_FINAL_GATE_FAILURE_RVA;
    uint8_t* failure = target - failure_delta;
    uint8_t* stub = build_kbo_foreign_ai_offer_final_gate_probe_stub(target + patch_len, failure);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO foreign AI offer final gate probe stub");
        return 0;
    }

    uint8_t patch[19] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO foreign AI offer final gate probe patch error=%lu", GetLastError());
        return 0;
    }
    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO foreign AI offer final gate probe patch target=%p rva=0x%llx stub=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        &ootp_kbo_foreign_ai_offer_final_gate_probe_wrapper);
    return 1;
}
