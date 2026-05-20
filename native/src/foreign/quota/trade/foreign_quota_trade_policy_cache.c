#include "foreign_quota_trade_policy_cache.h"

enum {
    KBO_CUSTOM_FOREIGN_TRADE_CACHE_SIZE = 128,
    KBO_CUSTOM_FOREIGN_TRADE_CACHE_TTL_MS = 1000u
};

typedef struct KboCustomForeignTradePolicyCacheEntry {
    uintptr_t trade_ptr;
    int32_t requested_side;
    uint32_t team_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT];
    uint32_t player_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS];
    uint32_t player_hash;
    uint32_t org_generations[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT];
    int replacement_count;
    DWORD tick;
    int allowed;
    int blocked_side;
    uint32_t team_id;
    uint32_t incoming_player_id;
    uint32_t effective_before;
    uint32_t effective_after;
    uint32_t effective_limit;
    uint8_t valid;
} KboCustomForeignTradePolicyCacheEntry;

static KboCustomForeignTradePolicyCacheEntry
    g_kbo_custom_foreign_trade_policy_cache[KBO_CUSTOM_FOREIGN_TRADE_CACHE_SIZE];

static uint32_t kbo_custom_foreign_trade_policy_hash_mix(uint32_t h, uint32_t value)
{
    h ^= value + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    return h;
}

void kbo_custom_foreign_trade_policy_read_player_ids(
    uintptr_t trade_ptr,
    uint32_t player_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS])
{
    if (player_ids == NULL) {
        return;
    }
    for (int side = 0; side < KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT; side++) {
        for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
            player_ids[side][slot] = kbo_custom_foreign_trade_player_id(trade_ptr, side, slot);
        }
    }
}

uint32_t kbo_custom_foreign_trade_policy_player_hash(
    const uint32_t* team_ids,
    const uint32_t player_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS])
{
    uint32_t h = 2166136261u;
    if (team_ids != NULL) {
        for (int side = 0; side < KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT; side++) {
            h = kbo_custom_foreign_trade_policy_hash_mix(h, team_ids[side]);
        }
    }
    if (player_ids != NULL) {
        for (int side = 0; side < KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT; side++) {
            for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
                h = kbo_custom_foreign_trade_policy_hash_mix(h, player_ids[side][slot]);
            }
        }
    }
    return h;
}

static int kbo_custom_foreign_trade_policy_player_ids_match(
    const uint32_t left[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS],
    const uint32_t right[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS])
{
    if (left == NULL || right == NULL) {
        return 0;
    }
    for (int side = 0; side < KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT; side++) {
        for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
            if (left[side][slot] != right[side][slot]) {
                return 0;
            }
        }
    }
    return 1;
}

static uint32_t kbo_custom_foreign_trade_policy_cache_slot(
    uintptr_t trade_ptr,
    int32_t requested_side,
    uint32_t player_hash)
{
    uint32_t h = (uint32_t)(trade_ptr >> 4) ^ (uint32_t)trade_ptr;
    h = kbo_custom_foreign_trade_policy_hash_mix(h, (uint32_t)requested_side);
    h = kbo_custom_foreign_trade_policy_hash_mix(h, player_hash);
    return h & (KBO_CUSTOM_FOREIGN_TRADE_CACHE_SIZE - 1u);
}

static void kbo_custom_foreign_trade_policy_cache_fill_outputs(
    const KboCustomForeignTradePolicyCacheEntry* entry,
    int* out_blocked_side,
    uint32_t* out_team_id,
    uint32_t* out_incoming_player_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit)
{
    if (entry == NULL) {
        return;
    }
    if (out_blocked_side != NULL) { *out_blocked_side = entry->blocked_side; }
    if (out_team_id != NULL) { *out_team_id = entry->team_id; }
    if (out_incoming_player_id != NULL) { *out_incoming_player_id = entry->incoming_player_id; }
    if (out_effective_before != NULL) { *out_effective_before = entry->effective_before; }
    if (out_effective_after != NULL) { *out_effective_after = entry->effective_after; }
    if (out_effective_limit != NULL) { *out_effective_limit = entry->effective_limit; }
}

