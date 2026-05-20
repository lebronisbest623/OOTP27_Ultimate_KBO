#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "../../../common/patch_host.h"

#include "../../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../hook_stubs/military/hook_stubs_military.h"
#include "../../../../hook_stubs/foreign_signability_stubs/signing/signing_branch_stubs.h"
#include "../../../../patch_helpers/patch_helpers.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "patch_installers_foreign_signing_branch.h"
static int kbo_fa_signing_branch_has_news_literal(uint8_t* signature_start)
{
    static const char literal[] = "Signed free agent %p.";
    if (signature_start == NULL) {
        return 0;
    }

    for (size_t off = 0; off < 0x140u; off++) {
        uint8_t* insn = signature_start + off;
        if (!memory_range_readable(insn, 7u)) {
            continue;
        }
        if (insn[0] != 0x4c || insn[1] != 0x8d || insn[2] != 0x05) {
            continue;
        }

        int32_t rel = 0;
        memcpy(&rel, insn + 3, sizeof(rel));
        const char* target = (const char*)(insn + 7 + rel);
        if (memory_range_readable(target, sizeof(literal)) && memcmp(target, literal, sizeof(literal)) == 0) {
            return 1;
        }
    }

    return 0;
}

static int kbo_matches_fa_signing_branch_signature(uint8_t* candidate)
{
    static const uint8_t pattern[] = {
        0x41, 0x8B, 0x57, 0x58,                         /* mov edx,[r15+0x58] */
        0x85, 0xD2,                                     /* test edx,edx */
        0x74, 0x0C,                                     /* je +0x0c */
        0x49, 0x8B, 0xCF,                               /* mov rcx,r15 */
        0xE8, 0x00, 0x00, 0x00, 0x00,                   /* call player team cleanup */
        0x41, 0x8B, 0x57, 0x58,                         /* mov edx,[r15+0x58] */
        0x45, 0x33, 0xF6,                               /* xor r14d,r14d */
        0x41, 0x89, 0x57, 0x5C,
        0x45, 0x89, 0x77, 0x58,
        0x41, 0x8B, 0x47, 0x60,
        0x41, 0x89, 0x47, 0x64,
        0x45, 0x89, 0x77, 0x60,
        0x41, 0x8B, 0x47, 0x48,
        0x41, 0x89, 0x47, 0x4C,
        0x45, 0x89, 0x77, 0x48,
        0x66, 0x41, 0xC7, 0x47, 0x41, 0x01, 0x01,
        0x4D, 0x85, 0xE4,
        0x0F, 0x84
    };

    if (candidate == NULL) {
        return 0;
    }

    for (size_t i = 0; i < sizeof(pattern); i++) {
        if (i >= 12u && i <= 15u) {
            continue;
        }
        if (candidate[i] != pattern[i]) {
            return 0;
        }
    }

    return kbo_fa_signing_branch_has_news_literal(candidate);
}

static uint8_t* resolve_kbo_fa_signing_branch_target(void)
{
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

    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;

    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD vsz = sections[s].Misc.VirtualSize;
        if (vsz < 96u) {
            continue;
        }

        DWORD scan_limit = vsz - 96u;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (kbo_matches_fa_signing_branch_signature(candidate)) {
                return candidate + OOTP27_FA_SIGNING_BRANCH_PATCH_OFFSET;
            }
        }
    }

    return NULL;
}

int install_kbo_fa_signing_branch_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO FA signing branch patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO FA signing branch patch host=%s", host);
        return 0;
    }

    uint8_t* target = resolve_kbo_fa_signing_branch_target();
    uint8_t* post_target = target != NULL
        ? target + (OOTP27_FA_SIGNING_BRANCH_POST_SUCCESS_OFFSET - OOTP27_FA_SIGNING_BRANCH_PATCH_OFFSET)
        : NULL;
    const size_t stolen_len = 16;
    const uint8_t expected[16] = {
        0x45, 0x89, 0x77, 0x60,                         /* mov [r15+0x60],r14d */
        0x41, 0x8B, 0x47, 0x48,                         /* mov eax,[r15+0x48] */
        0x41, 0x89, 0x47, 0x4C,                         /* mov [r15+0x4c],eax */
        0x45, 0x89, 0x77, 0x48                          /* mov [r15+0x48],r14d */
    };
    const uint8_t post_expected[5] = {
        0x41, 0xC6, 0x45, 0x68, 0x01                    /* mov byte ptr [r13+0x68],1 */
    };

    if (target == NULL) {
        kbo_log_runtime_line("Could not resolve KBO FA signing branch by signature");
        return 0;
    }
    if (!memory_range_readable(target, stolen_len)) {
        kbo_log_runtimef("KBO FA signing branch patch target unreadable target=%p", target);
        return 0;
    }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO FA signing branch patch already installed target=%p", target);
        return 1;
    }
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        log_patch_bytes_mismatch("KBO FA signing branch patch", target, sizeof(expected));
        return 0;
    }

    if (post_target == NULL || !memory_range_readable(post_target, sizeof(post_expected))) {
        kbo_log_runtimef("KBO FA signing success post patch target unreadable target=%p", post_target);
    } else if (post_target[0] == 0xE9) {
        kbo_log_runtimef("KBO FA signing success post patch already installed target=%p", post_target);
    } else if (memcmp(post_target, post_expected, sizeof(post_expected)) != 0) {
        log_patch_bytes_mismatch("KBO FA signing success post patch", post_target, sizeof(post_expected));
    } else {
        uint8_t* post_stub = build_kbo_fa_signing_success_post_stub(post_target + sizeof(post_expected), post_target);
        if (post_stub == NULL) {
            kbo_log_runtime_line("failed to allocate KBO FA signing success post detour stub");
        } else {
            intptr_t rel = (intptr_t)post_stub - ((intptr_t)post_target + 5);
            if (rel < INT32_MIN || rel > INT32_MAX) {
                kbo_log_runtimef("KBO FA signing success post patch skipped reason=stub_out_of_range target=%p stub=%p", post_target, post_stub);
            } else {
                uint8_t post_patch[5] = {0xE9, 0,0,0,0};
                int32_t rel32 = (int32_t)rel;
                memcpy(&post_patch[1], &rel32, sizeof(rel32));
                DWORD post_old_protect = 0;
                if (!VirtualProtect(post_target, sizeof(post_patch), PAGE_EXECUTE_READWRITE, &post_old_protect)) {
                    kbo_log_runtimef("VirtualProtect failed for KBO FA signing success post patch error=%lu", GetLastError());
                } else {
                    memcpy(post_target, post_patch, sizeof(post_patch));
                    FlushInstructionCache(GetCurrentProcess(), post_target, sizeof(post_patch));
                    DWORD ignored = 0;
                    VirtualProtect(post_target, sizeof(post_patch), post_old_protect, &ignored);
                    kbo_log_runtimef(
                        "installed KBO FA signing success post patch target=%p rva=0x%llx stub=%p wrapper=%p",
                        post_target,
                        (unsigned long long)((uintptr_t)post_target - (uintptr_t)exe),
                        post_stub,
                        &ootp_kbo_fa_signing_success_post_wrapper);
                }
            }
        }
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO FA signing branch trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_fa_signing_branch_detour_stub(trampoline);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO FA signing branch detour stub");
        return 0;
    }

    uint8_t patch[16] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO FA signing branch patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO FA signing branch patch target=%p rva=0x%llx stub=%p trampoline=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        trampoline,
        &ootp_kbo_fa_signing_branch_wrapper);
    return 1;
}

