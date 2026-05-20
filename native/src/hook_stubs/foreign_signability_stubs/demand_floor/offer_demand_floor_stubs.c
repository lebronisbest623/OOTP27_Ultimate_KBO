#include "offer_demand_floor_stubs.h"
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

uint8_t* build_kbo_fa_offer_player_demand_floor_17a79bb_stub(uint32_t source_rva, void* continuation)
{
    uint8_t code[96] = {
        0x50,                                           // push rax
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x49, 0x8B, 0xCD,                               // mov rcx, r13 (player)
        0x49, 0x8B, 0xD6,                               // mov rdx, r14 (screen)
        0x41, 0xB8, 0,0,0,0,                            // mov r8d, source rva
        0x48, 0xB8,                                     // mov rax, probe
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x28,                         // add rsp, 0x28
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x58,                                           // pop rax
        0x66, 0x89, 0x75, 0x98,                         // mov [rbp-0x68], si
        0x49, 0x89, 0xB6, 0xD8, 0x00, 0x00, 0x00,       // mov [r14+0xd8], rsi
        0x8D, 0x4E, 0x04,                               // lea ecx, [rsi+4]
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u32(&code[23], source_rva);
    write_u64(&code[29], (uint64_t)(uintptr_t)&ootp_kbo_fa_offer_player_demand_floor_probe);
    write_u64(&code[70], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_fa_offer_player_demand_floor_17b50b4_stub(uint32_t source_rva, void* continuation)
{
    uint8_t code[112] = {
        0x50,                                           // push rax
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x49, 0x8B, 0xCE,                               // mov rcx, r14 (player)
        0x49, 0x8B, 0xD5,                               // mov rdx, r13 (screen)
        0x41, 0xB8, 0,0,0,0,                            // mov r8d, source rva
        0x48, 0xB8,                                     // mov rax, probe
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x28,                         // add rsp, 0x28
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x58,                                           // pop rax
        0x45, 0x38, 0xBE, 0xC3, 0x0D, 0x00, 0x00,       // cmp r15b, [r14+0xdc3]
        0x74, 0x0B,                                     // je continuation block
        0x41, 0xC7, 0x85, 0x1C, 0x03, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00,                         // mov [r13+0x31c], 8
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u32(&code[23], source_rva);
    write_u64(&code[29], (uint64_t)(uintptr_t)&ootp_kbo_fa_offer_player_demand_floor_probe);
    write_u64(&code[76], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_no_minor_demand_write_floor_aab739_stub(
    uint32_t source_rva,
    void* notify_call,
    void* done_target)
{
    uint8_t code[112] = {
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x48, 0x8B, 0xCB,                               // mov rcx, rbx (player)
        0x8B, 0xD0,                                     // mov edx, eax (proposed demand)
        0x41, 0xB8, 0,0,0,0,                            // mov r8d, source rva
        0x45, 0x8B, 0x8D, 0x78, 0x02, 0x00, 0x00,       // mov r9d, [r13+0x278] (salary floor hint)
        0x48, 0xB8,                                     // mov rax, probe
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x89, 0x83, 0x84, 0x0D, 0x00, 0x00,             // mov [rbx+0xd84], eax
        0x48, 0xB8,                                     // mov rax, original notify call target
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0xB8,                                     // mov rax, original done target
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u32(&code[21], source_rva);
    write_u64(&code[34], (uint64_t)(uintptr_t)&ootp_kbo_no_minor_demand_write_floor_probe);
    write_u64(&code[66], (uint64_t)(uintptr_t)notify_call);
    write_u64(&code[78], (uint64_t)(uintptr_t)done_target);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_no_minor_demand_write_floor_1077952_stub(uint32_t source_rva, void* continuation)
{
    uint8_t code[96] = {
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 0x20
        0x8B, 0xD0,                                     // mov edx, eax (proposed demand)
        0x41, 0xB8, 0,0,0,0,                            // mov r8d, source rva
        0x45, 0x33, 0xC9,                               // xor r9d, r9d
        0x48, 0xB8,                                     // mov rax, probe
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x20,                         // add rsp, 0x20
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x89, 0x81, 0x84, 0x0D, 0x00, 0x00,             // mov [rcx+0xd84], eax
        0x41, 0x8B, 0x97, 0x20, 0x01, 0x00, 0x00,       // mov edx, [r15+0x120]
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC
    };

    write_u32(&code[18], source_rva);
    write_u64(&code[27], (uint64_t)(uintptr_t)&ootp_kbo_no_minor_demand_write_floor_probe);
    write_u64(&code[66], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}

uint8_t* build_kbo_foreign_fa_demand_baseline_prepare_aab624_stub(uint32_t source_rva, void* continuation)
{
    uint8_t code[112] = {
        0x50,                                           // push rax
        0x51,                                           // push rcx
        0x52,                                           // push rdx
        0x41, 0x50,                                     // push r8
        0x41, 0x51,                                     // push r9
        0x41, 0x52,                                     // push r10
        0x41, 0x53,                                     // push r11
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x49, 0x8B, 0xCD,                               // mov rcx, r13 (financials)
        0x48, 0x8B, 0xD3,                               // mov rdx, rbx (player)
        0x41, 0xB8, 0,0,0,0,                            // mov r8d, source rva
        0x48, 0xB8,                                     // mov rax, wrapper
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x28,                         // add rsp, 0x28
        0x41, 0x5B,                                     // pop r11
        0x41, 0x5A,                                     // pop r10
        0x41, 0x59,                                     // pop r9
        0x41, 0x58,                                     // pop r8
        0x5A,                                           // pop rdx
        0x59,                                           // pop rcx
        0x58,                                           // pop rax
        0x41, 0x8B, 0xBD, 0x74, 0x03, 0x00, 0x00,       // mov edi, [r13+0x374]
        0x0F, 0xB6, 0xC0,                               // movzx eax, al
        0x66, 0x0F, 0x6E, 0xCF,                         // movd xmm1, edi
        0x48, 0xB8,                                     // mov rax, continuation
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     // jmp rax
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u32(&code[23], source_rva);
    write_u64(&code[29], (uint64_t)(uintptr_t)&ootp_kbo_foreign_fa_demand_baseline_prepare_wrapper);
    write_u64(&code[76], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
