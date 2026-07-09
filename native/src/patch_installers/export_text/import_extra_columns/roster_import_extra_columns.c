#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_rvas.generated.h"
#include "../../../build_verify/build_verify.h"
#include "../../../core/logging/core_log.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../common/patch_host.h"
#include "roster_import_extra_columns.h"
#include "roster_import_extra_columns_consume.h"

#define KBO_ROSTER_IMPORT_EXTRA_COLUMNS_PATCH_LEN 12u
#define KBO_ROSTER_IMPORT_EXTRA_COLUMNS_CONTEXT_TARGET_OFFSET 9u
#define KBO_ROSTER_IMPORT_EXTRA_COLUMNS_TAIL_PATCH_LEN 16u
#define KBO_ROSTER_IMPORT_EXTRA_COLUMNS_TAIL_CONTEXT_TARGET_OFFSET 30u
#define KBO_ROSTER_IMPORT_EXTRA_COLUMNS_TAIL_SITE_RVA 0x00634262u

static volatile LONG g_kbo_roster_import_extra_columns_patch_installed = 0;

static void kbo_roster_import_emit_u8(uint8_t** cursor, uint8_t value)
{
    **cursor = value;
    (*cursor)++;
}

static void kbo_roster_import_emit_bytes(uint8_t** cursor, const uint8_t* bytes, size_t size)
{
    memcpy(*cursor, bytes, size);
    *cursor += size;
}

static void kbo_roster_import_emit_u64(uint8_t** cursor, uint64_t value)
{
    write_u64(*cursor, value);
    *cursor += sizeof(uint64_t);
}

static void kbo_roster_import_emit_u32(uint8_t** cursor, uint32_t value)
{
    write_u32(*cursor, value);
    *cursor += sizeof(uint32_t);
}

