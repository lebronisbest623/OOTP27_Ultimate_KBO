#include "patch_installers_foreign_ai_fa_fallback.h"
#include <stdio.h>
#include <string.h>
#include "../../common/patch_host.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../patch_helpers/patch_helpers.h"

int install_kbo_ai_fa_signability_random_fallback_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO AI FA signability fallback patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO AI FA signability fallback patch host=%s", host);
        return 0;
    }

    const uint8_t expected[60] = {
        0x85, 0xC0,                                           /* test eax,eax */
        0x75, 0x38,                                           /* jne +0x38 */
        0x8B, 0x8B, 0xB8, 0x0F, 0x00, 0x00,                   /* mov ecx,[rbx+0xfb8] */
        0x48, 0x8B, 0x85, 0x40, 0x01, 0x00, 0x00,             /* mov rax,[rbp+0x140] */
        0x03, 0x88, 0x50, 0x44, 0x00, 0x00,                   /* add ecx,[rax+0x4450] */
        0xB8, 0xE9, 0xA2, 0x8B, 0x2E,                         /* mov eax,0x2e8ba2e9 */
        0xF7, 0xE9,                                           /* imul ecx */
        0xD1, 0xFA,                                           /* sar edx,1 */
        0x8B, 0xC2,                                           /* mov eax,edx */
        0xC1, 0xE8, 0x1F,                                     /* shr eax,0x1f */
        0x03, 0xD0,                                           /* add edx,eax */
        0x6B, 0xC2, 0x0B,                                     /* imul eax,edx,0xb */
        0x2B, 0xC8,                                           /* sub ecx,eax */
        0x45, 0x0F, 0xB6, 0xF6,                               /* movzx r14d,r14b */
        0x83, 0xF9, 0x01,                                     /* cmp ecx,1 */
        0xB8, 0x00, 0x00, 0x00, 0x00,                         /* mov eax,0 */
        0x44, 0x0F, 0x45, 0xF0                                /* cmovne r14d,eax */
    };

    uint8_t* target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_AI_FA_SIGNABILITY_RANDOM_FALLBACK_RVA,
        expected,
        sizeof(expected),
        "KBO AI FA signability fallback patch");
    if (target == NULL) { return 0; }

    uint8_t patch[60];
    memset(patch, 0x90, sizeof(patch));
    patch[0] = 0x85;                                         /* test eax,eax */
    patch[1] = 0xC0;
    patch[2] = 0x0F;                                         /* je 0x140af1fe2 */
    patch[3] = 0x84;
    patch[4] = 0x57;
    patch[5] = 0x02;
    patch[6] = 0x00;
    patch[7] = 0x00;
    patch[8] = 0xE9;                                         /* jmp 0x140af1dbf */
    patch[9] = 0x2F;
    patch[10] = 0x00;
    patch[11] = 0x00;
    patch[12] = 0x00;

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO AI FA signability fallback patch error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef("installed KBO AI FA signability fallback patch target=%p", target);
    return 1;
}
