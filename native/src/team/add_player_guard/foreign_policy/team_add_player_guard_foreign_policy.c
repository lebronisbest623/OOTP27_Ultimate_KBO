#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/injury/api/foreign_injury.h"
#include "../../../foreign/rights/query/foreign_waiver_rights_query.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../assignment/org_query/team_org_assignment_query.h"
#include "../../assignment/roster_arrays/team_roster_arrays.h"
#include "../../lookup/team_lookup.h"
#include "../internal/team_add_player_guard_internal.h"

static int kbo_team_add_target_is_kbo_affiliate_league(
    uint8_t* team,
    uint32_t kbo_league_id,
    uint32_t* out_parent_team_id)
{
    if (out_parent_team_id != NULL) { *out_parent_team_id = 0u; }
    if (team == NULL
            || kbo_league_id == 0u
            || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_league_id == 0u || team_league_id == kbo_league_id) {
        return 0;
    }

    uint32_t parent_team_id = 0u;
    if (memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    }
    if (out_parent_team_id != NULL) { *out_parent_team_id = parent_team_id; }
    if (parent_team_id == 0u) {
        return 0;
    }

    uint8_t* parent_team = find_kbo_team_by_numeric_id_any_league(parent_team_id, 1);
    if (parent_team == NULL || !memory_range_readable(parent_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    return *(uint32_t*)(parent_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) == kbo_league_id;
}

int kbo_team_add_known_foreign_market_minor_caller(uint32_t caller_rva)
{
    return caller_rva == 0x8515f3u
        || caller_rva == 0xaa638du
        || caller_rva == 0xa4a0cbu;
}

static uint32_t kbo_team_add_kbo_org_team_id(uint32_t team_id, uint32_t kbo_league_id)
{
    if (team_id == 0u || kbo_league_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_league_id == kbo_league_id) {
        return team_id;
    }

    uint32_t parent_team_id = 0u;
    if (memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    }
    if (parent_team_id == 0u) {
        return 0u;
    }

    uint8_t* parent_team = find_kbo_team_by_numeric_id_any_league(parent_team_id, 1);
    if (parent_team == NULL || !memory_range_readable(parent_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    return *(uint32_t*)(parent_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) == kbo_league_id
        ? parent_team_id
        : 0u;
}

int kbo_team_add_foreign_policy_should_block(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t team_id,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id,
    uint32_t caller_rva)
{
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || team_id == 0u
            || team_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || player_ptr == 0
            || !memory_range_readable(
                (void*)player_ptr,
                OOTP27_PLAYER_NATION_ID_OFFSET + sizeof(uint32_t))) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    if (kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)((uint8_t*)team_ptr + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    uint32_t parent_team_id = 0u;
    int non_kbo_league = team_league_id != 0u && kbo_league_id != 0u && team_league_id != kbo_league_id;
    int kbo_affiliate_league = kbo_team_add_target_is_kbo_affiliate_league(
        (uint8_t*)team_ptr,
        kbo_league_id,
        &parent_team_id);
    if (before_current_team_id == 0u
            && before_active_team_id == 0u
            && non_kbo_league
            && (kbo_affiliate_league || kbo_team_add_known_foreign_market_minor_caller(caller_rva))) {
        static volatile LONG minor_market_block_log_count = 0;
        LONG minor_slot = InterlockedIncrement(&minor_market_block_log_count);
        if (minor_slot <= 200) {
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            kbo_log_runtimef(
                "custom foreign policy minor-league market signing blocked player=%u team=%u parent_team=%u team_league=%u kbo_league=%u before_current=%u before_active=%u before_original=%u contract_level=%u affiliate=%d market_caller=%d caller_rva=0x%x",
                player_id,
                team_id,
                parent_team_id,
                team_league_id,
                kbo_league_id,
                before_current_team_id,
                before_active_team_id,
                before_original_team_id,
                (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
                kbo_affiliate_league,
                kbo_team_add_known_foreign_market_minor_caller(caller_rva),
                caller_rva);
        }
        return 1;
    }

    uint32_t today = 0u;
    if (kbo_current_date_tick_latest_published_date(&today) && today != 0u) {
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t holder_team_id = 0u;
        if (kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
                && holder_team_id != 0u) {
            uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            uint32_t original_team_id = 0u;
            if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
                original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
            }
            int score = kbo_foreign_waiver_value_score(player);

            if (holder_team_id == team_id) {
                uint32_t effective_before = 0u;
                uint32_t effective_after = 0u;
                uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
                uint8_t slot_type = 0u;
                uint32_t injured_player_id = 0u;
                int quota_allowed = kbo_custom_foreign_policy_team_allows_final_signing(
                    team_id,
                    player,
                    &effective_before,
                    &effective_after,
                    &effective_limit,
                    &slot_type,
                    &injured_player_id);

                static volatile LONG reserve_holder_allow_log_count = 0;
                LONG allow_slot = InterlockedIncrement(&reserve_holder_allow_log_count);
                if (allow_slot <= 200) {
                    kbo_log_runtimef(
                        "foreign retention re-signing: holder_quota_check team=%u player=%u today=%u before_current=%u before_active=%u current=%u active=%u original=%u score=%d quota_allowed=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u caller_rva=0x%x",
                        team_id,
                        player_id,
                        today,
                        before_current_team_id,
                        before_active_team_id,
                        current_team_id,
                        active_team_id,
                        original_team_id,
                        score,
                        quota_allowed,
                        effective_before,
                        effective_after,
                        effective_limit,
                        slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                        injured_player_id,
                        caller_rva);
                }
                if (quota_allowed) {
                    return 0;
                }
                return 1;
            }

            static volatile LONG reserve_block_log_count = 0;
            LONG reserve_slot = InterlockedIncrement(&reserve_block_log_count);
            if (reserve_slot <= 200) {
                kbo_log_runtimef(
                    "foreign retention re-signing: blocked_non_holder_team_add team=%u holder_team=%u player=%u today=%u before_current=%u before_active=%u current=%u active=%u original=%u score=%d caller_rva=0x%x",
                    team_id,
                    holder_team_id,
                    player_id,
                    today,
                    before_current_team_id,
                    before_active_team_id,
                    current_team_id,
                    active_team_id,
                    original_team_id,
                    score,
                    caller_rva);
            }
            return 1;
        }
    }

    if (before_current_team_id == 0u
            && before_active_team_id == 0u
            && before_original_team_id != 0u
            && team_league_id == kbo_league_id
            && !read_kbo_localappdata_flag_file("disable_kbo_foreign_former_org_market_block.txt")) {
        uint32_t source_org_team_id = kbo_team_add_kbo_org_team_id(before_original_team_id, kbo_league_id);
        uint32_t target_org_team_id = kbo_team_add_kbo_org_team_id(team_id, kbo_league_id);
        if (source_org_team_id != 0u
                && target_org_team_id != 0u
                && source_org_team_id != target_org_team_id) {
            uint8_t injury_slot_type = 0u;
            uint32_t injured_player_id = 0u;
            uint32_t effective_count = 0u;
            uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
            if (kbo_foreign_injury_replacement_signing_exception_available(
                    team_id,
                    player,
                    &injury_slot_type,
                    &injured_player_id,
                    &effective_count,
                    &effective_limit)) {
                static volatile LONG former_org_injury_allow_log_count = 0;
                LONG allow_slot = InterlockedIncrement(&former_org_injury_allow_log_count);
                if (allow_slot <= 120) {
                    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
                    int score = kbo_foreign_waiver_value_score(player);
                    kbo_log_runtimef(
                        "custom foreign policy former-org market signing allowed by injury slot player=%u team=%u source_team=%u source_org=%u target_org=%u before_current=%u before_active=%u current=%u active=%u score=%d effective=%u limit=%u injury_slot=%s injured=%u caller_rva=0x%x",
                        player_id,
                        team_id,
                        before_original_team_id,
                        source_org_team_id,
                        target_org_team_id,
                        before_current_team_id,
                        before_active_team_id,
                        current_team_id,
                        active_team_id,
                        score,
                        effective_count,
                        effective_limit,
                        kbo_foreign_injury_slot_label(injury_slot_type),
                        injured_player_id,
                        caller_rva);
                }
                return 0;
            }

            static volatile LONG former_org_block_log_count = 0;
            LONG former_org_slot = InterlockedIncrement(&former_org_block_log_count);
            if (former_org_slot <= 200) {
                uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
                int score = kbo_foreign_waiver_value_score(player);
                kbo_log_runtimef(
                    "custom foreign policy former-org market signing blocked player=%u team=%u source_team=%u source_org=%u target_org=%u before_current=%u before_active=%u current=%u active=%u score=%d caller_rva=0x%x",
                    player_id,
                    team_id,
                    before_original_team_id,
                    source_org_team_id,
                    target_org_team_id,
                    before_current_team_id,
                    before_active_team_id,
                    current_team_id,
                    active_team_id,
                    score,
                    caller_rva);
            }
            return 1;
        }
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    int allowed = kbo_custom_foreign_policy_team_allows_final_signing(
        team_id,
        player,
        &effective_before,
        &effective_after,
        &effective_limit,
        &slot_type,
        &injured_player_id);
    if (allowed) {
        return 0;
    }

    static volatile LONG block_log_count = 0;
    LONG slot = InterlockedIncrement(&block_log_count);
    if (slot <= 300) {
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        kbo_log_runtimef(
            "custom foreign policy team-add blocked player=%u team=%u before_current=%u before_active=%u current=%u active=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u",
            player_id,
            team_id,
            before_current_team_id,
            before_active_team_id,
            current_team_id,
            active_team_id,
            effective_before,
            effective_after,
            effective_limit,
            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
            injured_player_id);
    }
    return 1;
}

