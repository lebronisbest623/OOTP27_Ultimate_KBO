#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "../common/patch_host.h"

#include "patch_installers_cbt_draft_order.h"
#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../build_verify/build_verify.h"
#include "../../core/logging/core_log.h"
#include "../../hook_stubs/competitive_balance_tax/hook_stubs_cbt_draft_order.h"
#include "../../patch_helpers/patch_helpers.h"
#include "../../runtime_memory/runtime_memory.h"

int install_kbo_cbt_draft_order_penalty_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO CBT draft order penalty patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO CBT draft order penalty patch host=%s", host);
        return 0;
    }

    const uint8_t expected[OOTP27_DRAFT_ORDER_CREATE_STOLEN_LEN] = {
        0x88, 0x54, 0x24, 0x10,       /* mov [rsp+0x10], dl */
        0x48, 0x89, 0x4C, 0x24, 0x08, /* mov [rsp+0x8], rcx */
        0x55,                         /* push rbp */
        0x53,                         /* push rbx */
        0x56                          /* push rsi */
    };
    const uint8_t context[] = {
        0x88, 0x54, 0x24, 0x10,
        0x48, 0x89, 0x4C, 0x24, 0x08,
        0x55,
        0x53,
        0x56,
        0x57,
        0x41, 0x54,
        0x41, 0x55,
        0x41, 0x56,
        0x41, 0x57,
        0x48, 0x8D, 0x6C, 0x24, 0xE1,
        0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00,
        0x4C, 0x8B, 0xE1,
        0x4C, 0x8B, 0x0D, 0, 0, 0, 0,
        0x4D, 0x85, 0xC9,
        0x0F, 0x84, 0, 0, 0, 0,
        0x4D, 0x8B, 0x81, 0xD8, 0x02, 0x00, 0x00,
        0x4D, 0x85, 0xC0,
        0x0F, 0x84, 0, 0, 0, 0
    };
    const uint8_t context_mask[] = {
        1, 1, 1, 1,
        1, 1, 1, 1, 1,
        1,
        1,
        1,
        1,
        1, 1,
        1, 1,
        1, 1,
        1, 1,
        1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1,
        1, 1, 1,
        1, 1, 1, 0, 0, 0, 0,
        1, 1, 1,
        1, 1, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1,
        1, 1, 1,
        1, 1, 0, 0, 0, 0
    };

    uint8_t* rva_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_DRAFT_ORDER_CREATE_RVA);
    if (memory_range_readable(rva_target, sizeof(expected)) && is_rax_absolute_jump_patch(rva_target)) {
        kbo_log_runtimef("KBO CBT draft order penalty patch already installed target=%p", rva_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_DRAFT_ORDER_CREATE_RVA,
        expected,
        sizeof(expected),
        context,
        context_mask,
        sizeof(context),
        0u,
        "KBO CBT draft order penalty patch");
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO CBT draft order penalty patch already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_cbt_draft_order_trampoline(
        target,
        OOTP27_DRAFT_ORDER_CREATE_STOLEN_LEN);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO CBT draft order penalty trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_cbt_draft_order_detour_stub(trampoline);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO CBT draft order penalty detour stub");
        return 0;
    }

    uint8_t patch[OOTP27_DRAFT_ORDER_CREATE_STOLEN_LEN] = {
        0x48, 0xB8,
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO CBT draft order penalty patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO CBT draft order penalty patch target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_cbt_draft_order_create_wrapper);
    return 1;
}
