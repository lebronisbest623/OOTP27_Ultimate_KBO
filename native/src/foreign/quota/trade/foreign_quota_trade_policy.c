#include "../internal/foreign_quota_internal.h"

#define KBO_CUSTOM_FOREIGN_TRADE_TEAM_ID_OFFSET       0x08u
#define KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET     0x10u
#define KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT           2
#define KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS         10
#define KBO_CUSTOM_FOREIGN_TRADE_READABLE_BYTES       (KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET + (KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT * KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS * sizeof(uint32_t)))
#define KBO_CUSTOM_FOREIGN_TRADE_CACHE_SIZE           128
#define KBO_CUSTOM_FOREIGN_TRADE_CACHE_TTL_MS         1000u

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

uint32_t kbo_custom_foreign_trade_team_id(uintptr_t trade_ptr, int side)
{
    return *(uint32_t*)(trade_ptr + KBO_CUSTOM_FOREIGN_TRADE_TEAM_ID_OFFSET + ((uintptr_t)side * sizeof(uint32_t)));
}

uint32_t kbo_custom_foreign_trade_player_id(uintptr_t trade_ptr, int side, int slot)
{
    uintptr_t offset = KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET
        + (((uintptr_t)side * KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS + (uintptr_t)slot) * sizeof(uint32_t));
    return *(uint32_t*)(trade_ptr + offset);
}

int kbo_custom_foreign_policy_team_in_trade_scope(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id != 0u) {
        return team_league_id == kbo_league_id;
    }
    return team_league_id == OOTP27_KBO_MAIN_LEAGUE_ID;
}

int kbo_custom_foreign_policy_trade_countable_player(
    uint32_t player_id,
    uint8_t** out_player,
    int* out_asian_quota)
{
    if (out_player != NULL) { *out_player = NULL; }
    if (out_asian_quota != NULL) { *out_asian_quota = 0; }
    if (player_id == 0u) {
        return 0;
    }

    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    if (player == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }

    uint8_t replacement_slot_type = 0u;
    if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
        return 0;
    }

    if (out_player != NULL) { *out_player = player; }
    if (out_asian_quota != NULL) { *out_asian_quota = kbo_player_is_asian_quota_candidate(player); }
    return 1;
}

void kbo_custom_foreign_policy_trade_adjust_counts_for_player(
    uint32_t team_id,
    uint32_t player_id,
    int incoming,
    uint32_t* asian_count,
    uint32_t* non_asian_count,
    uint32_t* incoming_foreign_count,
    uint32_t* first_incoming_player_id)
{
    uint8_t* player = NULL;
    int asian_quota = 0;
    if (!kbo_custom_foreign_policy_trade_countable_player(player_id, &player, &asian_quota)) {
        return;
    }

    int already_in_org = kbo_player_current_assignment_matches_team_or_affiliate(player, team_id);
    if (incoming) {
        if (already_in_org) {
            return;
        }
        if (asian_quota) {
            (*asian_count)++;
        } else {
            (*non_asian_count)++;
        }
        if (incoming_foreign_count != NULL) {
            (*incoming_foreign_count)++;
        }
        if (first_incoming_player_id != NULL && *first_incoming_player_id == 0u) {
            *first_incoming_player_id = player_id;
        }
        return;
    }

    if (!already_in_org) {
        return;
    }
    if (asian_quota) {
        if (*asian_count > 0u) {
            (*asian_count)--;
        }
    } else if (*non_asian_count > 0u) {
        (*non_asian_count)--;
    }
}

uint32_t kbo_custom_foreign_policy_trade_extra_slots(
    uintptr_t trade_ptr,
    int incoming_side,
    uint32_t team_id)
{
    int regular_seen = 0;
    int asian_seen = 0;
    uint32_t extra_slots = 0u;

    for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
        uint32_t player_id = kbo_custom_foreign_trade_player_id(trade_ptr, incoming_side, slot);
        uint8_t* player = NULL;
        int asian_quota = 0;
        if (!kbo_custom_foreign_policy_trade_countable_player(player_id, &player, &asian_quota)
                || kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
            continue;
        }

        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        if (!kbo_custom_foreign_policy_extra_slots_for_candidate(team_id, player, &slot_type, &injured_player_id)) {
            continue;
        }
        (void)injured_player_id;
        if (slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA) {
            if (!asian_seen) {
                asian_seen = 1;
                extra_slots++;
            }
        } else if (slot_type == KBO_FOREIGN_INJURY_SLOT_REGULAR && !regular_seen) {
            regular_seen = 1;
            extra_slots++;
        }
        (void)asian_quota;
    }

    return extra_slots;
}

static uint32_t kbo_custom_foreign_trade_policy_hash_mix(uint32_t h, uint32_t value)
{
    h ^= value + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    return h;
}

