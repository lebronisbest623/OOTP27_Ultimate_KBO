#include "../patch_helpers.h"
#include <stdio.h>
#include <string.h>
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../build_verify/build_verify.h"
#include "../../core/logging/core_log.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../patch_helpers_internal.h"
uint8_t* resolve_patch_target_by_rva_or_context_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    size_t expected_size,
    const uint8_t* context,
    size_t context_size,
    size_t target_offset,
    const char* label)
{
    const char* patch_label = label != NULL ? label : "patch target";
    if (exe == NULL || expected == NULL || expected_size == 0 || context == NULL || context_size == 0) {
        return NULL;
    }
    if (target_offset > context_size || expected_size > context_size - target_offset) {
        kbo_log_runtimef("%s invalid context target_offset=%llu context_size=%llu expected_size=%llu",
            patch_label,
            (unsigned long long)target_offset,
            (unsigned long long)context_size,
            (unsigned long long)expected_size);
        return NULL;
    }

    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, rva);
    if (memory_range_readable(target, expected_size) && memcmp(target, expected, expected_size) == 0) {
        kbo_log_runtimef(
            "%s resolved by RVA table target=%p rva=0x%llx original_rva=0x%08X",
            patch_label,
            target,
            (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
            rva);
        return target;
    }

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        kbo_log_runtimef("%s context scan failed: invalid DOS header", patch_label);
        return NULL;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        kbo_log_runtimef("%s context scan failed: invalid NT header", patch_label);
        return NULL;
    }

    uint8_t* found = NULL;
    int hits = 0;
    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;
    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD vsz = sections[s].Misc.VirtualSize;
        if (vsz < context_size || vsz < 32) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)context_size;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (memcmp(candidate, context, context_size) != 0) {
                continue;
            }

            hits++;
            if (hits == 1) {
                found = candidate;
            } else {
                kbo_log_runtimef("%s context pattern ambiguous hits=%d first=%p another=%p",
                    patch_label,
                    hits,
                    found,
                    candidate);
                return NULL;
            }
        }
    }

    if (hits != 1 || found == NULL) {
        if (target != NULL && memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        } else {
            kbo_log_runtimef("%s target unreadable target=%p", patch_label, target);
        }
        kbo_log_runtimef("%s context pattern not found hits=%d original_rva=0x%08X", patch_label, hits, rva);
        return NULL;
    }

    target = found + target_offset;
    if (!memory_range_readable(target, expected_size) || memcmp(target, expected, expected_size) != 0) {
        kbo_log_runtimef("%s context resolved target failed expected-byte check target=%p context=%p",
            patch_label,
            target,
            found);
        if (memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        }
        return NULL;
    }

    kbo_log_runtimef(
        "%s resolved by context signature target=%p rva=0x%llx original_rva=0x%08X context=%p",
        patch_label,
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        rva,
        found);
    return target;
}


uint8_t* resolve_patch_target_by_rva_or_masked_context_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    size_t expected_size,
    const uint8_t* context,
    const uint8_t* context_mask,
    size_t context_size,
    size_t target_offset,
    const char* label)
{
    const char* patch_label = label != NULL ? label : "patch target";
    if (exe == NULL || expected == NULL || expected_size == 0 || context == NULL || context_mask == NULL || context_size == 0) {
        return NULL;
    }
    if (target_offset > context_size || expected_size > context_size - target_offset) {
        kbo_log_runtimef("%s invalid masked context target_offset=%llu context_size=%llu expected_size=%llu",
            patch_label,
            (unsigned long long)target_offset,
            (unsigned long long)context_size,
            (unsigned long long)expected_size);
        return NULL;
    }

    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, rva);
    if (memory_range_readable(target, expected_size) && memcmp(target, expected, expected_size) == 0) {
        kbo_log_runtimef(
            "%s resolved by RVA table target=%p rva=0x%llx original_rva=0x%08X",
            patch_label,
            target,
            (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
            rva);
        return target;
    }

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        kbo_log_runtimef("%s masked context scan failed: invalid DOS header", patch_label);
        return NULL;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        kbo_log_runtimef("%s masked context scan failed: invalid NT header", patch_label);
        return NULL;
    }

    uint8_t* found = NULL;
    int hits = 0;
    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;
    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD vsz = sections[s].Misc.VirtualSize;
        if (vsz < context_size || vsz < 32) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)context_size;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (!kbo_memory_matches_masked_pattern(candidate, context, context_mask, context_size)) {
                continue;
            }

            hits++;
            if (hits == 1) {
                found = candidate;
            } else {
                kbo_log_runtimef("%s masked context pattern ambiguous hits=%d first=%p another=%p",
                    patch_label,
                    hits,
                    found,
                    candidate);
                return NULL;
            }
        }
    }

    if (hits != 1 || found == NULL) {
        if (target != NULL && memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        } else {
            kbo_log_runtimef("%s target unreadable target=%p", patch_label, target);
        }
        kbo_log_runtimef("%s masked context pattern not found hits=%d original_rva=0x%08X", patch_label, hits, rva);
        return NULL;
    }

    target = found + target_offset;
    if (!memory_range_readable(target, expected_size) || memcmp(target, expected, expected_size) != 0) {
        kbo_log_runtimef("%s masked context resolved target failed expected-byte check target=%p context=%p",
            patch_label,
            target,
            found);
        if (memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        }
        return NULL;
    }

    kbo_log_runtimef(
        "%s resolved by masked context signature target=%p rva=0x%llx original_rva=0x%08X context=%p",
        patch_label,
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        rva,
        found);
    return target;
}

