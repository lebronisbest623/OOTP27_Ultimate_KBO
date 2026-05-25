#ifndef KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_FOREIGN_AI_STATUS_H_
#define KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_FOREIGN_AI_STATUS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_kbo_ai_fa_status_candidate_insert_stub(void* continuation);
uint8_t* build_kbo_ai_fa_status_candidate_insert_primary_stub(void* continuation);
uint8_t* build_kbo_ai_fa_status_candidate_insert_direct_stub(void* continuation);
uint8_t* build_kbo_foreign_ai_offer_candidate_priority_stub(void* success_continuation, void* skip_continuation);
uint8_t* build_kbo_foreign_ai_offer_attach_probe_detour_stub(void* original_trampoline);
uint8_t* build_kbo_foreign_ai_offer_build_probe_stub(void* continuation);
uint8_t* build_kbo_foreign_ai_offer_terms_build_probe_stub(void* continuation);
uint8_t* build_kbo_foreign_ai_offer_terms_build_probe_final_create_stub(void* continuation);
uint8_t* build_kbo_foreign_ai_offer_terms_build_probe_final_create_alt_stub(void* continuation);
uint8_t* build_kbo_foreign_ai_offer_final_gate_probe_stub(void* success_continuation, void* failure_continuation);

#endif
