#include "../patch_installers_foreign_ai_fa_status.h"
#include <stdio.h>
#include <string.h>
#include "../../../common/patch_host.h"
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/dates/core_current_date.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../build_verify/build_verify.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../patch_helpers/patch_helpers.h"
#include "../../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../../hook_stubs/foreign/ai_status/hook_stubs_foreign_ai_status.h"
#include "../../../../hook_stubs/military/hook_stubs_military.h"

typedef uint8_t* (*KboForeignAiOfferTermsBuildProbeStubBuilder)(void* continuation);

static int kbo_install_foreign_ai_offer_build_probe_patch(HMODULE exe)
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

static int kbo_install_foreign_ai_offer_terms_build_probe_site_patch(
    HMODULE exe,
    uint32_t target_rva,
    const uint8_t* expected,
    size_t patch_len,
    const char* label,
    KboForeignAiOfferTermsBuildProbeStubBuilder build_stub)
{
    if (patch_len < 12u || patch_len > 64u || expected == NULL || build_stub == NULL) {
        return 0;
    }

    uint8_t* target = resolve_patch_target_by_rva_existing_rax_or_pattern(
        exe,
        target_rva,
        expected,
        patch_len,
        label);
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("%s already installed target=%p", label, target);
        return 1;
    }

    uint8_t* stub = build_stub(target + patch_len);
    if (stub == NULL) {
        kbo_log_runtimef("failed to allocate %s stub", label);
        return 0;
    }

    uint8_t patch[64];
    memset(patch, 0x90, sizeof(patch));
    patch[0] = 0x48;                                    /* mov rax, stub */
    patch[1] = 0xB8;
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);
    patch[10] = 0xFF;                                   /* jmp rax */
    patch[11] = 0xE0;

    DWORD old_protect = 0;
    if (!VirtualProtect(target, patch_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }
    memcpy(target, patch, patch_len);
    FlushInstructionCache(GetCurrentProcess(), target, patch_len);
    DWORD ignored = 0;
    VirtualProtect(target, patch_len, old_protect, &ignored);

    kbo_log_runtimef(
        "installed %s target=%p rva=0x%llx stub=%p wrapper=%p",
        label,
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        &ootp_kbo_foreign_ai_offer_terms_build_probe_wrapper);
    return 1;
}

