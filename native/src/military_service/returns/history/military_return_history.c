#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "military_return_history.h"

typedef struct KboMilitaryReturnHistoryKey {
    uint32_t player_id;
    uint32_t history_yyyymmdd;
} KboMilitaryReturnHistoryKey;

static KboMilitaryReturnHistoryKey g_kbo_military_return_history_keys[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS];
static LONG g_kbo_military_return_history_key_count = 0;

int kbo_military_return_history_was_recorded(uint32_t player_id, uint32_t history_yyyymmdd)
{
    if (player_id == 0u || history_yyyymmdd == 0u) {
        return 0;
    }
    LONG count = g_kbo_military_return_history_key_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    for (LONG i = 0; i < count; i++) {
        KboMilitaryReturnHistoryKey* key = &g_kbo_military_return_history_keys[i];
        if (key->player_id == player_id && key->history_yyyymmdd == history_yyyymmdd) {
            return 1;
        }
    }
    return 0;
}

int kbo_mark_military_return_history_once(uint32_t player_id, uint32_t history_yyyymmdd)
{
    if (player_id == 0u || history_yyyymmdd == 0u) {
        return 0;
    }
    if (kbo_military_return_history_was_recorded(player_id, history_yyyymmdd)) {
        return 0;
    }
    LONG slot = InterlockedIncrement(&g_kbo_military_return_history_key_count) - 1;
    if (slot < 0 || slot >= OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        InterlockedDecrement(&g_kbo_military_return_history_key_count);
        return 0;
    }
    g_kbo_military_return_history_keys[slot].player_id = player_id;
    g_kbo_military_return_history_keys[slot].history_yyyymmdd = history_yyyymmdd;
    return 1;
}
