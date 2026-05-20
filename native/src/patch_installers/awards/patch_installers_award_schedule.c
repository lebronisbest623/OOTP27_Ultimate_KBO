#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "../common/patch_host.h"

#include "../../awards/schedule/award_schedule_probe_module.h"
#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../build_verify/build_verify.h"
#include "../../core/logging/core_log.h"
#include "../../hook_stubs/military/hook_stubs_military.h"
#include "../../patch_helpers/patch_helpers.h"
#include "../../runtime_memory/runtime_memory.h"
#include "patch_installers_award_schedule.h"

int install_kbo_award_schedule_create_event_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO award schedule create-event patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO award schedule create-event patch host=%s", host);
        return 0;
    }

    const size_t stolen_len = 18u;
    const uint8_t expected[18] = {
        0x48, 0x89, 0x5C, 0x24, 0x10,
        0x48, 0x89, 0x6C, 0x24, 0x18,
        0x56,
        0x57,
        0x41, 0x54,
        0x41, 0x56,
        0x41, 0x57
    };

    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_CREATE_LEAGUE_EVENT_RVA);
    if (memory_range_readable(target, 12u) && is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO award schedule create-event patch already installed target=%p", target);
        return 1;
    }
    if (!memory_range_readable(target, stolen_len)) {
        kbo_log_runtimef("KBO award schedule create-event patch skipped reason=target_unreadable target=%p", target);
        return 0;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO award schedule create-event patch", target, sizeof(expected));
        return 0;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO award schedule create-event trampoline");
        return 0;
    }
    kbo_award_schedule_set_create_league_event_original((OotpCreateLeagueEventFn)trampoline);

    uint8_t patch[18] = {
        0x48, 0xB8,
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)&ootp_kbo_award_schedule_create_league_event_wrapper);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO award schedule create-event patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO award schedule create-event patch target=%p rva=0x%llx trampoline=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        trampoline,
        &ootp_kbo_award_schedule_create_league_event_wrapper);
    return 1;
}
