#include "hook_stubs_foreign_ai_status.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"

uint8_t* build_kbo_ai_fa_status_candidate_insert_stub(void* continuation)
{
    uint8_t code[80] = {
        0x41, 0x89, 0xC8,                               // mov r8d, ecx
        0x48, 0x89, 0xE9,                               // mov rcx, rbp
        0x4C, 0x89, 0xFA,                               // mov rdx, r15
        0x4D, 0x89, 0xF1,                               // mov r9, r14
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x89, 0xC1,                                     // mov ecx, eax
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[14], (uint64_t)(uintptr_t)&ootp_kbo_ai_fa_status_candidate_insert_wrapper);
    write_u64(&code[28], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_ai_fa_status_candidate_insert_primary_stub(void* continuation)
{
    uint8_t code[80] = {
        0x41, 0x89, 0xD0,                               // mov r8d, edx
        0x48, 0x89, 0xE9,                               // mov rcx, rbp
        0x48, 0x89, 0xDA,                               // mov rdx, rbx
        0x4C, 0x8B, 0x4D, 0xC8,                         // mov r9, [rbp-0x38]
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x89, 0xC2,                                     // mov edx, eax
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC
    };

    write_u64(&code[15], (uint64_t)(uintptr_t)&ootp_kbo_ai_fa_status_candidate_insert_wrapper);
    write_u64(&code[29], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_ai_fa_status_candidate_insert_direct_stub(void* continuation)
{
    uint8_t code[80] = {
        0x4C, 0x8B, 0x75, 0xC8,                         // mov r14, [rbp-0x38]
        0x41, 0x89, 0xC8,                               // mov r8d, ecx
        0x48, 0x89, 0xE9,                               // mov rcx, rbp
        0x4C, 0x89, 0xFA,                               // mov rdx, r15
        0x4D, 0x89, 0xF1,                               // mov r9, r14
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x89, 0xC1,                                     // mov ecx, eax
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[18], (uint64_t)(uintptr_t)&ootp_kbo_ai_fa_status_candidate_insert_wrapper);
    write_u64(&code[32], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_foreign_ai_offer_candidate_priority_stub(void* success_continuation, void* skip_continuation)
{
    uint8_t code[] = {
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp,0x20
        0x48, 0x89, 0xE9,                               // mov rcx,rbp
        0x4C, 0x89, 0xE2,                               // mov rdx,r12
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp,0x20
        0x49, 0x89, 0xC4,                               // mov r12,rax
        0x4C, 0x89, 0x65, 0xA0,                         // mov [rbp-0x60],r12
        0x41, 0x80, 0x7C, 0x24, 0x41, 0x00,             // cmp byte ptr [r12+0x41],0
        0x0F, 0x84, 0x0D, 0x00, 0x00, 0x00,             // je skip
        0x49, 0xBB,                                     // mov r11,success_continuation
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               // jmp r11
        0x49, 0xBB,                                     // mov r11,skip_continuation
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               // jmp r11
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_foreign_ai_offer_candidate_priority_wrapper);
    write_u64(&code[47], (uint64_t)(uintptr_t)success_continuation);
    write_u64(&code[60], (uint64_t)(uintptr_t)skip_continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_foreign_ai_offer_attach_probe_detour_stub(void* original_trampoline)
{
    uint8_t code[64] = {
        0x4C, 0x8B, 0x04, 0x24,                         // mov r8,[rsp]
        0x49, 0xB9,                                     // mov r9, original_trampoline
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[6], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[16], (uint64_t)(uintptr_t)&ootp_kbo_foreign_ai_offer_attach_probe_wrapper);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_foreign_ai_offer_build_probe_stub(void* continuation)
{
    uint8_t code[] = {
        0xC6, 0x44, 0x24, 0x20, 0x00,                   // mov byte ptr [rsp+0x20],0
        0x4C, 0x8D, 0x4D, 0x99,                         // lea r9,[rbp-0x67]
        0x45, 0x33, 0xC0,                               // xor r8d,r8d
        0x49, 0x8B, 0xCC,                               // mov rcx,r12
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x49, 0xBB,                                     // mov r11, continuation; preserve rax return value
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               // jmp r11
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC
    };

    write_u64(&code[17], (uint64_t)(uintptr_t)&ootp_kbo_foreign_ai_offer_build_probe_wrapper);
    write_u64(&code[29], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_foreign_ai_offer_terms_build_probe_stub(void* continuation)
{
    uint8_t code[] = {
        0x44, 0x88, 0x74, 0x24, 0x30,                   // mov [rsp+0x30],r14b
        0x44, 0x88, 0x74, 0x24, 0x28,                   // mov [rsp+0x28],r14b
        0x44, 0x88, 0x74, 0x24, 0x20,                   // mov [rsp+0x20],r14b
        0x45, 0x33, 0xC9,                               // xor r9d,r9d
        0x48, 0x8B, 0x85, 0x40, 0x05, 0x00, 0x00,       // mov rax,[rbp+0x540]
        0x44, 0x8B, 0x80, 0x50, 0x44, 0x00, 0x00,       // mov r8d,[rax+0x4450]
        0x48, 0x8D, 0x95, 0x90, 0x01, 0x00, 0x00,       // lea rdx,[rbp+0x190]
        0x49, 0x8B, 0xCC,                               // mov rcx,r12
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x49, 0xBB,                                     // mov r11, continuation
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               // jmp r11
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[44], (uint64_t)(uintptr_t)&ootp_kbo_foreign_ai_offer_terms_build_probe_wrapper);
    write_u64(&code[56], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_foreign_ai_offer_terms_build_probe_final_create_stub(void* continuation)
{
    uint8_t code[] = {
        0xC6, 0x44, 0x24, 0x30, 0x00,                   // mov byte ptr [rsp+0x30],0
        0xC6, 0x44, 0x24, 0x28, 0x00,                   // mov byte ptr [rsp+0x28],0
        0xC6, 0x44, 0x24, 0x20, 0x00,                   // mov byte ptr [rsp+0x20],0
        0x41, 0xB1, 0x01,                               // mov r9b,1
        0x45, 0x8B, 0x85, 0x50, 0x44, 0x00, 0x00,       // mov r8d,[r13+0x4450]
        0x48, 0x8D, 0x95, 0x80, 0x01, 0x00, 0x00,       // lea rdx,[rbp+0x180]
        0x48, 0x8B, 0xCE,                               // mov rcx,rsi
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x49, 0xBB,                                     // mov r11, continuation
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               // jmp r11
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[37], (uint64_t)(uintptr_t)&ootp_kbo_foreign_ai_offer_terms_build_probe_wrapper);
    write_u64(&code[49], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_foreign_ai_offer_terms_build_probe_final_create_alt_stub(void* continuation)
{
    uint8_t code[] = {
        0xC6, 0x44, 0x24, 0x30, 0x00,                   // mov byte ptr [rsp+0x30],0
        0xC6, 0x44, 0x24, 0x28, 0x00,                   // mov byte ptr [rsp+0x28],0
        0xC6, 0x44, 0x24, 0x20, 0x00,                   // mov byte ptr [rsp+0x20],0
        0x45, 0x33, 0xC9,                               // xor r9d,r9d
        0x45, 0x8B, 0x85, 0x50, 0x44, 0x00, 0x00,       // mov r8d,[r13+0x4450]
        0x48, 0x8D, 0x55, 0x90,                         // lea rdx,[rbp-0x70]
        0x48, 0x8B, 0xCE,                               // mov rcx,rsi
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x49, 0xBB,                                     // mov r11, continuation
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               // jmp r11
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[34], (uint64_t)(uintptr_t)&ootp_kbo_foreign_ai_offer_terms_build_probe_wrapper);
    write_u64(&code[46], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_foreign_ai_offer_final_gate_probe_stub(void* success_continuation, void* failure_continuation)
{
    uint8_t code[] = {
        0x49, 0x8B, 0xD4,                               // mov rdx,r12
        0x49, 0x8B, 0xCD,                               // mov rcx,r13
        0x4C, 0x8B, 0xCB,                               // mov r9,rbx
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x84, 0xC0,                                     // test al,al
        0x75, 0x0D,                                     // jne success
        0x49, 0xBB,                                     // mov r11, failure_continuation
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               // jmp r11
        0x49, 0xBB,                                     // mov r11, success_continuation
        0,0,0,0,0,0,0,0,
        0x41, 0xFF, 0xE3,                               // jmp r11
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC
    };

    write_u64(&code[11], (uint64_t)(uintptr_t)&ootp_kbo_foreign_ai_offer_final_gate_probe_wrapper);
    write_u64(&code[27], (uint64_t)(uintptr_t)failure_continuation);
    write_u64(&code[40], (uint64_t)(uintptr_t)success_continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
