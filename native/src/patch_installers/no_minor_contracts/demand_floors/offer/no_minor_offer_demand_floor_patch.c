#include "no_minor_offer_demand_floor_patch.h"
#include <stdio.h>
#include <string.h>
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/dates/core_current_date.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../patch_helpers/patch_helpers.h"
#include "../../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../../hook_stubs/foreign_signability_stubs/demand_floor/offer_demand_floor_stubs.h"

int install_kbo_no_minor_contract_offer_player_demand_floor_patch(
    HMODULE exe,
    const char* label,
    uint32_t rva,
    const uint8_t* expected,
    size_t stolen_len,
    int alt_path)
{
    if (exe == NULL || label == NULL || expected == NULL || stolen_len < 12 || stolen_len > 24) {
        return 0;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(exe, rva, expected, stolen_len, label);
    if (target == NULL) {
        return 0;
    }
    if (!memory_range_readable(target, stolen_len)) {
        kbo_log_runtimef("%s target unreadable target=%p", label, target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("%s already installed target=%p", label, target);
        return 1;
    }
    if (memcmp(target, expected, stolen_len) != 0) {
        log_patch_bytes_mismatch(label, target, stolen_len);
        return 0;
    }

    uint32_t source_rva = (uint32_t)((uintptr_t)target - (uintptr_t)exe);
    uint8_t* stub = alt_path
        ? build_kbo_fa_offer_player_demand_floor_17b50b4_stub(source_rva, target + stolen_len)
        : build_kbo_fa_offer_player_demand_floor_17a79bb_stub(source_rva, target + stolen_len);
    if (stub == NULL) {
        kbo_log_runtimef("failed to allocate %s detour stub", label);
        return 0;
    }

    uint8_t patch[24];
    memset(patch, 0x90, sizeof(patch));
    patch[0] = 0x48;                                    /* mov rax, stub */
    patch[1] = 0xB8;
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);
    patch[10] = 0xFF;                                   /* jmp rax */
    patch[11] = 0xE0;

    DWORD old_protect = 0;
    if (!VirtualProtect(target, stolen_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, stolen_len);
    FlushInstructionCache(GetCurrentProcess(), target, stolen_len);

    DWORD ignored = 0;
    VirtualProtect(target, stolen_len, old_protect, &ignored);

    kbo_log_runtimef(
        "installed %s target=%p stub=%p probe=%p",
        label,
        target,
        stub,
        &ootp_kbo_fa_offer_player_demand_floor_probe);
    return 1;
}
