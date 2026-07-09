#include "../hotkey_window_runtime_content.h"
#include "../../hotkey_window_domain_contract.h"
#include "../../../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../../../core/sync/lock.h"

static KboLock g_kbo_hub_foreign_slot_cache_lock = KBO_LOCK_INIT;
static ULONGLONG g_kbo_hub_foreign_slot_cache_tick = 0ull;
static uint32_t g_kbo_hub_foreign_slot_cache_injured_ids[KBO_FOREIGN_INJURY_REPLACEMENT_MAX] = {0};
static uint32_t g_kbo_hub_foreign_slot_cache_replacement_ids[KBO_FOREIGN_INJURY_REPLACEMENT_MAX] = {0};
static int g_kbo_hub_foreign_slot_cache_injured_count = 0;
static int g_kbo_hub_foreign_slot_cache_replacement_count = 0;

static void kbo_hub_refresh_foreign_slot_cache(void)
{
    ULONGLONG now = GetTickCount64();
    ULONGLONG cached = g_kbo_hub_foreign_slot_cache_tick;
    if (cached != 0ull
            && now >= cached
            && now - cached < (ULONGLONG)kbo_runtime_tuning_policy()->hub_foreign_slot_cache_ttl_ms) {
        return;
    }

    kbo_lock_enter(&g_kbo_hub_foreign_slot_cache_lock);

    cached = g_kbo_hub_foreign_slot_cache_tick;
    if (cached != 0ull
            && now >= cached
            && now - cached < (ULONGLONG)kbo_runtime_tuning_policy()->hub_foreign_slot_cache_ttl_ms) {
        kbo_lock_leave(&g_kbo_hub_foreign_slot_cache_lock);
        return;
    }

    int injured_count = 0;
    int replacement_count = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements_shared();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            continue;
        }
        if (rec->injured_player_id != 0u && injured_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
            g_kbo_hub_foreign_slot_cache_injured_ids[injured_count++] = rec->injured_player_id;
        }
        if (rec->replacement_player_id != 0u
                && kbo_foreign_injury_status_uses_slot(rec->status)
                && replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
            g_kbo_hub_foreign_slot_cache_replacement_ids[replacement_count++] = rec->replacement_player_id;
        }
    }
    kbo_unlock_foreign_injury_replacements_shared();

    g_kbo_hub_foreign_slot_cache_injured_count = injured_count;
    g_kbo_hub_foreign_slot_cache_replacement_count = replacement_count;
    g_kbo_hub_foreign_slot_cache_tick = now != 0ull ? now : 1ull;
    kbo_lock_leave(&g_kbo_hub_foreign_slot_cache_lock);
}

const char* kbo_hub_foreign_slot_code_for_player(uint8_t* player)
{
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id != 0u) {
            kbo_hub_refresh_foreign_slot_cache();
            for (int i = 0; i < g_kbo_hub_foreign_slot_cache_injured_count; i++) {
                if (g_kbo_hub_foreign_slot_cache_injured_ids[i] == player_id) {
                    return "INJ";
                }
            }
            for (int i = 0; i < g_kbo_hub_foreign_slot_cache_replacement_count; i++) {
                if (g_kbo_hub_foreign_slot_cache_replacement_ids[i] == player_id) {
                    return "REPL";
                }
            }
        }
    }
    return kbo_player_is_asian_quota_candidate(player) ? "AQ" : "REG";
}

