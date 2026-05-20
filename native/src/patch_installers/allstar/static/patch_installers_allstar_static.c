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
#include "patch_installers_allstar_static.h"

static int install_allstar_prep_single_division_gate_patch(HMODULE exe)
{
    const uint8_t expected[10] = {
        0xF6, 0x42, 0x4C, 0x01,
        0x0F, 0x84, 0xC0, 0x00, 0x00, 0x00
    };

    uint8_t* target = find_ootp_executable_pattern(expected, sizeof(expected));
    if (target == NULL) {
        target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
            exe,
            OOTP27_ALLSTAR_PREP_SINGLE_DIVISION_GATE_RVA);
    }

    if (target == NULL || !memory_range_readable(target, sizeof(expected))) {
        kbo_log_runtimef("KBO all-star prep single-division gate hook target unreadable target=%p", target);
        return 0;
    }

    if (target[0] == 0xE9) {
        kbo_log_runtimef("KBO all-star prep single-division gate hook already installed target=%p", target);
        return 1;
    }

    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star prep single-division gate hook", target, sizeof(expected));
        log_extended_context("KBO all-star prep single-division gate hook", target, 16, 96);
        return 0;
    }

    int32_t rel = *(int32_t*)(target + 6);
    void* skip_address = target + sizeof(expected) + rel;
    void* return_address = target + sizeof(expected);
    uint8_t* stub = build_allstar_prep_single_division_gate_stub(target, return_address, skip_address);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO all-star prep single-division gate hook stub");
        return 0;
    }

    intptr_t jmp_rel = (intptr_t)stub - ((intptr_t)target + 5);
    if (jmp_rel < -2147483647 - 1 || jmp_rel > 2147483647) {
        kbo_log_runtimef(
            "KBO all-star prep single-division gate hook skipped reason=stub_out_of_range target=%p stub=%p",
            target,
            stub);
        return 0;
    }

    uint8_t patch[10] = {0xE9, 0,0,0,0, 0x90,0x90,0x90,0x90,0x90};
    write_u32(&patch[1], (uint32_t)(int32_t)jmp_rel);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO all-star prep single-division gate hook error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);
    kbo_log_runtimef(
        "installed KBO all-star prep single-division gate hook target=%p stub=%p return=%p skip=%p helper=%p",
        target,
        stub,
        return_address,
        skip_address,
        &ootp_kbo_allow_single_division_allstar_prep);
    return 1;
}

static int install_allstar_roster_single_division_gate_patch(HMODULE exe)
{
    const uint8_t expected[8] = {
        0x41, 0x8B, 0x46, 0x4C,
        0xA8, 0x01,
        0x74, 0x0D
    };

    uint8_t* target = find_ootp_executable_pattern(expected, sizeof(expected));
    if (target == NULL) {
        target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
            exe,
            OOTP27_ALLSTAR_ROSTER_SINGLE_DIVISION_GATE_RVA);
    }

    if (target == NULL || !memory_range_readable(target, sizeof(expected))) {
        kbo_log_runtimef("KBO all-star roster single-division gate hook target unreadable target=%p", target);
        return 0;
    }

    if (target[0] == 0xE9) {
        kbo_log_runtimef("KBO all-star roster single-division gate hook already installed target=%p", target);
        return 1;
    }

    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star roster single-division gate hook", target, sizeof(expected));
        log_extended_context("KBO all-star roster single-division gate hook", target, 16, 96);
        return 0;
    }

    void* skip_address = target + sizeof(expected) + (int8_t)target[7];
    void* return_address = target + sizeof(expected);
    uint8_t* stub = build_allstar_roster_single_division_gate_stub(target, return_address, skip_address);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO all-star roster single-division gate hook stub");
        return 0;
    }

    intptr_t jmp_rel = (intptr_t)stub - ((intptr_t)target + 5);
    if (jmp_rel < -2147483647 - 1 || jmp_rel > 2147483647) {
        kbo_log_runtimef(
            "KBO all-star roster single-division gate hook skipped reason=stub_out_of_range target=%p stub=%p",
            target,
            stub);
        return 0;
    }

    uint8_t patch[8] = {0xE9, 0,0,0,0, 0x90,0x90,0x90};
    write_u32(&patch[1], (uint32_t)(int32_t)jmp_rel);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO all-star roster single-division gate hook error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);
    kbo_log_runtimef(
        "installed KBO all-star roster single-division gate hook target=%p stub=%p return=%p skip=%p helper=%p",
        target,
        stub,
        return_address,
        skip_address,
        &ootp_kbo_allow_single_division_allstar_roster);
    return 1;
}

