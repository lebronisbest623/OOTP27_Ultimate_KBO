#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "../common/patch_host.h"

#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../build_verify/build_verify.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../hook_stubs/military/hook_stubs_military.h"
#include "../../patch_helpers/patch_helpers.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/add_player_guard/team_add_player_guard_ai_roster.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

typedef void (*KboRosterMoveTraceSetTrampolineFn)(void*);

static int install_kbo_foreign_roster_move_trace_patch(
    HMODULE exe,
    const char* label,
    uint32_t target_rva,
    const uint8_t* expected,
    size_t patch_len,
    void* wrapper,
    KboRosterMoveTraceSetTrampolineFn set_trampoline)
{
    uint8_t* rva_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, target_rva);
    if (memory_range_readable(rva_target, patch_len) && is_rax_absolute_jump_patch(rva_target)) {
        kbo_log_runtimef("%s already installed target=%p", label, rva_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        target_rva,
        expected,
        patch_len,
        label);
    if (target == NULL) {
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("%s already installed target=%p", label, target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, patch_len);
    if (trampoline == NULL) {
        kbo_log_runtimef("failed to allocate %s trampoline", label);
        return 0;
    }
    set_trampoline(trampoline);

    uint8_t patch[32] = {0};
    if (patch_len > sizeof(patch) || patch_len < 12) {
        kbo_log_runtimef("%s invalid patch_len=%llu", label, (unsigned long long)patch_len);
        return 0;
    }
    memset(patch, 0x90, patch_len);
    patch[0] = 0x48;
    patch[1] = 0xB8;
    write_u64(&patch[2], (uint64_t)(uintptr_t)wrapper);
    patch[10] = 0xFF;
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
        "installed %s target=%p rva=0x%llx stub=%p trampoline=%p wrapper=%p",
        label,
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        wrapper,
        trampoline,
        wrapper);
    return 1;
}

int install_kbo_ai_roster_select_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_AI_ROSTER_SELECT_TRACE_FILE)) {
        kbo_log_runtime_line("KBO AI roster select trace skipped: disable_ai_roster_select_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO AI roster select trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO AI roster select trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[12] = {
        0x48, 0x8B, 0xC4,
        0x44, 0x89, 0x40, 0x18,
        0x89, 0x50, 0x10,
        0x55,
        0x53
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster select trace DD86C0",
        OOTP27_AI_ROSTER_SELECT_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_select_trace_wrapper,
        &kbo_set_ai_roster_select_trace_trampoline);
}

int install_kbo_ai_roster_primary_apply_flow_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_AI_ROSTER_PRIMARY_APPLY_FLOW_TRACE_FILE)) {
        kbo_log_runtime_line("KBO AI roster primary apply-flow trace skipped: disable_ai_roster_primary_apply_flow_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO AI roster primary apply-flow trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO AI roster primary apply-flow trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[14] = {
        0x40, 0x53,
        0x48, 0x83, 0xEC, 0x30,
        0x48, 0x8B, 0xD9,
        0x48, 0x89, 0x6C, 0x24, 0x48
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster primary apply-flow trace DD1B20",
        OOTP27_AI_ROSTER_PRIMARY_APPLY_FLOW_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_primary_apply_flow_trace_wrapper,
        &kbo_set_ai_roster_primary_apply_flow_trace_trampoline);
}

int install_kbo_ai_roster_apply_selection_trace_patch(void)
{
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_AI_ROSTER_APPLY_SELECTION_TRACE_FILE)) {
        kbo_log_runtime_line("KBO AI roster apply-selection trace skipped: disable_ai_roster_apply_selection_trace is true");
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO AI roster apply-selection trace patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO AI roster apply-selection trace patch host=%s", host);
        return 0;
    }

    const uint8_t expected[17] = {
        0x44, 0x89, 0x4C, 0x24, 0x20,
        0x89, 0x54, 0x24, 0x10,
        0x53,
        0x56,
        0x41, 0x55,
        0x48, 0x83, 0xEC, 0x40
    };

    return install_kbo_foreign_roster_move_trace_patch(
        exe,
        "KBO AI roster apply-selection trace DE6730",
        OOTP27_AI_ROSTER_APPLY_SELECTION_TRACE_RVA,
        expected,
        sizeof(expected),
        &ootp_kbo_ai_roster_apply_selection_trace_wrapper,
        &kbo_set_ai_roster_apply_selection_trace_trampoline);
}
