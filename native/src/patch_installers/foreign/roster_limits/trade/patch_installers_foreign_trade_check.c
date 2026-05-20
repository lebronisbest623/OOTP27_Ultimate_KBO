#include "patch_installers_foreign_trade_check.h"
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
#include "../../../../hook_stubs/foreign_signability_stubs/trade/trade_check_stub.h"
#include "../../../../hook_stubs/military/hook_stubs_military.h"

int install_kbo_trade_check_foreign_policy_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO trade foreign policy patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO trade foreign policy patch host=%s", host);
        return 0;
    }

    const size_t stolen_len = 18;
    const uint8_t expected[18] = {
        0x48, 0x8B, 0xC4,                               /* mov rax,rsp */
        0x44, 0x88, 0x48, 0x20,                         /* mov [rax+0x20],r9b */
        0x4C, 0x89, 0x40, 0x18,                         /* mov [rax+0x18],r8 */
        0x89, 0x50, 0x10,                               /* mov [rax+0x10],edx */
        0x48, 0x89, 0x48, 0x08                          /* mov [rax+0x8],rcx */
    };
    const uint8_t context[48] = {
        0x00, 0x44, 0x0F, 0x28, 0x4C, 0x24, 0x60, 0x48,
        0x81, 0xC4, 0xC0, 0x00, 0x00, 0x00, 0x5E, 0xC3,
        0x48, 0x8B, 0xC4, 0x44, 0x88, 0x48, 0x20, 0x4C,
        0x89, 0x40, 0x18, 0x89, 0x50, 0x10, 0x48, 0x89,
        0x48, 0x08, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54,
        0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D
    };

    uint8_t* target = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_LEAGUE_TRADE_CHECK_RVA,
        expected,
        sizeof(expected),
        context,
        sizeof(context),
        16u,
        "KBO trade foreign policy patch");
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO trade foreign policy patch already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO trade foreign policy trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_trade_check_detour_stub(trampoline);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO trade foreign policy detour stub");
        return 0;
    }

    uint8_t patch[18] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO trade foreign policy patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO trade foreign policy patch target=%p rva=0x%llx stub=%p trampoline=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        trampoline,
        &ootp_kbo_trade_check_foreign_policy_probe);
    return 1;
}
