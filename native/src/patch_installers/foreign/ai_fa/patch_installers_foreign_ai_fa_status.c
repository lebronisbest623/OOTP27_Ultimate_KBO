#include "patch_installers_foreign_ai_fa_status_internal.h"
#include <stdio.h>
#include <string.h>
#include "../../common/patch_host.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../build_verify/build_verify.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../hook_stubs/foreign/ai_status/hook_stubs_foreign_ai_status.h"
#include "../../../hook_stubs/military/hook_stubs_military.h"

typedef uint8_t* (*KboAiFaStatusCandidateInsertStubBuilder)(void* continuation);

static uint8_t* kbo_ai_fa_status_resolve_insert_site(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    size_t expected_size,
    const uint8_t* context,
    size_t context_size,
    size_t context_offset,
    const char* label)
{
    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, rva);
    if (memory_range_readable(target, 12) && is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("%s already installed target=%p", label, target);
        return target;
    }
    if (memory_range_readable(target, expected_size) && memcmp(target, expected, expected_size) == 0) {
        kbo_log_runtimef("%s resolved by rva target=%p rva=0x%08X", label, target, rva);
        return target;
    }
    if (context != NULL && context_size != 0) {
        return resolve_patch_target_by_rva_or_context_pattern(
            exe,
            rva,
            expected,
            expected_size,
            context,
            context_size,
            context_offset,
            label);
    }
    return resolve_patch_target_by_rva_or_pattern(exe, rva, expected, expected_size, label);
}

static int kbo_install_ai_fa_status_candidate_insert_site(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    size_t expected_size,
    const uint8_t* context,
    size_t context_size,
    size_t context_offset,
    const char* label,
    KboAiFaStatusCandidateInsertStubBuilder build_stub)
{
    uint8_t* target = kbo_ai_fa_status_resolve_insert_site(
        exe,
        rva,
        expected,
        expected_size,
        context,
        context_size,
        context_offset,
        label);
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        return 1;
    }

    uint8_t* stub = build_stub(target + expected_size);
    if (stub == NULL) {
        kbo_log_runtimef("failed to allocate %s stub", label);
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
    if (!VirtualProtect(target, expected_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, expected_size);
    FlushInstructionCache(GetCurrentProcess(), target, expected_size);

    DWORD ignored = 0;
    VirtualProtect(target, expected_size, old_protect, &ignored);

    kbo_log_runtimef(
        "installed %s target=%p rva=0x%llx stub=%p wrapper=%p",
        label,
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        &ootp_kbo_ai_fa_status_candidate_insert_wrapper);
    return 1;
}

int install_kbo_ai_fa_status_candidate_insert_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO AI FA status candidate insert patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO AI FA status candidate insert patch host=%s", host);
        return 0;
    }

    const uint8_t primary_expected[16] = {
        0x48, 0x63, 0xC2,                               /* movsxd rax,edx */
        0x48, 0x8B, 0x4D, 0xC8,                         /* mov rcx,[rbp-0x38] */
        0x48, 0x89, 0x1C, 0xC1,                         /* mov [rcx+rax*8],rbx */
        0xFF, 0xC2,                                     /* inc edx */
        0x89, 0x55, 0xD4                                /* mov [rbp-0x2c],edx */
    };
    const uint8_t expected[12] = {
        0x48, 0x63, 0xC1,                               /* movsxd rax,ecx */
        0x4D, 0x89, 0x3C, 0xC6,                         /* mov [r14+rax*8],r15 */
        0xFF, 0xC1,                                     /* inc ecx */
        0x89, 0x4D, 0xD4                                /* mov [rbp-0x2c],ecx */
    };
    const uint8_t context[48] = {
        0x8B, 0x9D, 0x58, 0x01, 0x00, 0x00, 0x4C, 0x8B,
        0xA5, 0x50, 0x01, 0x00, 0x00, 0x8B, 0x4D, 0xD4,
        0x48, 0x63, 0xC1, 0x4D, 0x89, 0x3C, 0xC6, 0xFF,
        0xC1, 0x89, 0x4D, 0xD4, 0xEB, 0x19, 0x4C, 0x8B,
        0x75, 0xC8, 0x48, 0x63, 0xC1, 0x4D, 0x89, 0x3C,
        0xC6, 0xFF, 0xC1, 0x89, 0x4D, 0xD4, 0xEB, 0x07
    };
    const uint8_t direct_expected[16] = {
        0x4C, 0x8B, 0x75, 0xC8,                         /* mov r14,[rbp-0x38] */
        0x48, 0x63, 0xC1,                               /* movsxd rax,ecx */
        0x4D, 0x89, 0x3C, 0xC6,                         /* mov [r14+rax*8],r15 */
        0xFF, 0xC1,                                     /* inc ecx */
        0x89, 0x4D, 0xD4                                /* mov [rbp-0x2c],ecx */
    };

    int primary_ok = kbo_install_ai_fa_status_candidate_insert_site(
        exe,
        OOTP27_AI_FA_STATUS_CANDIDATE_INSERT_PRIMARY_RVA,
        primary_expected,
        sizeof(primary_expected),
        NULL,
        0,
        0,
        "KBO AI FA status candidate primary insert patch",
        build_kbo_ai_fa_status_candidate_insert_primary_stub);
    int expansion_ok = kbo_install_ai_fa_status_candidate_insert_site(
        exe,
        OOTP27_AI_FA_STATUS_CANDIDATE_INSERT_RVA,
        expected,
        sizeof(expected),
        context,
        sizeof(context),
        16u,
        "KBO AI FA status candidate expansion insert patch",
        build_kbo_ai_fa_status_candidate_insert_stub);
    int direct_ok = kbo_install_ai_fa_status_candidate_insert_site(
        exe,
        OOTP27_AI_FA_STATUS_CANDIDATE_INSERT_DIRECT_RVA,
        direct_expected,
        sizeof(direct_expected),
        NULL,
        0,
        0,
        "KBO AI FA status candidate direct insert patch",
        build_kbo_ai_fa_status_candidate_insert_direct_stub);
    return primary_ok && expansion_ok && direct_ok;
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
