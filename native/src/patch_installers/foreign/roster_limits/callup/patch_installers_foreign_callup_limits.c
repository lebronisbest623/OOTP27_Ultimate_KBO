#include "patch_installers_foreign_callup_limits.h"
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
#include "../../../../hook_stubs/foreign/counts/hook_stubs_foreign_counts.h"

int install_kbo_callup_foreign_limit_branch_patch(
    const char* label,
    uint32_t target_rva,
    int32_t allow_delta,
    int32_t fallback_delta,
    const uint8_t* expected,
    size_t patch_len,
    void* wrapper,
    int total_check)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtimef("GetModuleHandleA(NULL) failed for %s", label);
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping %s host=%s", label, host);
        return 0;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(exe, target_rva, expected, patch_len, label);
    if (target == NULL) {
        return 0;
    }
    if (!memory_range_readable(target, patch_len)) {
        kbo_log_runtimef("%s target unreadable target=%p", label, target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("%s already installed target=%p", label, target);
        return 1;
    }
    if (memcmp(target, expected, patch_len) != 0) {
        log_patch_bytes_mismatch(label, target, patch_len);
        return 0;
    }

    uint8_t* allow_target = target + allow_delta;
    uint8_t* fallback_target = target + fallback_delta;
    if (!memory_range_readable(allow_target, 1u) || !memory_range_readable(fallback_target, 1u)) {
        kbo_log_runtimef(
            "%s branch destination unreadable allow=%p fallback=%p",
            label,
            allow_target,
            fallback_target);
        return 0;
    }

    uint8_t* stub = build_kbo_callup_foreign_limit_branch_stub(
        allow_target,
        target + patch_len,
        fallback_target,
        wrapper,
        total_check);
    if (stub == NULL) {
        kbo_log_runtimef("failed to allocate %s stub", label);
        return 0;
    }

    uint8_t patch[32] = {
        0x48, 0xB8,
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,
        0x90
    };
    if (patch_len < 13 || patch_len > sizeof(patch)) {
        kbo_log_runtimef("%s invalid patch_len=%llu", label, (unsigned long long)patch_len);
        return 0;
    }
    for (size_t i = 13; i < patch_len; i++) {
        patch[i] = 0x90;
    }
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, patch_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }
    memcpy(target, patch, patch_len);
    FlushInstructionCache(GetCurrentProcess(), target, patch_len);
    DWORD ignored = 0;
    VirtualProtect(target, patch_len, old_protect, &ignored);

    kbo_log_runtimef("installed %s target=%p stub=%p patch_len=%llu", label, target, stub, (unsigned long long)patch_len);
    return 1;
}

int install_kbo_callup_foreign_limit_branch_patches(void)
{
    const uint8_t hitter_expected[17] = {
        0x3B, 0xC2, 0x0F, 0x8C, 0x8B, 0x00, 0x00, 0x00,
        0x45, 0x84, 0xE4, 0x0F, 0x85, 0x1E, 0x18, 0x00,
        0x00
    };
    const uint8_t pitcher_expected[13] = {
        0x3B, 0xC2, 0x7C, 0x52, 0x45, 0x84, 0xE4,
        0x0F, 0x85, 0x35, 0x17, 0x00, 0x00
    };
    const uint8_t total_expected[13] = {
        0x3B, 0xD1, 0x7C, 0x52, 0x45, 0x84, 0xE4,
        0x0F, 0x85, 0x85, 0x16, 0x00, 0x00
    };

    int ok = 1;
    ok &= install_kbo_callup_foreign_limit_branch_patch(
        "KBO callup foreign hitter limit branch patch",
        OOTP27_CALLUP_FOREIGN_HITTER_LIMIT_BRANCH_RVA,
        0x93,
        0x182F,
        hitter_expected,
        sizeof(hitter_expected),
        &ootp_kbo_callup_foreign_hitter_limit_allows_wrapper,
        0);
    ok &= install_kbo_callup_foreign_limit_branch_patch(
        "KBO callup foreign pitcher limit branch patch",
        OOTP27_CALLUP_FOREIGN_PITCHER_LIMIT_BRANCH_RVA,
        0x56,
        0x1742,
        pitcher_expected,
        sizeof(pitcher_expected),
        &ootp_kbo_callup_foreign_pitcher_limit_allows_wrapper,
        0);
    ok &= install_kbo_callup_foreign_limit_branch_patch(
        "KBO callup foreign total limit branch patch",
        OOTP27_CALLUP_FOREIGN_TOTAL_LIMIT_BRANCH_RVA,
        0x56,
        0x1692,
        total_expected,
        sizeof(total_expected),
        &ootp_kbo_callup_foreign_total_limit_allows_wrapper,
        1);
    return ok;
}
