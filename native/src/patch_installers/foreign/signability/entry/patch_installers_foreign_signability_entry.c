#include "patch_installers_foreign_signability_entry.h"
#include <stdio.h>
#include <string.h>
#include "../../../common/patch_host.h"
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/dates/core_current_date.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../patch_helpers/patch_helpers.h"
#include "../../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../../hook_stubs/foreign_signability_stubs/eligibility/basic_eligibility_stubs.h"
#include "../../../../hook_stubs/military/hook_stubs_military.h"

int install_kbo_player_team_signability_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO player/team signability patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO player/team signability patch host=%s", host);
        return 0;
    }

    const size_t stolen_len = 16;
    const uint8_t expected[16] = {
        0x66, 0x44, 0x89, 0x44, 0x24, 0x18,             // mov [rsp + 0x18], r8w
        0x53,                                           // push rbx
        0x56,                                           // push rsi
        0x41, 0x54,                                     // push r12
        0x41, 0x55,                                     // push r13
        0x48, 0x83, 0xEC, 0x28                          // sub rsp, 0x28
    };

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_PLAYER_TEAM_SIGNABILITY_RVA,
        expected,
        sizeof(expected),
        "KBO player/team signability patch");
    if (target == NULL) { return 0; }

    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO player/team signability patch already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player/team signability trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_player_team_signability_detour_stub(trampoline);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player/team signability detour stub");
        return 0;
    }

    uint8_t patch[16] = {
        0x48, 0xB8,                                     // mov rax, stub
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO player/team signability patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO player/team signability patch target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_player_team_signability_wrapper);
    return 1;
}

int install_kbo_player_offer_eligibility_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO player offer eligibility patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO player offer eligibility patch host=%s", host);
        return 0;
    }

    const size_t stolen_len = 20;
    const uint8_t expected[20] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,                   // mov [rsp + 0x08], rbx
        0x48, 0x89, 0x6C, 0x24, 0x10,                   // mov [rsp + 0x10], rbp
        0x48, 0x89, 0x74, 0x24, 0x18,                   // mov [rsp + 0x18], rsi
        0x48, 0x89, 0x7C, 0x24, 0x20                    // mov [rsp + 0x20], rdi
    };
    const uint8_t context[48] = {
        0x20, 0x5F, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48,
        0x89, 0x7C, 0x24, 0x20, 0x41, 0x54, 0x41, 0x56,
        0x41, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x8D, 0x42
    };

    uint8_t* target = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_PLAYER_SIGNABILITY_BLOCK_849320_RVA,
        expected,
        sizeof(expected),
        context,
        sizeof(context),
        16u,
        "KBO player offer eligibility patch");
    if (target == NULL) { return 0; }

    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO player offer eligibility patch already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player offer eligibility trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_player_offer_eligibility_detour_stub(trampoline);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO player offer eligibility detour stub");
        return 0;
    }

    uint8_t patch[20] = {
        0x48, 0xB8,                                     // mov rax, stub
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO player offer eligibility patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO player offer eligibility patch target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_player_offer_eligibility_wrapper);
    return 1;
}
