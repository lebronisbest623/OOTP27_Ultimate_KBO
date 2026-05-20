#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "../../../common/patch_host.h"

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../hook_stubs/foreign/ai_status/hook_stubs_foreign_ai_status.h"
#include "../../../../patch_helpers/patch_helpers.h"

int install_kbo_foreign_ai_offer_candidate_priority_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO foreign AI offer candidate priority patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO foreign AI offer candidate priority patch host=%s", host);
        return 0;
    }

    const size_t patch_len = 16;
    const uint8_t expected[16] = {
        0x4C, 0x89, 0x65, 0xA0,                         /* mov [rbp-0x60],r12 */
        0x41, 0x80, 0x7C, 0x24, 0x41, 0x00,             /* cmp byte ptr [r12+0x41],0 */
        0x0F, 0x84, 0x1A, 0x0A, 0x00, 0x00              /* je skip candidate */
    };

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_AI_FA_OFFER_CANDIDATE_SELECTED_RVA,
        expected,
        sizeof(expected),
        "KBO foreign AI offer candidate priority patch");
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO foreign AI offer candidate priority patch already installed target=%p", target);
        return 1;
    }

    uintptr_t skip_delta =
        (uintptr_t)OOTP27_AI_FA_OFFER_CANDIDATE_SKIP_RVA
        - (uintptr_t)OOTP27_AI_FA_OFFER_CANDIDATE_SELECTED_RVA;
    uint8_t* skip = target + skip_delta;
    uint8_t* stub = build_kbo_foreign_ai_offer_candidate_priority_stub(target + patch_len, skip);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO foreign AI offer candidate priority stub");
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
        kbo_log_runtimef("VirtualProtect failed for KBO foreign AI offer candidate priority patch error=%lu", GetLastError());
        return 0;
    }
    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO foreign AI offer candidate priority patch target=%p rva=0x%llx stub=%p continue=%p skip=%p wrapper=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        target + patch_len,
        skip,
        &ootp_kbo_foreign_ai_offer_candidate_priority_wrapper);
    return 1;
}
