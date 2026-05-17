#ifndef KBOFIX_SRC_HOOK_STUBS_CURRENT_DATE_TICK_H_
#define KBOFIX_SRC_HOOK_STUBS_CURRENT_DATE_TICK_H_

#include <stddef.h>
#include <stdint.h>

uint8_t* build_kbo_current_date_tick_capture_stub(
    void* patch_site,
    uint32_t site_rva);
uint8_t* build_kbo_current_date_tick_capture_stub_global_r12(
    void* patch_site,
    uint32_t site_rva);
uint8_t* build_kbo_current_date_tick_capture_stub_global_rax(
    void* patch_site,
    uint32_t site_rva);
uint8_t* build_kbo_current_date_tick_capture_stub_live_date_rbp_0x200_global_rax(
    void* patch_site,
    uint32_t site_rva);
uint8_t* build_kbo_current_date_tick_capture_stub_direct_r9(
    void* patch_site,
    uint32_t site_rva);
uint8_t* build_kbo_current_date_tick_capture_stub_direct_r12(
    void* patch_site,
    uint32_t site_rva);
uint8_t* build_kbo_current_date_tick_date_add_trampoline(
    void* original_address,
    size_t stolen_len);
uint8_t* build_kbo_current_date_tick_date_add_detour_stub(
    void* original_trampoline,
    uint32_t site_rva);

#endif
