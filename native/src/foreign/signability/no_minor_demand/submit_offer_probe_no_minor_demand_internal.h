#ifndef KBOFIX_SUBMIT_OFFER_PROBE_NO_MINOR_DEMAND_INTERNAL_H_
#define KBOFIX_SUBMIT_OFFER_PROBE_NO_MINOR_DEMAND_INTERNAL_H_

#include "../submit_offer_probe/submit_offer_probe.h"

#define KBO_NO_MINOR_SCAN_PREFILTER_BYTES (OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET + 1u)

int kbo_no_minor_copy_player_scan_prefix(uintptr_t player_ptr, uint8_t* out, SIZE_T out_size);
int kbo_no_minor_copy_player_scan(uintptr_t player_ptr, uint8_t* out);
int kbo_no_minor_read_player_i32(uintptr_t player_ptr, uint32_t offset, int32_t* out);
int kbo_no_minor_write_player_i32(uintptr_t player_ptr, uint32_t offset, int32_t value);
int kbo_no_minor_scan_is_teamless_demand_floor_candidate(const uint8_t* scan, uint32_t league_id);
int kbo_no_minor_scan_is_foreign_fa_candidate(const uint8_t* scan);
int kbo_no_minor_scan_should_floor_teamless_demand(const uint8_t* scan, uint32_t league_id);

#endif
