#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#include "foreign_fa_block_state.h"
#include "../../../core/sync/lock.h"
#include "../../common/policy/foreign_player_policy.h"

volatile LONG g_kbo_foreign_offer_block_player_id = 0;
volatile LONG g_kbo_foreign_offer_block_requester_team_id = 0;
volatile LONG g_kbo_foreign_offer_block_holder_team_id = 0;
static volatile LONG g_kbo_foreign_offer_block_date = 0;
static volatile LONG64 g_kbo_foreign_offer_block_tick = 0;
volatile LONG g_kbo_custom_foreign_policy_block_player_id = 0;
volatile LONG g_kbo_custom_foreign_policy_block_requester_team_id = 0;
static volatile LONG g_kbo_custom_foreign_policy_block_date = 0;
static volatile LONG64 g_kbo_custom_foreign_policy_block_tick = 0;

typedef struct KboRecentForeignFaAllow {
    uint32_t player_id;
    uint32_t requester_team_id;
    uint32_t date_yyyymmdd;
    ULONGLONG tick;
} KboRecentForeignFaAllow;

enum {
    KBO_RECENT_FOREIGN_FA_ALLOW_MAX = 512
};

static KboRecentForeignFaAllow g_kbo_foreign_offer_allow_records[KBO_RECENT_FOREIGN_FA_ALLOW_MAX];
static KboRecentForeignFaAllow g_kbo_custom_foreign_policy_allow_records[KBO_RECENT_FOREIGN_FA_ALLOW_MAX];
static KboLock g_kbo_foreign_offer_allow_lock = KBO_LOCK_INIT;
static KboLock g_kbo_custom_foreign_policy_allow_lock = KBO_LOCK_INIT;
static int g_kbo_foreign_offer_allow_next = 0;
static int g_kbo_custom_foreign_policy_allow_next = 0;

static void kbo_recent_foreign_allow_lock(KboLock* lock)
{
    kbo_lock_enter(lock);
}

static void kbo_recent_foreign_allow_unlock(KboLock* lock)
{
    kbo_lock_leave(lock);
}

static void kbo_record_recent_foreign_allow(
    KboRecentForeignFaAllow* records,
    int* next_index,
    KboLock* lock,
    uint32_t player_id,
    uint32_t requester_team_id,
    uint32_t today)
{
    if (records == NULL || next_index == NULL || lock == NULL
            || player_id == 0u || requester_team_id == 0u || today == 0u) {
        return;
    }

    ULONGLONG now = GetTickCount64();
    kbo_recent_foreign_allow_lock(lock);

    for (int i = 0; i < KBO_RECENT_FOREIGN_FA_ALLOW_MAX; i++) {
        KboRecentForeignFaAllow* rec = &records[i];
        if (rec->player_id == player_id && rec->requester_team_id == requester_team_id) {
            rec->date_yyyymmdd = today;
            rec->tick = now;
            kbo_recent_foreign_allow_unlock(lock);
            return;
        }
    }

    int slot = *next_index;
    if (slot < 0 || slot >= KBO_RECENT_FOREIGN_FA_ALLOW_MAX) {
        slot = 0;
    }
    records[slot] = (KboRecentForeignFaAllow){
        .player_id = player_id,
        .requester_team_id = requester_team_id,
        .date_yyyymmdd = today,
        .tick = now
    };
    *next_index = (slot + 1) % KBO_RECENT_FOREIGN_FA_ALLOW_MAX;

    kbo_recent_foreign_allow_unlock(lock);
}

static int kbo_recent_foreign_allow_matches(
    KboRecentForeignFaAllow* records,
    KboLock* lock,
    uint32_t player_id,
    uint32_t today,
    uint32_t* out_requester_team_id)
{
    if (out_requester_team_id != NULL) {
        *out_requester_team_id = 0u;
    }
    if (records == NULL || lock == NULL || player_id == 0u || today == 0u) {
        return 0;
    }

    ULONGLONG best_tick = 0;
    uint32_t best_team = 0u;

    kbo_recent_foreign_allow_lock(lock);
    for (int i = 0; i < KBO_RECENT_FOREIGN_FA_ALLOW_MAX; i++) {
        KboRecentForeignFaAllow rec = records[i];
        if (rec.player_id != player_id || rec.requester_team_id == 0u || rec.date_yyyymmdd != today) {
            continue;
        }
        if (rec.tick == 0) {
            continue;
        }
        if (rec.tick >= best_tick) {
            best_tick = rec.tick;
            best_team = rec.requester_team_id;
        }
    }
    kbo_recent_foreign_allow_unlock(lock);

    if (best_team == 0u) {
        return 0;
    }
    if (out_requester_team_id != NULL) {
        *out_requester_team_id = best_team;
    }
    return 1;
}