int install_single_division_allstar_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO single-division all-star patch");
    if (exe == NULL) {
        return 0;
    }

    int ok = 0;
    kbo_log_runtime_line("KBO single-division all-star scoped prep/roster gates enabled with capture and wrapper hooks");
    ok |= install_allstar_prep_single_division_gate_patch(exe);
    ok |= install_allstar_roster_single_division_gate_patch(exe);

    /* Search for the schedule import gate with both known exact patterns first,
     * then fall back to a flexible structural search that handles any register
     * (R13/R14/R15) and any game_flag offset in the 0x44xx-0x47xx range. */
    const uint8_t schedule_expected_may[] = {
        0x41, 0x80, 0xBD, 0xF0, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x7F,
        0x41, 0xC6, 0x85, 0xF0, 0x45, 0x00, 0x00, 0x01
    };
    const uint8_t schedule_expected_april[] = {
        0x41, 0x80, 0xBD, 0xE8, 0x45, 0x00, 0x00, 0x00,
        0x74, 0x7F,
        0x41, 0xC6, 0x85, 0xE8, 0x45, 0x00, 0x00, 0x01
    };
    uint8_t* schedule_target = find_ootp_executable_pattern(schedule_expected_may, sizeof(schedule_expected_may));
    const uint8_t* schedule_expected = schedule_expected_may;
    int schedule_reg_idx = 0;
    uint32_t schedule_game_flag_override = 0;

    if (schedule_target == NULL) {
        schedule_target = find_ootp_executable_pattern(schedule_expected_april, sizeof(schedule_expected_april));
        schedule_expected = schedule_expected_april;
    }
    if (schedule_target == NULL) {
        /* Flexible search: CMP BYTE PTR [R{13|14|15}+off], 0  /  JE  /  MOV BYTE PTR [same reg+off], 1
         * where off is in the 0x4400-0x47FF range (any low byte, high byte 0x44-0x47). */
        static const uint8_t cmp_modrm[3] = {0xBD, 0xBE, 0xBF};
        static const uint8_t mov_modrm[3] = {0x85, 0x86, 0x87};
        HMODULE flex_exe = GetModuleHandleA(NULL);
        uint8_t* flex_base = (flex_exe != NULL) ? (uint8_t*)flex_exe : NULL;
        if (flex_base != NULL) {
            IMAGE_DOS_HEADER* fdos = (IMAGE_DOS_HEADER*)flex_base;
            IMAGE_NT_HEADERS* fnt = (IMAGE_NT_HEADERS*)(flex_base + fdos->e_lfanew);
            IMAGE_SECTION_HEADER* fsects = IMAGE_FIRST_SECTION(fnt);
            WORD fnsects = fnt->FileHeader.NumberOfSections;
            for (WORD fs = 0; fs < fnsects && schedule_target == NULL; fs++) {
                DWORD fchars = fsects[fs].Characteristics;
                if (!(fchars & IMAGE_SCN_MEM_EXECUTE) || !(fchars & IMAGE_SCN_MEM_READ) || !(fchars & IMAGE_SCN_CNT_CODE)) {
                    continue;
                }
                uint8_t* fsec = flex_base + fsects[fs].VirtualAddress;
                DWORD fvsz = fsects[fs].Misc.VirtualSize;
                if (fvsz < 20u) {
                    continue;
                }
                DWORD flim = fvsz - 20u;
                for (DWORD foff = 0; foff <= flim && schedule_target == NULL; foff++) {
                    uint8_t* p = fsec + foff;
                    if (p[0] != 0x41 || p[1] != 0x80) {
                        continue;
                    }
                    int ri = -1;
                    for (int r = 0; r < 3; r++) {
                        if (p[2] == cmp_modrm[r]) { ri = r; break; }
                    }
                    if (ri < 0) continue;
                    /* disp32: bytes 3-6. High byte (p[4]) in 0x44-0x47, upper two bytes zero. */
                    if (p[4] < 0x44u || p[4] > 0x47u || p[5] != 0x00u || p[6] != 0x00u) {
                        continue;
                    }
                    /* CMP immediate must be 0 */
                    if (p[7] != 0x00u) continue;
                    /* JE */
                    if (p[8] != 0x74u) continue;
                    /* MOV BYTE PTR [same reg + same offset], 1 */
                    if (!memory_range_readable(p + 10, 8)) continue;
                    if (p[10] != 0x41u || p[11] != 0xC6u || p[12] != mov_modrm[ri]) continue;
                    if (p[13] != p[3] || p[14] != p[4] || p[15] != 0x00u || p[16] != 0x00u) continue;
                    if (p[17] != 0x01u) continue;
                    schedule_target = p;
                    schedule_reg_idx = ri;
                    schedule_game_flag_override = (uint32_t)p[3] | ((uint32_t)p[4] << 8u);
                    schedule_expected = schedule_expected_may; /* use may size */
                    kbo_log_runtimef("KBO all-star schedule capture: flexible match at %p reg=R%d offset=0x%04X je=+0x%02X",
                        p, 13 + ri, schedule_game_flag_override, (unsigned)p[9]);
                }
            }
        }
    }
    int schedule_ok = 0;
    if (schedule_target == NULL || !memory_range_readable(schedule_target, sizeof(schedule_expected_may))) {
        kbo_log_runtimef("KBO all-star schedule import capture hook target unreadable target=%p", schedule_target);
    } else if (is_rip_absolute_jump_patch(schedule_target) || is_rax_absolute_jump_patch(schedule_target)) {
        kbo_log_runtimef("KBO all-star schedule import capture hook already installed target=%p", schedule_target);
        schedule_ok = 1;
    } else if (memcmp(schedule_target, schedule_expected, sizeof(schedule_expected_may)) != 0) {
        log_patch_bytes_mismatch("KBO all-star schedule import capture hook", schedule_target, sizeof(schedule_expected_may));
        log_extended_context("KBO all-star schedule import capture hook", schedule_target, 16, 80);
    } else {
        void* return_address = schedule_target + sizeof(schedule_expected_may);
        KboAllstarLayout layout = kbo_get_allstar_layout();
        uint32_t used_flag = (schedule_game_flag_override != 0u) ? schedule_game_flag_override : layout.game_flag_offset;
        uint8_t* stub = build_allstar_schedule_import_capture_stub_r(return_address, used_flag, schedule_reg_idx);
        if (stub == NULL) {
            kbo_log_runtime_line("failed to allocate KBO all-star schedule import capture hook stub");
        } else {
            uint8_t patch[18] = {
                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
                0,0,0,0,0,0,0,0,
                0x90, 0x90, 0x90, 0x90
            };
            write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

            DWORD old_protect = 0;
            if (!VirtualProtect(schedule_target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
                kbo_log_runtimef("VirtualProtect failed for KBO all-star schedule import capture hook error=%lu", GetLastError());
            } else {
                memcpy(schedule_target, patch, sizeof(patch));
                FlushInstructionCache(GetCurrentProcess(), schedule_target, sizeof(patch));
                DWORD ignored = 0;
                VirtualProtect(schedule_target, sizeof(patch), old_protect, &ignored);
                kbo_log_runtimef(
                    "installed KBO all-star schedule import capture hook target=%p stub=%p return=%p reg=R%d flag=0x%X helper=%p",
                    schedule_target,
                    stub,
                    return_address,
                    13 + schedule_reg_idx,
                    used_flag,
                    &ootp_kbo_capture_allstar_schedule_import_league);
                schedule_ok = 1;
            }
        }
    }
    ok |= schedule_ok;

    return ok;
}

