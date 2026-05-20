#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/sync/lock.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../lookup/team_lookup.h"
#include "../internal/team_add_player_guard_ai_roster_internal.h"
#include "../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

int32_t kbo_pointer_vector_count(uintptr_t vector_ptr)
{
    if (vector_ptr == 0u || !memory_range_readable((void*)vector_ptr, 0x18u)) {
        return -1;
    }

    uint8_t* vector = (uint8_t*)vector_ptr;
    int32_t count = *(int32_t*)(vector + 0x0cu);
    if (count < 0 || count > 10000) {
        return -1;
    }

    uintptr_t values = *(uintptr_t*)vector;
    if (count > 0 && (values == 0u || !memory_range_readable((void*)values, (SIZE_T)count * sizeof(uintptr_t)))) {
        return -1;
    }
    return count;
}

static uintptr_t kbo_pointer_vector_value_at(uintptr_t vector_ptr, int32_t index, int32_t count)
{
    if (vector_ptr == 0u
            || index < 0
            || count <= 0
            || index >= count
            || count > 10000
            || !memory_range_readable((void*)vector_ptr, 0x18u)) {
        return 0u;
    }

    uintptr_t values = *(uintptr_t*)vector_ptr;
    if (values == 0u || !memory_range_readable((void*)values, (SIZE_T)count * sizeof(uintptr_t))) {
        return 0u;
    }
    return ((uintptr_t*)values)[index];
}

static int32_t kbo_read_ai_roster_select_score(uintptr_t player_ptr, uint32_t offset)
{
    if (player_ptr == 0u
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(player_ptr + offset);
}

static void kbo_read_player_default_status(
    uint8_t* player,
    uint32_t* out_status24,
    uint32_t* out_status25,
    uint32_t* out_status26)
{
    if (out_status24 != NULL) {
        *out_status24 = 0u;
    }
    if (out_status25 != NULL) {
        *out_status25 = 0u;
    }
    if (out_status26 != NULL) {
        *out_status26 = 0u;
    }
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        return;
    }

    uintptr_t status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    if (status_ptr == 0u || !memory_range_readable((void*)status_ptr, 0x29u)) {
        return;
    }

    uint8_t* status = (uint8_t*)status_ptr;
    if (out_status24 != NULL) {
        *out_status24 = status[0x24u];
    }
    if (out_status25 != NULL) {
        *out_status25 = status[0x25u];
    }
    if (out_status26 != NULL) {
        *out_status26 = status[0x26u];
    }
}

static void kbo_fill_ai_roster_foreign_candidate_summary(
    KboAiRosterForeignCandidateSummary* summary,
    int32_t index,
    uint8_t* player)
{
    if (summary == NULL) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
    summary->index = -1;

    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    summary->index = index;
    summary->player_ptr = (uintptr_t)player;
    summary->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    summary->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    summary->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    summary->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    summary->league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        summary->default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    kbo_read_player_default_status(player, &summary->status24, &summary->status25, &summary->status26);
    summary->f25 = (uint32_t)player[0xf25u];
    summary->f62 = (uint32_t)player[0xf62u];
    summary->f65 = (uint32_t)player[0xf65u];
    summary->f06 = kbo_read_player_i16(player, 0xf06u);
    summary->score_fe0 = kbo_read_ai_roster_select_score(summary->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
    summary->score_fe4 = kbo_read_ai_roster_select_score(summary->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
    summary->overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    summary->talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    summary->ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
}

static int64_t kbo_ai_roster_source_select_rescue_score(
    const KboAiRosterForeignCandidateSummary* summary,
    uint8_t* player)
{
    if (summary == NULL || player == NULL) {
        return INT64_MIN;
    }

    int64_t score = 0;
    score += (int64_t)summary->overall * 6;
    score += (int64_t)summary->ratings * 4;
    score += (int64_t)summary->talent * 2;
    score += (int64_t)summary->score_fe4 * 150;
    score += (int64_t)summary->score_fe0 * 80;
    score += (int64_t)summary->f25 * 12;
    score += (int64_t)kbo_read_player_i16(player, 0xf06u) * 8;
    score -= (int64_t)summary->status26 * 40;
    return score;
}

static uint32_t kbo_ai_roster_parent_team_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u) {
            return parent_team_id;
        }
    }
    return team_id;
}

static uint8_t* kbo_ai_roster_resolve_active_team(uint8_t* player, uint32_t* out_team_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return NULL;
    }

    uint32_t team_ids[3] = {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
    };
    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    for (uint32_t i = 0u; i < 3u; i++) {
        uint32_t org_team_id = kbo_ai_roster_parent_team_id(team_ids[i]);
        if (org_team_id == 0u) {
            continue;
        }

        uint8_t* team = find_kbo_team_by_numeric_id_any_league(org_team_id, 1);
        if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (kbo_league_id != 0u && team_league_id != kbo_league_id) {
            continue;
        }

        if (out_team_id != NULL) {
            *out_team_id = org_team_id;
        }
        return team;
    }
    return NULL;
}

