#include "patch_installers_intl_established_fa.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../common/patch_host.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../build_verify/build_verify.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../hook_stubs/foreign/intl_established_fa/hook_stubs_intl_established_fa.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"

int install_kbo_intl_established_fa_multiplier_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO international established FA multiplier patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO international established FA multiplier patch host=%s", host);
        return 0;
    }

    const uint8_t expected[14] = {
        0x45, 0x33, 0xC9,                               /* xor r9d,r9d */
        0x4C, 0x8D, 0x05, 0x76, 0x98, 0x41, 0x02,       /* lea r8,[Creating International Free Agents] */
        0x41, 0x8D, 0x51, 0x01                          /* lea edx,[r9+1] */
    };
    const uint8_t expected_mask[14] = {
        1,1,1,1,1,1,0,0,0,0,1,1,1,1
    };
    const uint8_t context[64] = {
        0x00, 0x00, 0x00, 0xE8, 0x40, 0x85, 0xBE, 0x00,
        0x83, 0xC0, 0xFD, 0x03, 0xF8, 0x89, 0x7D, 0x98,
        0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05, 0x76, 0x98,
        0x41, 0x02, 0x41, 0x8D, 0x51, 0x01, 0xE8, 0xD5,
        0x56, 0xB1, 0x00, 0x48, 0x8B, 0xD8, 0x45, 0x33,
        0xC9, 0x4C, 0x8D, 0x05, 0x38, 0x96, 0x3F, 0x02,
        0x41, 0x8D, 0x51, 0x01, 0xE8, 0xBF, 0x56, 0xB1,
        0x00, 0x44, 0x89, 0x6C, 0x24, 0x20, 0x45, 0x33
    };
    const uint8_t context_mask[64] = {
        1,1,1,1,0,0,0,0, 1,1,1,1,1,1,1,0,
        1,1,1,1,1,1,0,0, 0,0,1,1,1,1,1,0,
        0,0,0,1,1,1,1,1, 1,1,1,1,0,0,0,0,
        1,1,1,1,1,0,0,0, 0,1,1,1,1,1,1,1
    };

    uint8_t* target = resolve_patch_target_by_rva_or_masked_context_and_expected_pattern(
        exe,
        OOTP27_INTL_ESTABLISHED_FA_COUNT_READY_RVA,
        expected,
        expected_mask,
        sizeof(expected),
        context,
        context_mask,
        sizeof(context),
        16u,
        "KBO international established FA multiplier patch");
    if (target == NULL) {
        return 0;
    }
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO international established FA multiplier patch already installed target=%p", target);
        return 1;
    }
    if (!kbo_memory_matches_masked_pattern(target, expected, expected_mask, sizeof(expected))) {
        log_patch_bytes_mismatch("KBO international established FA multiplier patch", target, sizeof(expected));
        return 0;
    }

    void* continuation = target + sizeof(expected);
    void* progress_title = resolve_rip_relative_lea_target(target + 3);
    if (progress_title == NULL) {
        kbo_log_runtime_line("failed to resolve KBO international established FA progress title");
        return 0;
    }
    uint8_t* stub = build_kbo_intl_established_fa_count_stub(continuation, progress_title);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO international established FA multiplier stub");
        return 0;
    }

    uint8_t patch[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0,0,0,0,0,0,0,0
    };
    write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO international established FA multiplier patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO international established FA multiplier patch target=%p stub=%p continuation=%p wrapper=%p multiplier=%d",
        target,
        stub,
        continuation,
        &ootp_kbo_intl_established_fa_count_wrapper,
        kbo_get_intl_established_fa_multiplier());
    return 1;
}