int install_allstar_team_setup_single_division_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star team setup single-division gate patch");
    if (exe == NULL) {
        return 0;
    }

    const uint8_t expected[15] = {
        0xF6, 0x41, 0x4C, 0x01,
        0x0F, 0x85, 0x74, 0x02, 0x00, 0x00,
        0xB9, 0xD8, 0x00, 0x00, 0x00
    };

    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_ALLSTAR_TEAM_SETUP_SINGLE_DIVISION_GATE_RVA);
    if (!memory_range_readable(target, sizeof(expected))
            || (memcmp(target, expected, sizeof(expected)) != 0
                && !is_rip_absolute_jump_patch(target)
                && !is_rax_absolute_jump_patch(target))) {
        target = find_ootp_executable_pattern(expected, sizeof(expected));
    }

    if (target == NULL || !memory_range_readable(target, sizeof(expected))) {
        kbo_log_runtimef("KBO all-star team setup single-division gate hook target unreadable target=%p", target);
        return 0;
    }

    if (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO all-star team setup single-division gate hook already installed target=%p", target);
        return 1;
    }

    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO all-star team setup single-division gate hook", target, sizeof(expected));
        log_extended_context("KBO all-star team setup single-division gate hook", target, 16, 96);
        return 0;
    }

    int32_t rel = *(int32_t*)(target + 6);
    void* bail_address = target + 10 + rel;
    void* return_address = target + sizeof(expected);
    uint8_t* stub = build_allstar_team_setup_single_division_gate_stub(return_address, bail_address);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO all-star team setup single-division gate hook stub");
        return 0;
    }

    uint8_t patch[15] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0,0,0,0,0,0,0,0,
        0x90
    };
    write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO all-star team setup single-division gate hook error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);
    kbo_log_runtimef(
        "installed KBO all-star team setup single-division gate hook target=%p stub=%p return=%p bail=%p helper=%p",
        target,
        stub,
        return_address,
        bail_address,
        &ootp_kbo_allow_single_division_allstar_team_setup);
    return 1;
}