uint8_t* resolve_patch_target_by_rva_or_masked_context_and_expected_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    const uint8_t* expected_mask,
    size_t expected_size,
    const uint8_t* context,
    const uint8_t* context_mask,
    size_t context_size,
    size_t target_offset,
    const char* label)
{
    const char* patch_label = label != NULL ? label : "patch target";
    if (exe == NULL || expected == NULL || expected_mask == NULL || expected_size == 0 || context == NULL || context_mask == NULL || context_size == 0) {
        return NULL;
    }
    if (target_offset > context_size || expected_size > context_size - target_offset) {
        kbo_log_runtimef("%s invalid masked context/expected target_offset=%llu context_size=%llu expected_size=%llu",
            patch_label,
            (unsigned long long)target_offset,
            (unsigned long long)context_size,
            (unsigned long long)expected_size);
        return NULL;
    }

    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, rva);
    if (memory_range_readable(target, expected_size)
            && kbo_memory_matches_masked_pattern(target, expected, expected_mask, expected_size)) {
        kbo_log_runtimef(
            "%s resolved by RVA table target=%p rva=0x%llx original_rva=0x%08X",
            patch_label,
            target,
            (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
            rva);
        return target;
    }

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        kbo_log_runtimef("%s masked context/expected scan failed: invalid DOS header", patch_label);
        return NULL;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        kbo_log_runtimef("%s masked context/expected scan failed: invalid NT header", patch_label);
        return NULL;
    }

    uint8_t* found = NULL;
    int hits = 0;
    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;
    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD vsz = sections[s].Misc.VirtualSize;
        if (vsz < context_size || vsz < 32) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)context_size;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (!kbo_memory_matches_masked_pattern(candidate, context, context_mask, context_size)) {
                continue;
            }

            hits++;
            if (hits == 1) {
                found = candidate;
            } else {
                kbo_log_runtimef("%s masked context/expected pattern ambiguous hits=%d first=%p another=%p",
                    patch_label,
                    hits,
                    found,
                    candidate);
                return NULL;
            }
        }
    }

    if (hits != 1 || found == NULL) {
        if (memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        } else {
            kbo_log_runtimef("%s target unreadable target=%p", patch_label, target);
        }
        kbo_log_runtimef("%s masked context/expected pattern not found hits=%d original_rva=0x%08X", patch_label, hits, rva);
        return NULL;
    }

    target = found + target_offset;
    if (!memory_range_readable(target, expected_size) || !kbo_memory_matches_masked_pattern(target, expected, expected_mask, expected_size)) {
        kbo_log_runtimef("%s masked context/expected resolved target failed expected-byte check target=%p context=%p",
            patch_label,
            target,
            found);
        if (memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        }
        return NULL;
    }

    kbo_log_runtimef(
        "%s resolved by masked context/expected signature target=%p rva=0x%llx original_rva=0x%08X context=%p",
        patch_label,
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        rva,
        found);
    return target;
}


void* resolve_relative_call_target(uint8_t* call_site)
{
    if (call_site == NULL || !memory_range_readable(call_site, 5) || call_site[0] != 0xE8) {
        return NULL;
    }

    int32_t rel = 0;
    memcpy(&rel, call_site + 1, sizeof(rel));
    return call_site + 5 + rel;
}

void* resolve_rip_relative_lea_target(uint8_t* instruction)
{
    if (instruction == NULL
        || !memory_range_readable(instruction, 7)
        || (instruction[0] != 0x48 && instruction[0] != 0x4C)
        || (instruction[1] != 0x8D && instruction[1] != 0x8B)
        || (instruction[2] & 0xC7u) != 0x05u) {
        return NULL;
    }

    int32_t rel = 0;
    memcpy(&rel, instruction + 3, sizeof(rel));
    return instruction + 7 + rel;
}
