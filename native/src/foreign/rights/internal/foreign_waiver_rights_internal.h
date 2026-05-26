#ifndef NATIVE_SRC_FOREIGN_RIGHTS_FOREIGN_WAIVER_RIGHTS_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_RIGHTS_FOREIGN_WAIVER_RIGHTS_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../common/paths/foreign_waiver_paths.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../query/foreign_waiver_rights_query.h"


int kbo_is_foreign_waiver_right_active(const KboForeignWaiverRetention* rec, uint32_t today);
int kbo_is_foreign_waiver_right_expired(const KboForeignWaiverRetention* rec, uint32_t today);
int kbo_persist_foreign_waiver_rights(void);
int kbo_load_foreign_waiver_rights(void);
void kbo_prune_expired_foreign_waiver_rights(uint32_t today_yyyymmdd);
int kbo_set_foreign_waiver_right(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t league_id,
    uint32_t retained_on,
    uint32_t expires_on);
int kbo_clear_foreign_waiver_right(uint32_t team_id, uint32_t player_id);
int kbo_consume_foreign_waiver_right_after_holder_signing(uint32_t team_id, uint32_t player_id);
void kbo_ensure_foreign_waiver_rights_loaded_for_lookup(void);
int kbo_has_active_foreign_waiver_right(uint32_t team_id, uint32_t player_id, uint32_t today_yyyymmdd);
int kbo_get_active_foreign_waiver_right_dates(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t today_yyyymmdd,
    uint32_t* out_retained_on,
    uint32_t* out_expires_on);
int kbo_find_active_foreign_waiver_holder(uint32_t player_id, uint32_t today_yyyymmdd, uint32_t* out_team_id);
int kbo_sync_active_foreign_waiver_right_to_memory(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    uint32_t today_yyyymmdd);
void kbo_sync_active_foreign_waiver_rights_to_memory(
    const char* source,
    uint32_t today_yyyymmdd);

#endif