int kbo_ai_roster_minor_foreign_callup_allows(
    int32_t team_arg,
    uint8_t* player,
    uint32_t* out_team_id,
    int* out_callup_allowed)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_callup_allowed != NULL) {
        *out_callup_allowed = 0;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    int minor_league_match = kbo_league_id != 0u
        && (current_league_id == kbo_league_id + 1u || (uint32_t)team_arg == kbo_league_id + 1u);
    if (!minor_league_match) {
        return 0;
    }

    uint32_t active_team_id = 0u;
    uint8_t* active_team = kbo_ai_roster_resolve_active_team(player, &active_team_id);
    if (active_team == NULL) {
        return 0;
    }

    uint8_t allowed = kbo_custom_foreign_policy_callup_allows(
        (uintptr_t)active_team,
        (uintptr_t)player,
        0,
        (int32_t)KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT,
        3);
    if (out_team_id != NULL) {
        *out_team_id = active_team_id;
    }
    if (out_callup_allowed != NULL) {
        *out_callup_allowed = allowed ? 1 : 0;
    }
    return 1;
}

uintptr_t kbo_ai_roster_choose_source_select_rescue_candidate(
    uintptr_t source_vector_ptr,
    int32_t source_count,
    uintptr_t native_result_ptr,
    int32_t* out_source_index,
    uint32_t* out_active_team_id,
    int64_t* out_score,
    KboAiRosterForeignCandidateSummary* out_summary)
{
    if (out_source_index != NULL) {
        *out_source_index = -1;
    }
    if (out_active_team_id != NULL) {
        *out_active_team_id = 0u;
    }
    if (out_score != NULL) {
        *out_score = 0;
    }
    if (out_summary != NULL) {
        memset(out_summary, 0, sizeof(*out_summary));
        out_summary->index = -1;
    }

    if (!kbo_custom_foreign_policy_enabled()
            || !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_AI_ROSTER_FOREIGN_SOURCE_SELECT_RESCUE_FILE)
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_AI_ROSTER_FOREIGN_SOURCE_SELECT_RESCUE_FILE)
            || source_vector_ptr == 0u
            || source_count <= 0) {
        return 0u;
    }

    if (kbo_player_pointer_plausible(native_result_ptr)
            && memory_range_readable((void*)native_result_ptr, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights((uint8_t*)native_result_ptr)) {
        return 0u;
    }

    int32_t scanned_count = source_count > KBO_AI_ROSTER_SELECT_SCAN_LIMIT
        ? KBO_AI_ROSTER_SELECT_SCAN_LIMIT
        : source_count;
    uintptr_t best_ptr = 0u;
    int32_t best_index = -1;
    uint32_t best_active_team_id = 0u;
    int64_t best_score = INT64_MIN;
    KboAiRosterForeignCandidateSummary best_summary = {0};
    best_summary.index = -1;
    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();

    for (int32_t i = 0; i < scanned_count; i++) {
        uintptr_t candidate_ptr = kbo_pointer_vector_value_at(source_vector_ptr, i, source_count);
        if (!kbo_player_pointer_plausible(candidate_ptr)
                || !memory_range_readable((void*)candidate_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }

        uint8_t* candidate = (uint8_t*)candidate_ptr;
        if (!kbo_player_is_foreign_for_kbo_rights(candidate)) {
            continue;
        }

        KboAiRosterForeignCandidateSummary summary;
        kbo_fill_ai_roster_foreign_candidate_summary(&summary, i, candidate);
        if (summary.player_id == 0u
                || summary.f25 < KBO_AI_ROSTER_FOREIGN_F25_MIN
                || summary.f65 != 0u
                || (summary.status26 != 0u && summary.status26 != 1u)) {
            continue;
        }

        uint32_t active_team_id = 0u;
        int callup_allowed = 0;
        if (!kbo_ai_roster_minor_foreign_callup_allows(
                (int32_t)summary.league_id,
                candidate,
                &active_team_id,
                &callup_allowed)
                || !callup_allowed
                || active_team_id == 0u) {
            continue;
        }

        if (kbo_league_id != 0u
                && summary.current_team_id == active_team_id
                && summary.active_team_id == active_team_id
                && summary.league_id == kbo_league_id) {
            continue;
        }

        int64_t score = kbo_ai_roster_source_select_rescue_score(&summary, candidate);
        if (score > best_score) {
            best_score = score;
            best_ptr = candidate_ptr;
            best_index = i;
            best_active_team_id = active_team_id;
            best_summary = summary;
        }
    }

    if (best_ptr == 0u) {
        return 0u;
    }

    static KboLock recent_lock = KBO_LOCK_INIT;
    static uint32_t recent_player_id = 0u;
    static DWORD recent_tick = 0u;
    DWORD now = GetTickCount();
    kbo_lock_enter(&recent_lock);
    DWORD recent_age_ms = now - recent_tick;
    if (best_summary.player_id != 0u
            && best_summary.player_id == recent_player_id
            && recent_age_ms <= 5000u) {
        recent_tick = now;
        kbo_lock_leave(&recent_lock);
        return 0u;
    }
    recent_player_id = best_summary.player_id;
    recent_tick = now;
    kbo_lock_leave(&recent_lock);

    if (out_source_index != NULL) {
        *out_source_index = best_index;
    }
    if (out_active_team_id != NULL) {
        *out_active_team_id = best_active_team_id;
    }
    if (out_score != NULL) {
        *out_score = best_score;
    }
    if (out_summary != NULL) {
        *out_summary = best_summary;
    }
    return best_ptr;
}