static uint8_t* build_kbo_roster_import_extra_columns_stub(void* row_cell_fn, void* continuation)
{
    if (row_cell_fn == NULL || continuation == NULL) {
        return NULL;
    }

    uint8_t* code = (uint8_t*)VirtualAlloc(NULL, 160u, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (code == NULL) {
        return NULL;
    }

    uint8_t* p = code;
    const uint8_t prologue[] = {
        0x9C,                                           /* pushfq */
        0x50,                                           /* push rax */
        0x51,                                           /* push rcx */
        0x52,                                           /* push rdx */
        0x41, 0x50,                                     /* push r8 */
        0x41, 0x51,                                     /* push r9 */
        0x41, 0x52,                                     /* push r10 */
        0x41, 0x53,                                     /* push r11 */
        0x48, 0x83, 0xEC, 0x20,                         /* sub rsp, 0x20 */
        0x49, 0x8B, 0xCE,                               /* mov rcx, r14 */
        0x49, 0x8B, 0xD7,                               /* mov rdx, r15 */
        0x4C, 0x8B, 0xC6,                               /* mov r8, rsi */
        0x48, 0xB8                                      /* mov rax, helper */
    };
    kbo_roster_import_emit_bytes(&p, prologue, sizeof(prologue));
    kbo_roster_import_emit_u64(&p, (uint64_t)(uintptr_t)&kbo_roster_import_extra_columns_consume);

    const uint8_t call_and_restore[] = {
        0xFF, 0xD0,                                     /* call rax */
        0x48, 0x83, 0xC4, 0x20,                         /* add rsp, 0x20 */
        0x41, 0x5B,                                     /* pop r11 */
        0x41, 0x5A,                                     /* pop r10 */
        0x41, 0x59,                                     /* pop r9 */
        0x41, 0x58,                                     /* pop r8 */
        0x5A,                                           /* pop rdx */
        0x59,                                           /* pop rcx */
        0x58,                                           /* pop rax */
        0x9D,                                           /* popfq */
        0xFF, 0x06,                                     /* inc dword ptr [rsi] */
        0x8B, 0x16,                                     /* mov edx, [rsi] */
        0x49, 0x8B, 0xCF,                               /* mov rcx, r15 */
        0x48, 0xB8                                      /* mov rax, row_cell_fn */
    };
    kbo_roster_import_emit_bytes(&p, call_and_restore, sizeof(call_and_restore));
    kbo_roster_import_emit_u64(&p, (uint64_t)(uintptr_t)row_cell_fn);

    const uint8_t tail_prefix[] = {
        0xFF, 0xD0,                                     /* call rax */
        0x48, 0xB8                                      /* mov rax, continuation */
    };
    kbo_roster_import_emit_bytes(&p, tail_prefix, sizeof(tail_prefix));
    kbo_roster_import_emit_u64(&p, (uint64_t)(uintptr_t)continuation);
    kbo_roster_import_emit_u8(&p, 0xFF);                /* jmp rax */
    kbo_roster_import_emit_u8(&p, 0xE0);

    FlushInstructionCache(GetCurrentProcess(), code, (SIZE_T)(p - code));
    return code;
}

static uint8_t* build_kbo_roster_import_extra_columns_tail_stub(void* r12_value, void* continuation)
{
    if (r12_value == NULL || continuation == NULL) {
        return NULL;
    }

    uint8_t* code = (uint8_t*)VirtualAlloc(NULL, 192u, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (code == NULL) {
        return NULL;
    }

    uint8_t* p = code;
    const uint8_t prologue[] = {
        0x9C,                                           /* pushfq */
        0x50,                                           /* push rax */
        0x51,                                           /* push rcx */
        0x52,                                           /* push rdx */
        0x41, 0x50,                                     /* push r8 */
        0x41, 0x51,                                     /* push r9 */
        0x41, 0x52,                                     /* push r10 */
        0x41, 0x53,                                     /* push r11 */
        0x48, 0x83, 0xEC, 0x20,                         /* sub rsp, 0x20 */
        0x49, 0x8B, 0xCF,                               /* mov rcx, r15 */
        0x49, 0x8B, 0xD6,                               /* mov rdx, r14 */
        0x48, 0xB8                                      /* mov rax, helper */
    };
    kbo_roster_import_emit_bytes(&p, prologue, sizeof(prologue));
    kbo_roster_import_emit_u64(&p, (uint64_t)(uintptr_t)&kbo_roster_import_extra_columns_consume_tail);

    const uint8_t call_and_restore[] = {
        0xFF, 0xD0,                                     /* call rax */
        0x48, 0x83, 0xC4, 0x20,                         /* add rsp, 0x20 */
        0x41, 0x5B,                                     /* pop r11 */
        0x41, 0x5A,                                     /* pop r10 */
        0x41, 0x59,                                     /* pop r9 */
        0x41, 0x58,                                     /* pop r8 */
        0x5A,                                           /* pop rdx */
        0x59,                                           /* pop rcx */
        0x58,                                           /* pop rax */
        0x9D,                                           /* popfq */
        0x8B, 0x74, 0x24, 0x44,                         /* mov esi, [rsp+0x44] */
        0x49, 0xBC                                      /* mov r12, r12_value */
    };
    kbo_roster_import_emit_bytes(&p, call_and_restore, sizeof(call_and_restore));
    kbo_roster_import_emit_u64(&p, (uint64_t)(uintptr_t)r12_value);

    const uint8_t stolen_tail[] = {
        0xB8                                            /* mov eax, 0x51EB851F */
    };
    kbo_roster_import_emit_bytes(&p, stolen_tail, sizeof(stolen_tail));
    kbo_roster_import_emit_u32(&p, 0x51EB851Fu);

    const uint8_t jump_back[] = {
        0x48, 0xB8                                      /* mov rax, continuation */
    };
    kbo_roster_import_emit_bytes(&p, jump_back, sizeof(jump_back));
    kbo_roster_import_emit_u64(&p, (uint64_t)(uintptr_t)continuation);
    kbo_roster_import_emit_u8(&p, 0xFF);                /* jmp rax */
    kbo_roster_import_emit_u8(&p, 0xE0);

    FlushInstructionCache(GetCurrentProcess(), code, (SIZE_T)(p - code));
    return code;
}

static int kbo_install_roster_import_extra_columns_site(HMODULE exe)
{
    const char* label = "KBO roster import extra columns parser hook";
    const uint8_t expected[KBO_ROSTER_IMPORT_EXTRA_COLUMNS_PATCH_LEN] = {
        0xFF, 0x06,                                     /* inc dword ptr [rsi] */
        0x8B, 0x16,                                     /* mov edx, [rsi] */
        0x49, 0x8B, 0xCF,                               /* mov rcx, r15 */
        0xE8, 0, 0, 0, 0                                /* call row_cell */
    };
    const uint8_t expected_mask[KBO_ROSTER_IMPORT_EXTRA_COLUMNS_PATCH_LEN] = {
        1, 1,
        1, 1,
        1, 1, 1,
        1, 0, 0, 0, 0
    };
    const uint8_t context[] = {
        0xF2, 0x41, 0x0F, 0x11, 0x86, 0x48, 0x0C, 0x00, 0x00,
        0xFF, 0x06, 0x8B, 0x16, 0x49, 0x8B, 0xCF, 0xE8, 0, 0, 0, 0,
        0x48, 0x8B, 0xD0, 0x48, 0x8D, 0x4C, 0x24, 0x70, 0xE8, 0, 0, 0, 0,
        0xFF, 0x06
    };
    const uint8_t context_mask[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
        1, 1
    };

    uint8_t* direct_target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_ROSTER_IMPORT_EXTRA_COLUMNS_EOF_CHECK_RVA);
    if (memory_range_readable(direct_target, KBO_ROSTER_IMPORT_EXTRA_COLUMNS_PATCH_LEN)
            && is_rax_absolute_jump_patch(direct_target)) {
        kbo_log_runtimef("%s already installed target=%p", label, direct_target);
        return 1;
    }

    uint8_t* target = resolve_patch_target_by_rva_or_masked_context_and_expected_pattern(
        exe,
        OOTP27_ROSTER_IMPORT_EXTRA_COLUMNS_EOF_CHECK_RVA,
        expected,
        expected_mask,
        sizeof(expected),
        context,
        context_mask,
        sizeof(context),
        KBO_ROSTER_IMPORT_EXTRA_COLUMNS_CONTEXT_TARGET_OFFSET,
        label);
    if (target == NULL) {
        return 0;
    }

    void* row_cell_fn = resolve_relative_call_target(target + 7u);
    if (row_cell_fn == NULL || !memory_range_readable(row_cell_fn, 8u)) {
        kbo_log_runtimef("%s skipped: row cell helper target unresolved target=%p", label, target);
        return 0;
    }

    uint8_t* stub = build_kbo_roster_import_extra_columns_stub(
        row_cell_fn,
        target + KBO_ROSTER_IMPORT_EXTRA_COLUMNS_PATCH_LEN);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO roster import extra columns parser hook stub");
        return 0;
    }

    uint8_t patch[KBO_ROSTER_IMPORT_EXTRA_COLUMNS_PATCH_LEN] = {
        0x48, 0xB8,
        0, 0, 0, 0, 0, 0, 0, 0,
        0xFF, 0xE0
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("%s VirtualProtect failed error=%lu target=%p", label, GetLastError(), target);
        return 0;
    }
    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO roster import extra columns parser hook target=%p rva=0x%llx stub=%p row_cell=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        row_cell_fn);
    return 1;
}