static int kbo_install_foreign_ai_offer_terms_build_probe_patch(HMODULE exe)
{
    const uint8_t expected_main[47] = {
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
    int main_ok = kbo_install_foreign_ai_offer_terms_build_probe_site_patch(
        exe,
        OOTP27_AI_FA_OFFER_TERMS_BUILD_PREP_RVA,
        expected_main,
        sizeof(expected_main),
        "KBO foreign AI offer terms build probe patch",
        build_kbo_foreign_ai_offer_terms_build_probe_stub);

    const uint8_t expected_final_create[40] = {
        0xC6, 0x44, 0x24, 0x30, 0x00,                   /* mov [rsp+0x30],0 */
        0xC6, 0x44, 0x24, 0x28, 0x00,                   /* mov [rsp+0x28],0 */
        0xC6, 0x44, 0x24, 0x20, 0x00,                   /* mov [rsp+0x20],0 */
        0x41, 0xB1, 0x01,                               /* mov r9b,1 */
        0x45, 0x8B, 0x85, 0x50, 0x44, 0x00, 0x00,       /* mov r8d,[r13+0x4450] */
        0x48, 0x8D, 0x95, 0x80, 0x01, 0x00, 0x00,       /* lea rdx,[rbp+0x180] */
        0x48, 0x8B, 0xCE,                               /* mov rcx,rsi */
        0xE8, 0xF9, 0x6D, 0xDB, 0xFF                    /* call offer terms builder */
    };
    int final_create_ok = kbo_install_foreign_ai_offer_terms_build_probe_site_patch(
        exe,
        OOTP27_AI_FA_OFFER_TERMS_BUILD_PREP_FINAL_CREATE_RVA,
        expected_final_create,
        sizeof(expected_final_create),
        "KBO foreign AI offer terms final-create probe patch",
        build_kbo_foreign_ai_offer_terms_build_probe_final_create_stub);

    const uint8_t expected_final_create_alt[37] = {
        0xC6, 0x44, 0x24, 0x30, 0x00,                   /* mov [rsp+0x30],0 */
        0xC6, 0x44, 0x24, 0x28, 0x00,                   /* mov [rsp+0x28],0 */
        0xC6, 0x44, 0x24, 0x20, 0x00,                   /* mov [rsp+0x20],0 */
        0x45, 0x33, 0xC9,                               /* xor r9d,r9d */
        0x45, 0x8B, 0x85, 0x50, 0x44, 0x00, 0x00,       /* mov r8d,[r13+0x4450] */
        0x48, 0x8D, 0x55, 0x90,                         /* lea rdx,[rbp-0x70] */
        0x48, 0x8B, 0xCE,                               /* mov rcx,rsi */
        0xE8, 0x56, 0xB7, 0xDA, 0xFF                    /* call offer terms builder */
    };
    int final_create_alt_ok = kbo_install_foreign_ai_offer_terms_build_probe_site_patch(
        exe,
        OOTP27_AI_FA_OFFER_TERMS_BUILD_PREP_FINAL_CREATE_ALT_RVA,
        expected_final_create_alt,
        sizeof(expected_final_create_alt),
        "KBO foreign AI offer terms final-create-alt probe patch",
        build_kbo_foreign_ai_offer_terms_build_probe_final_create_alt_stub);

    return main_ok && final_create_ok && final_create_alt_ok;
}

static int kbo_install_foreign_ai_offer_final_gate_probe_patch(HMODULE exe)
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

int install_kbo_foreign_ai_offer_attach_probe_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO foreign AI offer attach probe patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO foreign AI offer attach probe patch host=%s", host);
        return 0;
    }

    const size_t stolen_len = 16;
    const uint8_t expected[16] = {
        0x48, 0x89, 0x5C, 0x24, 0x18,                   /* mov [rsp+0x18],rbx */
        0x55,                                           /* push rbp */
        0x56,                                           /* push rsi */
        0x57,                                           /* push rdi */
        0x41, 0x54,                                     /* push r12 */
        0x41, 0x55,                                     /* push r13 */
        0x41, 0x56,                                     /* push r14 */
        0x41, 0x57                                      /* push r15 */
    };

    int attach_ok = 0;
    uint8_t* target = resolve_patch_target_by_rva_existing_rax_or_pattern(
        exe,
        OOTP27_PLAYER_CONTRACT_OFFER_ATTACH_RVA,
        expected,
        sizeof(expected),
        "KBO foreign AI offer attach probe patch");
    if (target == NULL) { return 0; }

    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO foreign AI offer attach probe patch already installed target=%p", target);
        attach_ok = 1;
    } else {

        uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
        if (trampoline == NULL) {
            kbo_log_runtime_line("failed to allocate KBO foreign AI offer attach probe trampoline");
            return 0;
        }

        uint8_t* stub = build_kbo_foreign_ai_offer_attach_probe_detour_stub(trampoline);
        if (stub == NULL) {
            kbo_log_runtime_line("failed to allocate KBO foreign AI offer attach probe detour stub");
            return 0;
        }

        uint8_t patch[16] = {
            0x48, 0xB8,                                     /* mov rax, stub */
            0,0,0,0,0,0,0,0,
            0xFF, 0xE0,                                     /* jmp rax */
            0x90, 0x90, 0x90, 0x90
        };
        write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

        DWORD old_protect = 0;
        if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
            kbo_log_runtimef("VirtualProtect failed for KBO foreign AI offer attach probe patch error=%lu", GetLastError());
            return 0;
        }

        memcpy(target, patch, sizeof(patch));
        FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

        DWORD ignored = 0;
        VirtualProtect(target, sizeof(patch), old_protect, &ignored);

        kbo_log_runtimef(
            "installed KBO foreign AI offer attach probe patch target=%p rva=0x%llx stub=%p trampoline=%p wrapper=%p",
            target,
            (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
            stub,
            trampoline,
            &ootp_kbo_foreign_ai_offer_attach_probe_wrapper);
        attach_ok = 1;
    }

    int build_ok = kbo_install_foreign_ai_offer_build_probe_patch(exe);
    int terms_ok = kbo_install_foreign_ai_offer_terms_build_probe_patch(exe);
    int gate_ok = kbo_install_foreign_ai_offer_final_gate_probe_patch(exe);
    return attach_ok && build_ok && terms_ok && gate_ok;
}