static void kbo_custom_foreign_trade_policy_read_player_ids(
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

static uint32_t kbo_custom_foreign_trade_policy_player_hash(
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

static int kbo_custom_foreign_trade_policy_cache_hit(
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

static void kbo_custom_foreign_trade_policy_cache_store(
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

static int kbo_custom_foreign_policy_trade_allows_uncached(
    uintptr_t trade_ptr,
    int32_t requested_side,
    int* out_blocked_side,
    uint32_t* out_team_id,
    uint32_t* out_incoming_player_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit)
{
    if (out_blocked_side != NULL) { *out_blocked_side = -1; }
    if (out_team_id != NULL) { *out_team_id = 0u; }
    if (out_incoming_player_id != NULL) { *out_incoming_player_id = 0u; }
    if (out_effective_before != NULL) { *out_effective_before = 0u; }
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT; }
    (void)requested_side;

    if (trade_ptr == 0
            || !memory_range_readable((void*)trade_ptr, KBO_CUSTOM_FOREIGN_TRADE_READABLE_BYTES)) {
        return 1;
    }

    uint32_t team_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT] = {
        kbo_custom_foreign_trade_team_id(trade_ptr, 0),
        kbo_custom_foreign_trade_team_id(trade_ptr, 1)
    };

    for (int side = 0; side < KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT; side++) {
        uint32_t team_id = team_ids[side];
        if (!kbo_custom_foreign_policy_team_in_trade_scope(team_id)) {
            continue;
        }

        uint32_t foreign_count = 0u;
        uint32_t asian_count = 0u;
        uint32_t non_asian_count = 0u;
        kbo_count_team_asian_quota_probe_fresh(team_id, &foreign_count, &asian_count, &non_asian_count);
        (void)foreign_count;

        uint32_t effective_before = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
        uint32_t asian_after = asian_count;
        uint32_t non_asian_after = non_asian_count;
        uint32_t incoming_foreign_count = 0u;
        uint32_t first_incoming_player_id = 0u;
        int incoming_side = 1 - side;

        for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
            kbo_custom_foreign_policy_trade_adjust_counts_for_player(
                team_id,
                kbo_custom_foreign_trade_player_id(trade_ptr, side, slot),
                0,
                &asian_after,
                &non_asian_after,
                NULL,
                NULL);
        }
        for (int slot = 0; slot < KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS; slot++) {
            kbo_custom_foreign_policy_trade_adjust_counts_for_player(
                team_id,
                kbo_custom_foreign_trade_player_id(trade_ptr, incoming_side, slot),
                1,
                &asian_after,
                &non_asian_after,
                &incoming_foreign_count,
                &first_incoming_player_id);
        }

        uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT
            + kbo_custom_foreign_policy_trade_extra_slots(trade_ptr, incoming_side, team_id);
        if (incoming_foreign_count > 0u && effective_after > effective_limit) {
            if (out_blocked_side != NULL) { *out_blocked_side = side; }
            if (out_team_id != NULL) { *out_team_id = team_id; }
            if (out_incoming_player_id != NULL) { *out_incoming_player_id = first_incoming_player_id; }
            if (out_effective_before != NULL) { *out_effective_before = effective_before; }
            if (out_effective_after != NULL) { *out_effective_after = effective_after; }
            if (out_effective_limit != NULL) { *out_effective_limit = effective_limit; }
            return 0;
        }
    }

    return 1;
}

int kbo_custom_foreign_policy_trade_allows(
    uintptr_t trade_ptr,
    int32_t requested_side,
    int* out_blocked_side,
    uint32_t* out_team_id,
    uint32_t* out_incoming_player_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit)
{
    if (out_blocked_side != NULL) { *out_blocked_side = -1; }
    if (out_team_id != NULL) { *out_team_id = 0u; }
    if (out_incoming_player_id != NULL) { *out_incoming_player_id = 0u; }
    if (out_effective_before != NULL) { *out_effective_before = 0u; }
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT; }

    if (trade_ptr == 0
            || !memory_range_readable((void*)trade_ptr, KBO_CUSTOM_FOREIGN_TRADE_READABLE_BYTES)) {
        return 1;
    }

    uint32_t team_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT] = {
        kbo_custom_foreign_trade_team_id(trade_ptr, 0),
        kbo_custom_foreign_trade_team_id(trade_ptr, 1)
    };
    uint32_t player_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS];
    kbo_custom_foreign_trade_policy_read_player_ids(trade_ptr, player_ids);
    uint32_t player_hash = kbo_custom_foreign_trade_policy_player_hash(team_ids, player_ids);
    uint32_t org_generations[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT] = {
        kbo_foreign_org_count_cache_generation_for_team(team_ids[0]),
        kbo_foreign_org_count_cache_generation_for_team(team_ids[1])
    };
    int replacement_count = g_kbo_foreign_injury_replacement_count;

    int allowed = 1;
    if (kbo_custom_foreign_trade_policy_cache_hit(
            trade_ptr,
            requested_side,
            team_ids,
            player_ids,
            player_hash,
            org_generations,
            replacement_count,
            out_blocked_side,
            out_team_id,
            out_incoming_player_id,
            out_effective_before,
            out_effective_after,
            out_effective_limit,
            &allowed)) {
        kbo_profiler_record_us("foreign_policy.trade.cache_hit", 0);
        return allowed;
    }

    int blocked_side = -1;
    uint32_t blocked_team_id = 0u;
    uint32_t incoming_player_id = 0u;
    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    allowed = kbo_custom_foreign_policy_trade_allows_uncached(
        trade_ptr,
        requested_side,
        &blocked_side,
        &blocked_team_id,
        &incoming_player_id,
        &effective_before,
        &effective_after,
        &effective_limit);

    kbo_custom_foreign_trade_policy_cache_store(
        trade_ptr,
        requested_side,
        team_ids,
        player_ids,
        player_hash,
        org_generations,
        replacement_count,
        allowed,
        blocked_side,
        blocked_team_id,
        incoming_player_id,
        effective_before,
        effective_after,
        effective_limit);
    if (out_blocked_side != NULL) { *out_blocked_side = blocked_side; }
    if (out_team_id != NULL) { *out_team_id = blocked_team_id; }
    if (out_incoming_player_id != NULL) { *out_incoming_player_id = incoming_player_id; }
    if (out_effective_before != NULL) { *out_effective_before = effective_before; }
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    if (out_effective_limit != NULL) { *out_effective_limit = effective_limit; }
    return allowed;
}