int install_kbo_intl_established_fa_generation_filter_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO international established FA generation filter patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO international established FA generation filter patch host=%s", host);
        return 0;
    }

    const uint8_t expected[OOTP27_INTL_ESTABLISHED_FA_REGISTER_GATE_STOLEN_LEN] = {
        0x48, 0x8B, 0xD3,                               /* mov rdx,rbx */
        0x48, 0x8B, 0x0D, 0x3C, 0x4C, 0xB3, 0x02,       /* mov rcx,[global db] */
        0xE8, 0xB7, 0xC4, 0xDF, 0xFF                    /* call register player */
    };
    const uint8_t expected_mask[OOTP27_INTL_ESTABLISHED_FA_REGISTER_GATE_STOLEN_LEN] = {
        1,1,1,1,1,1,0,0,0,0,1,0,0,0,0
    };
    const uint8_t context[64] = {
        0x7C, 0x12, 0x0F, 0x9E, 0xC0, 0x66, 0x83, 0xC0,
        0x04, 0x66, 0x89, 0x83, 0x56, 0x08, 0x00, 0x00,
        0x48, 0x8B, 0xD3, 0x48, 0x8B, 0x0D, 0x3C, 0x4C,
        0xB3, 0x02, 0xE8, 0xB7, 0xC4, 0xDF, 0xFF, 0x8B,
        0x45, 0xCC, 0x8B, 0x55, 0xC8, 0x3B, 0xC2, 0x0F,
        0x8C, 0xBB, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x4D,
        0xC0, 0x48, 0x85, 0xC9, 0x74, 0x60, 0x0F, 0xBF,
        0x45, 0xD0, 0x03, 0xC2, 0x48, 0x63, 0xD0, 0x48
    };
    const uint8_t context_mask[64] = {
        1,0,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,0,0, 0,0,1,0,0,0,0,1,
        1,1,1,1,1,1,1,1, 1,0,0,0,0,1,1,1,
        1,1,1,1,1,0,1,1, 1,1,1,1,1,1,1,1
    };

    uint8_t* target = resolve_patch_target_by_rva_or_masked_context_and_expected_pattern(
        exe,
        OOTP27_INTL_ESTABLISHED_FA_REGISTER_GATE_RVA,
        expected,
        expected_mask,
        sizeof(expected),
        context,
        context_mask,
        sizeof(context),
        16u,
        "KBO international established FA generation filter patch");
    if (target == NULL) {
        return 0;
    }
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO international established FA generation filter patch already installed target=%p", target);
        return 1;
    }
    if (!kbo_memory_matches_masked_pattern(target, expected, expected_mask, sizeof(expected))) {
        log_patch_bytes_mismatch("KBO international established FA generation filter patch", target, sizeof(expected));
        return 0;
    }

    void* global_database_slot = resolve_rip_relative_lea_target(target + 3);
    void* register_player_func = resolve_relative_call_target(target + OOTP27_INTL_ESTABLISHED_FA_REGISTER_CALL_OFFSET);
    if (global_database_slot == NULL || register_player_func == NULL) {
        kbo_log_runtimef(
            "failed to resolve KBO international established FA register dependencies global=%p call=%p",
            global_database_slot,
            target + OOTP27_INTL_ESTABLISHED_FA_REGISTER_CALL_OFFSET);
        return 0;
    }

    void* continuation = target + sizeof(expected);
    void* retry_continuation = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_INTL_ESTABLISHED_FA_RETRY_LOOP_RVA);
    if (retry_continuation == NULL) {
        kbo_log_runtimef(
            "failed to resolve KBO international established FA retry loop RVA (canonical=0x%08X)",
            OOTP27_INTL_ESTABLISHED_FA_RETRY_LOOP_RVA);
        return 0;
    }
    uint8_t* stub = build_kbo_intl_established_fa_register_gate_stub(
        continuation,
        retry_continuation,
        global_database_slot,
        register_player_func);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO international established FA generation filter stub");
        return 0;
    }

    uint8_t patch[OOTP27_INTL_ESTABLISHED_FA_REGISTER_GATE_STOLEN_LEN] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0,0,0,0,0,0,0,0,
        0x90
    };
    write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO international established FA generation filter patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO international established FA generation filter patch target=%p stub=%p continuation=%p retry=%p global_slot=%p register=%p wrapper=%p action=retry_without_registering",
        target,
        stub,
        continuation,
        retry_continuation,
        global_database_slot,
        register_player_func,
        &ootp_kbo_intl_established_fa_generation_filter_allows_wrapper);
    return 1;
}
