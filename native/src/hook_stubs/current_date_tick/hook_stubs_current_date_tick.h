#ifndef KBOFIX_SRC_HOOK_STUBS_CURRENT_DATE_TICK_H_
#define KBOFIX_SRC_HOOK_STUBS_CURRENT_DATE_TICK_H_

#include <stdint.h>

uint8_t* build_kbo_current_date_tick_capture_stub(
    void* patch_site,
    uint32_t site_rva);
uint8_t* build_kbo_current_date_tick_capture_stub_global_r12(
    void* patch_site,
    uint32_t site_rva);
uint8_t* build_kbo_current_date_tick_capture_stub_direct_r12(
    void* patch_site,
    uint32_t site_rva);

#endif
