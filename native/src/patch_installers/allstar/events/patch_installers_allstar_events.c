#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../build_verify/build_verify.h"
#include "../../../core/logging/core_log.h"
#include "../../../hook_stubs/allstar/candidate/hook_stubs_allstar_candidate.h"
#include "../../../hook_stubs/allstar/events/hook_stubs_allstar_events.h"
#include "../../../hook_stubs/allstar/settings/hook_stubs_allstar_settings.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../common/patch_installers_allstar_common.h"
#include "patch_installers_allstar_events.h"

static void* resolve_allstar_team_setup_address(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe != NULL) {
        uint8_t* setup = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
            exe,
            OOTP27_ALLSTAR_TEAM_SETUP_FUNC_RVA);
        if (memory_range_readable(setup, 16u)) {
            return setup;
        }
    }

    static const uint8_t original_context[] = {
        0xF6, 0x41, 0x4C, 0x01,
        0x0F, 0x85, 0x74, 0x02, 0x00, 0x00
    };
    static const uint8_t patched_context[] = {
        0xF6, 0x41, 0x4C, 0x01,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };

    uint8_t* context = find_ootp_executable_pattern(original_context, sizeof(original_context));
    if (context == NULL) {
        context = find_ootp_executable_pattern(patched_context, sizeof(patched_context));
    }
    if (context != NULL && (uintptr_t)context > (uintptr_t)OOTP27_ALLSTAR_TEAM_SETUP_BAIL_CONTEXT_FROM_FUNC_DELTA) {
        uint8_t* setup = context - OOTP27_ALLSTAR_TEAM_SETUP_BAIL_CONTEXT_FROM_FUNC_DELTA;
        if (memory_range_readable(setup, 16u)) {
            return setup;
        }
    }

    return NULL;
}

int install_allstar_voting_begin_prepare_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star voting begin prepare patch");
    if (exe == NULL) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    const uint8_t may_expected[16] = {
        0x8D, 0x53, 0xE4,
        0x80, 0xBE, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0xF3, 0x01, 0x00, 0x00
    };
    const uint8_t april_expected[16] = {
        0x8D, 0x53, 0xE4,
        0x80, 0xBE, 0xE8, 0x45, 0x00, 0x00, 0x00,
        0x0F, 0x84, 0xF3, 0x01, 0x00, 0x00
    };

    const uint8_t* expected = april_expected;
    uint8_t* target = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
    if (target != NULL) {
        expected = may_expected;
    } else {
        target = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
        expected = april_expected;
    }

    if (!memory_range_readable(target, sizeof(april_expected))) {
        kbo_log_runtimef("KBO all-star voting begin prepare hook target unreadable target=%p", target);
        return 0;
    }

    int ok = 0;
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO all-star voting begin prepare hook already installed target=%p", target);
        ok = 1;
    } else if (memcmp(target, expected, sizeof(april_expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star voting begin prepare hook", target, sizeof(april_expected));
        log_extended_context("KBO all-star voting begin prepare hook", target, 16, 96);
    } else {
        void* return_address = target + sizeof(april_expected);
        void* no_game_address = kbo_resolve_build_specific_rva_related_ptr(
            exe,
            target,
            OOTP27_ALLSTAR_VOTING_BEGIN_PREP_SITE_RVA,
            OOTP27_ALLSTAR_VOTING_BEGIN_NO_GAME_RVA);
        if (no_game_address == NULL) {
            kbo_log_runtime_line("KBO all-star voting begin prepare hook skipped: no-game target unresolved");
            return 0;
        }
        void* allstar_team_setup_address = resolve_allstar_team_setup_address();
        uint8_t* stub = build_allstar_voting_begin_prepare_stub(return_address, no_game_address, allstar_team_setup_address, layout.game_flag_offset);
        if (stub == NULL) {
            kbo_log_runtime_line("failed to allocate KBO all-star voting begin prepare hook stub");
        } else {
            uint8_t patch[16] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                kbo_log_runtimef("VirtualProtect failed for KBO all-star voting begin prepare hook error=%lu", GetLastError());
            } else {
                memcpy(target, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(target, sizeof(patch), old_protect, &ignored);
                kbo_log_runtimef(
                    "installed KBO all-star voting begin prepare hook target=%p stub=%p return=%p no_game=%p setup=%p helper=%p",
                    target,
                    stub,
                    return_address,
                    no_game_address,
                    allstar_team_setup_address,
                    &ootp_kbo_prepare_allstar_voting_begin);
                ok = 1;
            }
        }
    }

    return ok;
}

int install_allstar_events_prepare_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star events prepare patch");
    if (exe == NULL) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    const uint8_t may_expected[19] = {
        0x41, 0x80, 0xBC, 0x24, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x08,
        0x49, 0x8B, 0xCC,
        0xE8, 0xB9, 0x55, 0xFF, 0xFF
    };
    const uint8_t april_expected[19] = {
        0x41, 0x80, 0xBC, 0x24, 0xE8, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x08,
        0x49, 0x8B, 0xCC,
        0xE8, 0x89, 0x55, 0xFF, 0xFF
    };

    const uint8_t* expected = april_expected;
    uint8_t* target = find_ootp_executable_pattern(may_expected, sizeof(may_expected));
    if (target != NULL) {
        expected = may_expected;
    } else {
        target = find_ootp_executable_pattern(april_expected, sizeof(april_expected));
        expected = april_expected;
    }

    if (!memory_range_readable(target, sizeof(april_expected))) {
        kbo_log_runtimef("KBO all-star events prepare hook target unreadable target=%p", target);
        return 0;
    }

    int ok = 0;
    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO all-star events prepare hook already installed target=%p", target);
        ok = 1;
    } else if (memcmp(target, expected, sizeof(april_expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star events prepare hook", target, sizeof(april_expected));
        log_extended_context("KBO all-star events prepare hook", target, 16, 96);
    } else {
        void* return_address = target + sizeof(april_expected);
        void* allstar_prep_address = resolve_relative_call_target(
            target + OOTP27_MAKE_ALLSTAR_EVENTS_PREP_CALL_OFFSET);
        if (allstar_prep_address == NULL) {
            kbo_log_runtime_line("KBO all-star events prepare hook skipped: prep call target unresolved");
            return 0;
        }
        void* make_events_address = kbo_resolve_build_specific_rva_ptr(
            exe,
            OOTP27_MAKE_ALLSTAR_GAME_EVENTS_RVA);
        if (!memory_range_readable(make_events_address, 16u)) {
            kbo_log_runtimef("KBO all-star events prepare hook skipped: make-events target unreadable target=%p", make_events_address);
            return 0;
        }
        InterlockedExchangePointer(
            (PVOID volatile*)&g_allstar_make_events_ptr,
            make_events_address);

        uint8_t* stub = build_allstar_events_prepare_stub(return_address, allstar_prep_address, layout.game_flag_offset);
        if (stub == NULL) {
            kbo_log_runtime_line("failed to allocate KBO all-star events prepare hook stub");
        } else {
            uint8_t patch[19] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90, 0x90, 0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                kbo_log_runtimef("VirtualProtect failed for KBO all-star events prepare hook error=%lu", GetLastError());
            } else {
                memcpy(target, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(target, sizeof(patch), old_protect, &ignored);
                kbo_log_runtimef(
                    "installed KBO all-star events prepare hook target=%p stub=%p return=%p make_events=%p prep=%p helper=%p",
                    target,
                    stub,
                    return_address,
                    make_events_address,
                    allstar_prep_address,
                    &ootp_kbo_prepare_allstar_events);
                ok = 1;
            }
        }
    }

    return ok;
}

