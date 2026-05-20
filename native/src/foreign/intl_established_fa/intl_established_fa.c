#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../common/player_eval/foreign_waiver_player_eval.h"
#include "../common/policy/foreign_waiver_policy.h"
#include "../intl_established_fa_postscan/api/intl_established_fa_postscan.h"
#include "intl_established_fa_policy.h"

static LONG g_kbo_intl_established_fa_quality_probe_log_count = 0;
static LONG g_kbo_intl_established_fa_generation_filter_block_count = 0;

static uint32_t kbo_intl_established_fa_read_league_id(uintptr_t league_ptr, uint32_t offset)
{
    if (league_ptr == 0 || !memory_range_readable((void*)(league_ptr + offset), sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(league_ptr + offset);
}

static int kbo_intl_established_fa_league_matches(uintptr_t league_ptr, uint32_t* out_primary_id, uint32_t* out_fallback_id)
{
    uint32_t primary_id = kbo_intl_established_fa_read_league_id(league_ptr, OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
    uint32_t fallback_id = kbo_intl_established_fa_read_league_id(league_ptr, OOTP27_KBO_LEAGUE_ID_OFFSET);
    if (out_primary_id != NULL) {
        *out_primary_id = primary_id;
    }
    if (out_fallback_id != NULL) {
        *out_fallback_id = fallback_id;
    }

    uint32_t configured_id = kbo_resolve_kbo_league_id();
    if (configured_id == 0u) {
        return 0;
    }
    return primary_id == configured_id || fallback_id == configured_id;
}

__declspec(noinline) int32_t ootp_kbo_intl_established_fa_count_wrapper(int32_t original_count, uintptr_t league_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (original_count <= 0) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_established_count", original_count);
    }

    uint32_t primary_id = 0u;
    uint32_t fallback_id = 0u;
    if (!kbo_intl_established_fa_league_matches(league_ptr, &primary_id, &fallback_id)) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_established_count", original_count);
    }

    InterlockedExchange(&g_kbo_intl_established_fa_quality_probe_log_count, 0);
    InterlockedExchange(&g_kbo_intl_established_fa_generation_filter_block_count, 0);

    int multiplier = kbo_get_intl_established_fa_multiplier();
    if (multiplier <= 1) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_established_count", original_count);
    }
    const KboIntlEstablishedFaPolicy* policy = kbo_intl_established_fa_policy();

    int64_t scaled = (int64_t)original_count * (int64_t)multiplier;
    if (scaled > policy->max_scaled_count) {
        scaled = policy->max_scaled_count;
    }

    kbo_log_runtimef(
        "international established FA multiplier league=%p league_id=%u/%u original=%d multiplier=%d scaled=%d",
        (void*)league_ptr,
        primary_id,
        fallback_id,
        original_count,
        multiplier,
        (int32_t)scaled);
    kbo_intl_established_fa_postscan_schedule(
        original_count,
        (int32_t)scaled,
        multiplier,
        primary_id,
        fallback_id);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_established_count", (int32_t)scaled);
}

/* Generation-time filter for international established free agents. */

static int kbo_intl_established_fa_generation_filter_enabled(void)
{
    return kbo_get_foreign_fa_quality_cap_enabled_setting();
}

static int kbo_intl_established_fa_player_is_catcher(uint8_t position_group, uint8_t position_role)
{
    return position_group == 2u || (position_group != 1u && position_role == 2u);
}

static uint32_t kbo_intl_established_fa_mix_u32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static int kbo_intl_established_fa_catcher_rarity_allows(uint32_t player_id, uint32_t nation_id)
{
    const KboIntlEstablishedFaPolicy* policy = kbo_intl_established_fa_policy();
    uint32_t mixed = kbo_intl_established_fa_mix_u32(
        player_id ^ (nation_id * 0x9e3779b9u) ^ 0xC0A7C4E5u);
    return (mixed % (uint32_t)policy->catcher_allow_one_in) == 0u;
}

