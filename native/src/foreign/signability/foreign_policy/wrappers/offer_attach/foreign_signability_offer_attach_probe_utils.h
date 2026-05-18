#ifndef KBOFIX_SRC_FOREIGN_SIGNABILITY_OFFER_ATTACH_PROBE_UTILS_H_
#define KBOFIX_SRC_FOREIGN_SIGNABILITY_OFFER_ATTACH_PROBE_UTILS_H_

#include "../../internal/foreign_signability_internal.h"

#define KBO_OFFER_READABLE_BYTES 0xE0u
#define KBO_OFFER_MAJOR_FLAG_OFFSET 0x08u
#define KBO_OFFER_MINOR_FLAG_OFFSET 0x0Au
#define KBO_OFFER_SALARY_PRIMARY_OFFSET 0x24u
#define KBO_OFFER_TEAM_ID_OFFSET 0x28u
#define KBO_OFFER_TEAM_ORG_ID_OFFSET 0x30u
#define KBO_OFFER_SALARY_FIRST_YEAR_OFFSET 0x38u
#define KBO_OFFER_YEAR_COUNT_OFFSET 0x74u
#define KBO_OFFER_FLAG_AA_OFFSET 0xAAu
#define KBO_OFFER_FLAG_AB_OFFSET 0xABu
#define KBO_OFFER_FLAG_AC_OFFSET 0xACu
#define KBO_OFFER_TYPE_AE_OFFSET 0xAEu
#define KBO_OFFER_TYPE_B0_OFFSET 0xB0u
#define KBO_OFFER_FLAG_D0_OFFSET 0xD0u
#define KBO_OFFER_FLAG_D8_OFFSET 0xD8u
#define KBO_PLAYER_SELECTED_OFFER_ID_OFFSET 0x1750u

uint32_t kbo_foreign_ai_offer_attach_caller_rva(uintptr_t caller_return_ptr);
uint8_t kbo_offer_read_u8(uintptr_t offer_ptr, uint32_t offset);
uint16_t kbo_offer_read_u16(uintptr_t offer_ptr, uint32_t offset);
int32_t kbo_offer_read_i32(uintptr_t offer_ptr, uint32_t offset);
int kbo_offer_write_u8(uintptr_t offer_ptr, uint32_t offset, uint8_t value);
int kbo_offer_write_i32(uintptr_t offer_ptr, uint32_t offset, int32_t value);
uint32_t kbo_offer_probe_team_id_from_ptr(uintptr_t team_ptr);
void* kbo_offer_probe_resolve_rva(uint32_t rva);

#endif