int kbo_custom_foreign_trade_policy_cache_hit(
    uintptr_t trade_ptr,
    int32_t requested_side,
    const uint32_t* team_ids,
    const uint32_t player_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS],
    uint32_t player_hash,
    const uint32_t* org_generations,
    int replacement_count,
    int* out_blocked_side,
    uint32_t* out_team_id,
    uint32_t* out_incoming_player_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    int* out_allowed)
{
    if (team_ids == NULL || player_ids == NULL || org_generations == NULL || out_allowed == NULL) {
        return 0;
    }
    DWORD now = GetTickCount();
    uint32_t slot = kbo_custom_foreign_trade_policy_cache_slot(
        trade_ptr,
        requested_side,
        player_hash);
    KboCustomForeignTradePolicyCacheEntry entry =
        g_kbo_custom_foreign_trade_policy_cache[slot];
    if (!entry.valid
            || entry.trade_ptr != trade_ptr
            || entry.requested_side != requested_side
            || entry.player_hash != player_hash
            || entry.team_ids[0] != team_ids[0]
            || entry.team_ids[1] != team_ids[1]
            || !kbo_custom_foreign_trade_policy_player_ids_match(entry.player_ids, player_ids)
            || entry.org_generations[0] != org_generations[0]
            || entry.org_generations[1] != org_generations[1]
            || entry.replacement_count != replacement_count
            || entry.tick == 0u
            || now - entry.tick > KBO_CUSTOM_FOREIGN_TRADE_CACHE_TTL_MS) {
        return 0;
    }

    kbo_custom_foreign_trade_policy_cache_fill_outputs(
        &entry,
        out_blocked_side,
        out_team_id,
        out_incoming_player_id,
        out_effective_before,
        out_effective_after,
        out_effective_limit);
    *out_allowed = entry.allowed;
    return 1;
}

void kbo_custom_foreign_trade_policy_cache_store(
    uintptr_t trade_ptr,
    int32_t requested_side,
    const uint32_t* team_ids,
    const uint32_t player_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS],
    uint32_t player_hash,
    const uint32_t* org_generations,
    int replacement_count,
    int allowed,
    int blocked_side,
    uint32_t team_id,
    uint32_t incoming_player_id,
    uint32_t effective_before,
    uint32_t effective_after,
    uint32_t effective_limit)
{
    if (team_ids == NULL || player_ids == NULL || org_generations == NULL) {
        return;
    }
    uint32_t slot = kbo_custom_foreign_trade_policy_cache_slot(
        trade_ptr,
        requested_side,
        player_hash);
    KboCustomForeignTradePolicyCacheEntry* entry =
        &g_kbo_custom_foreign_trade_policy_cache[slot];
    entry->valid = 0u;
    entry->trade_ptr = trade_ptr;
    entry->requested_side = requested_side;
    entry->team_ids[0] = team_ids[0];
    entry->team_ids[1] = team_ids[1];
    for (int side = 0; side < KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT; side++) {
        for (int player_slot = 0; player_slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; player_slot++) {
            entry->player_ids[side][player_slot] = player_ids[side][player_slot];
        }
    }
    entry->player_hash = player_hash;
    entry->org_generations[0] = org_generations[0];
    entry->org_generations[1] = org_generations[1];
    entry->replacement_count = replacement_count;
    entry->allowed = allowed;
    entry->blocked_side = blocked_side;
    entry->team_id = team_id;
    entry->incoming_player_id = incoming_player_id;
    entry->effective_before = effective_before;
    entry->effective_after = effective_after;
    entry->effective_limit = effective_limit;
    entry->tick = GetTickCount();
    entry->valid = 1u;
}