void kbo_record_recent_foreign_offer_allow(uint32_t player_id, uint32_t requester_team_id, uint32_t today)
{
    kbo_record_recent_foreign_allow(
        g_kbo_foreign_offer_allow_records,
        &g_kbo_foreign_offer_allow_next,
        &g_kbo_foreign_offer_allow_lock,
        player_id,
        requester_team_id,
        today);
}

int kbo_recent_foreign_offer_allow_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id)
{
    return kbo_recent_foreign_allow_matches(
        g_kbo_foreign_offer_allow_records,
        &g_kbo_foreign_offer_allow_lock,
        player_id,
        today,
        out_requester_team_id);
}

void kbo_record_recent_custom_foreign_policy_allow(uint32_t player_id, uint32_t requester_team_id, uint32_t today)
{
    kbo_record_recent_foreign_allow(
        g_kbo_custom_foreign_policy_allow_records,
        &g_kbo_custom_foreign_policy_allow_next,
        &g_kbo_custom_foreign_policy_allow_lock,
        player_id,
        requester_team_id,
        today);
}

int kbo_recent_custom_foreign_policy_allow_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id)
{
    return kbo_recent_foreign_allow_matches(
        g_kbo_custom_foreign_policy_allow_records,
        &g_kbo_custom_foreign_policy_allow_lock,
        player_id,
        today,
        out_requester_team_id);
}

void kbo_record_recent_foreign_offer_block(
    uint32_t player_id,
    uint32_t requester_team_id,
    uint32_t holder_team_id,
    uint32_t today)
{
    InterlockedExchange(&g_kbo_foreign_offer_block_player_id, (LONG)player_id);
    InterlockedExchange(&g_kbo_foreign_offer_block_requester_team_id, (LONG)requester_team_id);
    InterlockedExchange(&g_kbo_foreign_offer_block_holder_team_id, (LONG)holder_team_id);
    InterlockedExchange(&g_kbo_foreign_offer_block_date, (LONG)today);
    InterlockedExchange64(&g_kbo_foreign_offer_block_tick, (LONG64)GetTickCount64());
}

int kbo_recent_foreign_offer_block_matches(
    uint32_t player_id,
    uint32_t today,
    uint32_t* out_requester_team_id,
    uint32_t* out_holder_team_id)
{
    if (player_id == 0u || today == 0u) {
        return 0;
    }

    LONG cached_player = InterlockedCompareExchange(&g_kbo_foreign_offer_block_player_id, 0, 0);
    LONG cached_date = InterlockedCompareExchange(&g_kbo_foreign_offer_block_date, 0, 0);
    if ((uint32_t)cached_player != player_id || (uint32_t)cached_date != today) {
        return 0;
    }

    LONG requester = InterlockedCompareExchange(&g_kbo_foreign_offer_block_requester_team_id, 0, 0);
    LONG holder = InterlockedCompareExchange(&g_kbo_foreign_offer_block_holder_team_id, 0, 0);
    if (requester <= 0 || holder <= 0 || requester == holder) {
        return 0;
    }
    if (out_requester_team_id != NULL) {
        *out_requester_team_id = (uint32_t)requester;
    }
    if (out_holder_team_id != NULL) {
        *out_holder_team_id = (uint32_t)holder;
    }
    return 1;
}

void kbo_record_recent_custom_foreign_policy_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today)
{
    if (player_id == 0u || requester_team_id == 0u || today == 0u) {
        return;
    }

    InterlockedExchange(&g_kbo_custom_foreign_policy_block_player_id, (LONG)player_id);
    InterlockedExchange(&g_kbo_custom_foreign_policy_block_requester_team_id, (LONG)requester_team_id);
    InterlockedExchange(&g_kbo_custom_foreign_policy_block_date, (LONG)today);
    InterlockedExchange64(&g_kbo_custom_foreign_policy_block_tick, (LONG64)GetTickCount64());
}

int kbo_recent_custom_foreign_policy_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id)
{
    if (player_id == 0u || today == 0u) {
        return 0;
    }

    LONG cached_player = InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_player_id, 0, 0);
    LONG cached_date = InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_date, 0, 0);
    if ((uint32_t)cached_player != player_id || (uint32_t)cached_date != today) {
        return 0;
    }

    LONG requester = InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_requester_team_id, 0, 0);
    if (requester <= 0) {
        return 0;
    }
    if (out_requester_team_id != NULL) {
        *out_requester_team_id = (uint32_t)requester;
    }
    return 1;
}
