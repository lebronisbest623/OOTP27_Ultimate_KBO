#ifndef KBOFIX_SRC_FOREIGN_RIGHTS_FOREIGN_WAIVER_RIGHTS_QUERY_H_
#define KBOFIX_SRC_FOREIGN_RIGHTS_FOREIGN_WAIVER_RIGHTS_QUERY_H_

#include <stdint.h>
#include <windows.h>

#include "../../../core/sync/lock.h"

#define KBO_FOREIGN_WAIVER_RIGHTS_MAX 1024

typedef struct KboForeignWaiverRetention {
    uint32_t player_id;
    uint32_t team_id;
    uint32_t league_id;
    uint32_t retained_on_yyyymmdd;
    uint32_t expires_on_yyyymmdd;
} KboForeignWaiverRetention;

extern KboLock g_kbo_foreign_waiver_rights_lock;
extern KboForeignWaiverRetention g_kbo_foreign_waiver_rights[KBO_FOREIGN_WAIVER_RIGHTS_MAX];
extern int g_kbo_foreign_waiver_rights_count;
extern volatile LONG g_kbo_foreign_waiver_rights_generation;

int kbo_find_active_foreign_waiver_holder(
    uint32_t player_id,
    uint32_t today_yyyymmdd,
    uint32_t* out_team_id);
void kbo_ensure_foreign_waiver_rights_loaded_for_lookup(void);
int kbo_is_foreign_waiver_right_active(const KboForeignWaiverRetention* rec, uint32_t today);
int kbo_load_foreign_waiver_rights(void);
int kbo_persist_foreign_waiver_rights(void);
void kbo_prune_expired_foreign_waiver_rights(uint32_t today_yyyymmdd);
int kbo_set_foreign_waiver_right(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t league_id,
    uint32_t retained_on,
    uint32_t expires_on);
int kbo_clear_foreign_waiver_right(uint32_t team_id, uint32_t player_id);
int kbo_consume_foreign_waiver_right_after_holder_signing(uint32_t team_id, uint32_t player_id);
int kbo_has_active_foreign_waiver_right(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t today_yyyymmdd);
int kbo_get_active_foreign_waiver_right_dates(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t today_yyyymmdd,
    uint32_t* out_retained_on,
    uint32_t* out_expires_on);
int kbo_sync_active_foreign_waiver_right_to_memory(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    uint32_t today_yyyymmdd);
void kbo_sync_active_foreign_waiver_rights_to_memory(
    const char* source,
    uint32_t today_yyyymmdd);

#endif
