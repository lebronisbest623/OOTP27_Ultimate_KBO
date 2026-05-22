#ifndef KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_INTL_ESTABLISHED_FA_H_
#define KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_INTL_ESTABLISHED_FA_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_kbo_intl_established_fa_count_stub(void* continuation, void* progress_title);
uint8_t* build_kbo_intl_established_fa_register_gate_stub(
    void* continuation,
    void* retry_continuation,
    void* global_database_slot,
    void* register_player_func);

#endif