static int kbo_install_roster_import_extra_columns_tail_site(HMODULE exe)
{
    const char* label = "KBO roster import extra columns roster tail hook";
    const uint8_t expected[KBO_ROSTER_IMPORT_EXTRA_COLUMNS_TAIL_PATCH_LEN] = {
        0x8B, 0x74, 0x24, 0x44,                         /* mov esi, [rsp+0x44] */
        0x4C, 0x8D, 0x25, 0, 0, 0, 0,                   /* lea r12, [rip+...] */
        0xB8, 0x1F, 0x85, 0xEB, 0x51                    /* mov eax, 0x51EB851F */
    };
    const uint8_t expected_mask[KBO_ROSTER_IMPORT_EXTRA_COLUMNS_TAIL_PATCH_LEN] = {
        1, 1, 1, 1,
        1, 1, 1, 0, 0, 0, 0,
        1, 1, 1, 1, 1
    };
    const uint8_t context[] = {
        0x41, 0x8B, 0x87, 0x7E, 0x0A, 0x00, 0x00,
        0x41, 0x89, 0x87, 0x8C, 0x0A, 0x00, 0x00,
        0x41, 0x0F, 0xB7, 0x87, 0x82, 0x0A, 0x00, 0x00,
        0x66, 0x41, 0x89, 0x87, 0x90, 0x0A, 0x00, 0x00,
        0x8B, 0x74, 0x24, 0x44,
        0x4C, 0x8D, 0x25, 0, 0, 0, 0,
        0xB8, 0x1F, 0x85, 0xEB, 0x51,
        0x8B, 0x5C, 0x24, 0x70,
        0xF7, 0xEB,
        0xC1, 0xFA, 0x04
    };
    const uint8_t context_mask[] = {
        1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 0, 0, 0, 0,
        1, 1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1,
        1, 1, 1
    };

    uint8_t* target = resolve_patch_target_by_rva_or_masked_context_and_expected_pattern(
        exe,
        KBO_ROSTER_IMPORT_EXTRA_COLUMNS_TAIL_SITE_RVA,
        expected,
        expected_mask,
        sizeof(expected),
        context,
        context_mask,
        sizeof(context),
        KBO_ROSTER_IMPORT_EXTRA_COLUMNS_TAIL_CONTEXT_TARGET_OFFSET,
        label);
    if (target == NULL) {
        return 0;
    }

    void* r12_value = resolve_rip_relative_lea_target(target + 4u);
    if (r12_value == NULL) {
        kbo_log_runtimef("%s skipped: stolen lea target unresolved target=%p", label, target);
        return 0;
    }

    uint8_t* stub = build_kbo_roster_import_extra_columns_tail_stub(
        r12_value,
        target + KBO_ROSTER_IMPORT_EXTRA_COLUMNS_TAIL_PATCH_LEN);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO roster import extra columns roster tail hook stub");
        return 0;
    }

    uint8_t patch[KBO_ROSTER_IMPORT_EXTRA_COLUMNS_TAIL_PATCH_LEN] = {
        0x48, 0xB8,
        0, 0, 0, 0, 0, 0, 0, 0,
        0xFF, 0xE0,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("%s VirtualProtect failed error=%lu target=%p", label, GetLastError(), target);
        return 0;
    }
    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO roster import extra columns roster tail hook target=%p rva=0x%llx stub=%p r12_value=%p",
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        stub,
        r12_value);
    return 1;
}

int install_kbo_roster_import_extra_columns_patch(void)
{
    if (InterlockedCompareExchange(&g_kbo_roster_import_extra_columns_patch_installed, 1, 0) != 0) {
        kbo_log_runtime_line("KBO roster import extra columns parser hook already requested");
        return 1;
    }

    char host_path[MAX_PATH];
    DWORD host_len = GetModuleFileNameA(NULL, host_path, (DWORD)sizeof(host_path));
    host_path[(host_len > 0 && host_len < (DWORD)sizeof(host_path)) ? host_len : 0] = '\0';
    if (!kbo_patch_host_matches_product(host_path)) {
        kbo_log_runtimef("KBO roster import extra columns parser hook skipped for host=%s", host_path);
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("KBO roster import extra columns parser hook skipped: executable module unavailable");
        return 0;
    }

    if (kbo_install_roster_import_extra_columns_tail_site(exe)) {
        return 1;
    }

    if (!kbo_install_roster_import_extra_columns_site(exe)) {
        InterlockedExchange(&g_kbo_roster_import_extra_columns_patch_installed, 0);
        return 0;
    }
    return 1;
}