__declspec(noinline) uint8_t ootp_kbo_intl_established_fa_generation_filter_allows_wrapper(
    uintptr_t player_ptr,
    uintptr_t league_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (!kbo_intl_established_fa_generation_filter_enabled()) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_generation_filter", 1u);
    }
    if (player_ptr == 0 || league_ptr == 0) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_generation_filter", 1u);
    }

    uint32_t primary_id = 0u;
    uint32_t fallback_id = 0u;
    if (!kbo_intl_established_fa_league_matches(league_ptr, &primary_id, &fallback_id)) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_generation_filter", 1u);
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_generation_filter", 1u);
    }
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_generation_filter", 1u);
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint8_t position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    uint8_t position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);

    int asian_quota = kbo_nation_is_asian_quota_candidate(nation_id);
    int32_t value_score = kbo_foreign_waiver_value_score(player);
    int32_t quality_cap = kbo_intl_established_fa_quality_score_cap(
        asian_quota,
        position_group,
        position_role);
    const char* reject_reason = NULL;
    if (kbo_intl_established_fa_player_is_catcher(position_group, position_role)
            && !kbo_intl_established_fa_catcher_rarity_allows(player_id, nation_id)) {
        reject_reason = "catcher_rarity";
    } else if (position_group == 1u
            && !asian_quota
            && kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
        reject_reason = "non_asian_bullpen";
    } else if (quality_cap > 0 && value_score > quality_cap) {
        reject_reason = kbo_intl_established_fa_quality_policy_label(
            asian_quota,
            position_group,
            position_role);
    }
    if (reject_reason == NULL) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_generation_filter", 1u);
    }

    LONG slot = InterlockedIncrement(&g_kbo_intl_established_fa_generation_filter_block_count);
    if (slot <= 160) {
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        kbo_log_runtimef(
            "international established FA generation filter rejected #%ld reason=%s player=%u nation=%u asian_quota=%d age=%d pos=%u/%u league_id=%u/%u score=%d cap=%d action=retry_without_registering",
            slot,
            reject_reason,
            player_id,
            nation_id,
            asian_quota,
            (int)age,
            position_group,
            position_role,
            primary_id,
            fallback_id,
            value_score,
            quality_cap);
    } else if (slot == 161) {
        kbo_log_runtime_line("international established FA generation filter rejected log suppressed after 160 players");
    }

    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.intl_generation_filter", 0u);
}

__declspec(noinline) void ootp_kbo_intl_established_fa_player_probe_wrapper(
    uintptr_t player_ptr,
    uintptr_t league_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (player_ptr == 0 || league_ptr == 0) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.intl_player_probe");
    }

    uint32_t primary_id = 0u;
    uint32_t fallback_id = 0u;
    if (!kbo_intl_established_fa_league_matches(league_ptr, &primary_id, &fallback_id)) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.intl_player_probe");
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        kbo_log_runtimef(
            "international established FA quality probe skipped reason=player_unreadable player=%p league=%p league_id=%u/%u",
            (void*)player_ptr,
            (void*)league_ptr,
            primary_id,
            fallback_id);
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.intl_player_probe");
    }

    LONG slot = InterlockedIncrement(&g_kbo_intl_established_fa_quality_probe_log_count);
    if (slot > 512) {
        if (slot == 513) {
            kbo_log_runtime_line("international established FA quality probe suppressed after 512 generated players");
        }
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.intl_player_probe");
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    uint8_t position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);
    uint8_t draft_class = *(uint8_t*)(player + OOTP27_PLAYER_DRAFT_CLASS_OFFSET);
    uint8_t draft_subtype = *(uint8_t*)(player + OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET);
    uint8_t draft_eligible = *(uint8_t*)(player + OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET);
    uint8_t draft_extra = *(uint8_t*)(player + OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET);
    uint8_t generation_flags = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_FLAGS_OFFSET);
    uint8_t generation_context = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET);
    uint8_t generation_grade = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_GRADE_OFFSET);
    uint8_t generation_special = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_SPECIAL_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    int32_t value_score = kbo_foreign_waiver_value_score(player);
    int asian_quota = kbo_nation_is_asian_quota_candidate(nation_id);

    kbo_log_runtimef(
        "international established FA quality probe #%ld player=%p player_id=%u league=%p league_id=%u/%u nation=%u asian_quota=%d age=%d pos=%u/%u gen=flags:%u context:%u grade:%u special:%u draft=db1:%u db2:%u db5:%u db6:%u value=overall:%d talent:%d ratings:%d career:%d score:%d",
        slot,
        (void*)player_ptr,
        player_id,
        (void*)league_ptr,
        primary_id,
        fallback_id,
        nation_id,
        asian_quota,
        (int)age,
        position_group,
        position_role,
        generation_flags,
        generation_context,
        generation_grade,
        generation_special,
        draft_class,
        draft_subtype,
        draft_eligible,
        draft_extra,
        overall,
        talent,
        ratings,
        career,
        value_score);
    KBO_HOOK_PROFILE_END(profile_hook, "foreign.intl_player_probe");
}
