#include "../no_withdraw/arbitration_no_withdraw_patch_module.h"

int32_t kbo_salary_arbitration_resolve_minimum_salary(uint32_t league_id)
{
    (void)league_id;
    int32_t current_floor = kbo_no_minor_current_league_minimum_salary();
    return current_floor > 0 ? current_floor : 0;
}

uintptr_t kbo_salary_arbitration_caller_rva(void* return_address)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL || return_address == NULL) {
        return 0;
    }
    return (uintptr_t)return_address - (uintptr_t)exe;
}

int kbo_salary_arbitration_is_known_non_tender_return(uintptr_t caller_rva)
{
    return caller_rva == 0x0067C0F1u
        || caller_rva == 0x00681B6Bu
        || caller_rva == 0x006820C6u;
}

int patch_kbo_salary_arbitration_r11_detour_at(
    const char* label,
    uint8_t* target,
    const uint8_t* expected,
    size_t size,
    uint8_t* stub);

int patch_kbo_salary_arbitration_r11_detour_at(
    const char* label,
    uint8_t* target,
    const uint8_t* expected,
    size_t size,
    uint8_t* stub)
{
    if (label == NULL || target == NULL || expected == NULL || size < 13 || stub == NULL) {
        return 0;
    }

    if (!memory_range_readable(target, size)) {
        kbo_log_runtimef("%s target unreadable target=%p", label, target);
        return 0;
    }
    if (is_r11_absolute_jump_patch(target)) {
        kbo_log_runtimef("%s already installed target=%p", label, target);
        return 1;
    }
    if (memcmp(target, expected, size) != 0) {
        log_patch_bytes_mismatch(label, target, size);
        return 0;
    }

    uint8_t patch[32] = {
        0x49, 0xBB,                                     /* mov r11, stub */
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3                                /* jmp r11 */
    };
    for (size_t i = 13; i < size && i < sizeof(patch); i++) {
        patch[i] = 0x90;
    }
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);

    DWORD ignored = 0;
    VirtualProtect(target, size, old_protect, &ignored);

    kbo_log_runtimef("installed %s target=%p stub=%p", label, target, stub);
    return 1;
}

int install_kbo_salary_arbitration_non_tender_function_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    const size_t stolen_len = 18;
    const uint8_t expected[18] = {
        0x48, 0x89, 0x54, 0x24, 0x10,                   /* mov [rsp+0x10], rdx */
        0x48, 0x89, 0x4C, 0x24, 0x08,                   /* mov [rsp+0x8], rcx */
        0x55,                                           /* push rbp */
        0x53,                                           /* push rbx */
        0x56,                                           /* push rsi */
        0x57,                                           /* push rdi */
        0x41, 0x54,                                     /* push r12 */
        0x41, 0x55                                      /* push r13 */
    };
    const uint8_t context[48] = {
        0x83, 0x68, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3, 0xCC,
        0x48, 0x89, 0x54, 0x24, 0x10, 0x48, 0x89, 0x4C,
        0x24, 0x08, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54,
        0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D,
        0x6C, 0x24, 0xE1, 0x48, 0x81, 0xEC, 0x88, 0x00
    };

    uint8_t* target = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_ARBITRATION_NON_TENDER_FUNCTION_RVA,
        expected,
        sizeof(expected),
        context,
        sizeof(context),
        16u,
        "KBO salary arbitration non-tender function");
    if (target == NULL) { return 0; }
    if (is_rax_absolute_jump_patch(target)) {
        kbo_log_runtimef("KBO salary arbitration non-tender function already installed target=%p", target);
        return 1;
    }

    uint8_t* trampoline = build_kbo_military_service_entry_trampoline(target, stolen_len);
    if (trampoline == NULL) {
        kbo_log_runtime_line("failed to allocate KBO salary arbitration non-tender trampoline");
        return 0;
    }

    uint8_t* stub = build_kbo_salary_arbitration_non_tender_detour_stub(trampoline);
    if (stub == NULL) {
        kbo_log_runtime_line("failed to allocate KBO salary arbitration non-tender detour stub");
        return 0;
    }

    uint8_t patch[18] = {
        0x48, 0xB8,                                     /* mov rax, stub */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[2], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for KBO salary arbitration non-tender function error=%lu", GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);

    kbo_log_runtimef(
        "installed KBO salary arbitration non-tender function target=%p stub=%p trampoline=%p wrapper=%p",
        target,
        stub,
        trampoline,
        &ootp_kbo_salary_arbitration_non_tender_wrapper);
    return 1;
}

