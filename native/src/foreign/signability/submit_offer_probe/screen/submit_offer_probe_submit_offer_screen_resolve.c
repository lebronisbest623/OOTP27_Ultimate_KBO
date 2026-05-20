#include "../submit_offer_probe.h"

#include <stdio.h>
#include <string.h>
uint32_t kbo_resolve_submit_offer_screen_player_id(uintptr_t screen_ptr, uint32_t today, uint32_t* out_offset)
{
    if (out_offset != NULL) {
        *out_offset = 0xffffffffu;
    }
    if (screen_ptr == 0 || !memory_range_readable((void*)screen_ptr, 0x400)) {
        return 0;
    }

    uint32_t direct_player_id = *(uint32_t*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_PLAYER_ID_OFFSET);
    uint32_t holder_team_id = 0;
    if (direct_player_id != 0u && kbo_find_active_foreign_waiver_holder(direct_player_id, today, &holder_team_id)) {
        if (out_offset != NULL) {
            *out_offset = OOTP27_FA_OFFER_SCREEN_PLAYER_ID_OFFSET;
        }
        return direct_player_id;
    }

    for (uint32_t offset = 0x20u; offset < 0x400u; offset += sizeof(uint32_t)) {
        uint32_t candidate_id = *(uint32_t*)(screen_ptr + offset);
        if (candidate_id == 0u || candidate_id > 10000000u) {
            continue;
        }
        if (kbo_find_active_foreign_waiver_holder(candidate_id, today, &holder_team_id)) {
            if (out_offset != NULL) {
                *out_offset = offset;
            }
            return candidate_id;
        }
    }
    return direct_player_id;
}

