#include "foreign_signability_reserve_log.h"
#include "../internal/foreign_signability_internal.h"

typedef struct KboForeignReserveSignabilityPlayerContext {
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t default_team_id;
    uint32_t current_league_id;
} KboForeignReserveSignabilityPlayerContext;

static KboForeignReserveSignabilityPlayerContext
kbo_foreign_reserve_signability_player_context(uint8_t* player)
{
    KboForeignReserveSignabilityPlayerContext context = {0};
    if (memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        context.current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        context.active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        context.original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
        context.current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
            context.default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
        }
    }
    return context;
}

void kbo_log_foreign_reserve_holder_visible_generic_signability(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    int original_signability,
    int adjusted,
    uint32_t today,
    uintptr_t caller_rva,
    int32_t score,
    int32_t threshold)
{
    static LONG generic_visible_log_count = 0;
    LONG visible_slot = InterlockedIncrement(&generic_visible_log_count);
    if (visible_slot > 200) {
        return;
    }

    KboForeignReserveSignabilityPlayerContext context =
        kbo_foreign_reserve_signability_player_context(player);
    kbo_log_runtimef(
        "foreign reserve signability: holder-visible generic request player=%u holder_team=%u original=%d adjusted=%d today=%u caller_rva=0x%llx current=%u active=%u original_team=%u default_team=%u league=%u score=%d threshold=%d",
        player_id,
        holder_team_id,
        original_signability,
        adjusted,
        today,
        (unsigned long long)caller_rva,
        context.current_team_id,
        context.active_team_id,
        context.original_team_id,
        context.default_team_id,
        context.current_league_id,
        score,
        threshold);
}

void kbo_log_foreign_reserve_blocked_generic_signability(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    int original_signability,
    uint32_t today,
    uintptr_t caller_rva,
    int32_t score,
    int32_t threshold)
{
    static LONG generic_block_log_count = 0;
    LONG slot = InterlockedIncrement(&generic_block_log_count);
    if (slot > 200) {
        return;
    }

    KboForeignReserveSignabilityPlayerContext context =
        kbo_foreign_reserve_signability_player_context(player);
    kbo_log_runtimef(
        "foreign reserve signability: blocked generic request player=%u holder_team=%u original=%d forced=0 today=%u caller_rva=0x%llx current=%u active=%u original_team=%u default_team=%u league=%u score=%d threshold=%d",
        player_id,
        holder_team_id,
        original_signability,
        today,
        (unsigned long long)caller_rva,
        context.current_team_id,
        context.active_team_id,
        context.original_team_id,
        context.default_team_id,
        context.current_league_id,
        score,
        threshold);
}

void kbo_log_foreign_reserve_blocked_display_signability(
    uint8_t* player,
    uint32_t player_id,
    uint32_t team_id,
    uint32_t holder_team_id,
    int original_signability,
    uint32_t today,
    uintptr_t caller_rva)
{
    static LONG display_block_log_count = 0;
    LONG display_slot = InterlockedIncrement(&display_block_log_count);
    if (display_slot > 120) {
        return;
    }

    KboForeignReserveSignabilityPlayerContext context =
        kbo_foreign_reserve_signability_player_context(player);
    kbo_log_runtimef(
        "foreign reserve signability: blocked display request player=%u requester_team=%u holder_team=%u original=%d today=%u caller_rva=0x%llx current=%u active=%u original_team=%u default_team=%u league=%u score=%d",
        player_id,
        team_id,
        holder_team_id,
        original_signability,
        today,
        (unsigned long long)caller_rva,
        context.current_team_id,
        context.active_team_id,
        context.original_team_id,
        context.default_team_id,
        context.current_league_id,
        kbo_foreign_waiver_value_score(player));
}
