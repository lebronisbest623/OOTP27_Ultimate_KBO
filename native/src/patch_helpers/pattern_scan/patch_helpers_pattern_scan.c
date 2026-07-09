#include "../patch_helpers_internal.h"
#include "../../build_verify/build_verify.h"

uint8_t* resolve_patch_target_by_rva_or_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    size_t expected_size,
    const char* label)
{
    const char* patch_label = label != NULL ? label : "patch target";
    if (exe == NULL || expected == NULL || expected_size == 0) {
        return NULL;
    }

    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, rva);
    if (memory_range_readable(target, expected_size) && memcmp(target, expected, expected_size) == 0) {
        kbo_log_runtimef(
            "%s resolved by rva target=%p rva=0x%08X",
            patch_label,
            target,
            rva);
        return target;
    }

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        kbo_log_runtimef("%s signature scan failed: invalid DOS header", patch_label);
        return NULL;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        kbo_log_runtimef("%s signature scan failed: invalid NT header", patch_label);
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
        if (vsz < expected_size || vsz < 32) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)expected_size;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (memcmp(candidate, expected, expected_size) != 0) {
                continue;
            }

            hits++;
            if (hits == 1) {
                found = candidate;
            } else {
                kbo_log_runtimef(
                    "%s signature pattern ambiguous hits=%d first=%p another=%p original_rva=0x%08X",
                    patch_label,
                    hits,
                    found,
                    candidate,
                    rva);
                return NULL;
            }
        }
    }

    if (hits == 1 && found != NULL) {
        kbo_log_runtimef(
            "%s resolved by unique signature target=%p rva=0x%llx original_rva=0x%08X",
            patch_label,
            found,
            (unsigned long long)((uintptr_t)found - (uintptr_t)exe),
            rva);
        return found;
    }

    if (target != NULL && memory_range_readable(target, expected_size)) {
        log_patch_bytes_mismatch(patch_label, target, expected_size);
    } else {
        kbo_log_runtimef("%s target unreadable target=%p", patch_label, target);
    }
    kbo_log_runtimef("%s signature pattern not found hits=%d original_rva=0x%08X", patch_label, hits, rva);
    return NULL;
}

uint8_t* resolve_patch_target_by_rva_or_masked_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    const uint8_t* expected_mask,
    size_t expected_size,
    const char* label)
{
    const char* patch_label = label != NULL ? label : "patch target";
    if (exe == NULL || expected == NULL || expected_mask == NULL || expected_size == 0) {
        return NULL;
    }

    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, rva);
    if (memory_range_readable(target, expected_size)
            && kbo_memory_matches_masked_pattern(target, expected, expected_mask, expected_size)) {
        kbo_log_runtimef(
            "%s resolved by masked rva target=%p rva=0x%08X",
            patch_label,
            target,
            rva);
        return target;
    }

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        kbo_log_runtimef("%s masked signature scan failed: invalid DOS header", patch_label);
        return NULL;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        kbo_log_runtimef("%s masked signature scan failed: invalid NT header", patch_label);
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
        if (vsz < expected_size || vsz < 32) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)expected_size;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (!kbo_memory_matches_masked_pattern(candidate, expected, expected_mask, expected_size)) {
                continue;
            }

            hits++;
            if (hits == 1) {
                found = candidate;
            } else {
                kbo_log_runtimef(
                    "%s masked signature pattern ambiguous hits=%d first=%p another=%p original_rva=0x%08X",
                    patch_label,
                    hits,
                    found,
                    candidate,
                    rva);
                return NULL;
            }
        }
    }

    if (hits == 1 && found != NULL) {
        kbo_log_runtimef(
            "%s resolved by unique masked signature target=%p rva=0x%llx original_rva=0x%08X",
            patch_label,
            found,
            (unsigned long long)((uintptr_t)found - (uintptr_t)exe),
            rva);
        return found;
    }

    if (target != NULL && memory_range_readable(target, expected_size)) {
        log_patch_bytes_mismatch(patch_label, target, expected_size);
    } else {
        kbo_log_runtimef("%s target unreadable target=%p", patch_label, target);
    }
    kbo_log_runtimef("%s masked signature pattern not found hits=%d original_rva=0x%08X", patch_label, hits, rva);
    return NULL;
}

uint8_t* resolve_patch_target_by_rva_existing_rax_or_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    size_t expected_size,
    const char* label)
{
    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, rva);
    if (memory_range_readable(target, 12) && is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("%s already installed target=%p", label != NULL ? label : "patch target", target);
        return target;
    }
    return resolve_patch_target_by_rva_or_pattern(exe, rva, expected, expected_size, label);
}

uint8_t* resolve_patch_target_by_rva_existing_rax_or_masked_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    const uint8_t* expected_mask,
    size_t expected_size,
    const char* label)
{
    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, rva);
    if (memory_range_readable(target, 12) && is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("%s already installed target=%p", label != NULL ? label : "patch target", target);
        return target;
    }
    return resolve_patch_target_by_rva_or_masked_pattern(
        exe,
        rva,
        expected,
        expected_mask,
        expected_size,
        label);
}

uint8_t* find_ootp_executable_pattern_nth(const uint8_t* pattern, size_t pattern_size, int desired_index, int expected_hits)
{
    if (pattern == NULL || pattern_size == 0 || desired_index < 0 || expected_hits <= desired_index) {
        return NULL;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }
    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return NULL;
    }
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
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
        if (vsz < pattern_size) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)pattern_size;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (memcmp(candidate, pattern, pattern_size) != 0) {
                continue;
            }
            if (hits == desired_index) {
                found = candidate;
            }
            hits++;
        }
    }

    if (hits != expected_hits) {
        return NULL;
    }
    return found;
}
