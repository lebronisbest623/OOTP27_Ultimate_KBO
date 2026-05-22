#include "hook_stubs_intl_established_fa.h"
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

uint8_t* build_kbo_intl_established_fa_count_stub(void* continuation, void* progress_title)
{
    uint8_t code[96] = {
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x8B, 0xCF,                                     // mov ecx, edi
        0x49, 0x8B, 0xD4,                               // mov rdx, r12
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0x8B, 0xF8,                                     // mov edi, eax
        0x89, 0x7D, 0x98,                               // mov [rbp-0x68], edi
        0x45, 0x33, 0xC9,                               // xor r9d, r9d
        0x49, 0xB8,                                     // mov r8, progress_title
        0,0,0,0,0,0,0,0,
        0x41, 0x8D, 0x51, 0x01,                         // lea edx, [r9+1]
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[11], (uint64_t)(uintptr_t)&ootp_kbo_intl_established_fa_count_wrapper);
    write_u64(&code[35], (uint64_t)(uintptr_t)progress_title);
    write_u64(&code[49], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
uint8_t* build_kbo_intl_established_fa_register_gate_stub(
    void* continuation,
    void* retry_continuation,
    void* global_database_slot,
    void* register_player_func)
{
    uint8_t code[160] = {
        0x50,                                           // push rax
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp,0x28
        0x48, 0x8B, 0xCB,                               // mov rcx,rbx
        0x49, 0x8B, 0xD4,                               // mov rdx,r12
        0x48, 0xB8,                                     // mov rax,wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x84, 0xC0,                                     // test al,al
        0x74, 0x37,                                     // je retry
        0x48, 0x83, 0xC4, 0x28,                         // add rsp,0x28
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x58,                                           // pop rax
        0x48, 0x8B, 0xD3,                               // mov rdx,rbx
        0x48, 0xB8,                                     // mov rax,global_database_slot
        0,0,0,0,0,0,0,0,
        0x48, 0x8B, 0x08,                               // mov rcx,[rax]
        0x48, 0xB8,                                     // mov rax,register_player_func
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0xB8,                                     // mov rax,continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0x48, 0x83, 0xC4, 0x28,                         // add rsp,0x28
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x58,                                           // pop rax
        0xBF, 0x00, 0x00, 0x00, 0x80,                   // mov edi,0x80000000
        0x48, 0xB8,                                     // mov rax,retry_continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u64(&code[23], (uint64_t)(uintptr_t)&ootp_kbo_intl_established_fa_generation_filter_allows_wrapper);
    write_u64(&code[57], (uint64_t)(uintptr_t)global_database_slot);
    write_u64(&code[70], (uint64_t)(uintptr_t)register_player_func);
    write_u64(&code[82], (uint64_t)(uintptr_t)continuation);
    write_u64(&code[114], (uint64_t)(uintptr_t)retry_continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
