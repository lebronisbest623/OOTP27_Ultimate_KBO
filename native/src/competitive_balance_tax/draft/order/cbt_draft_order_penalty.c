#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbt_draft_order_penalty.h"
#include "ledger/cbt_draft_order_ledger.h"
#include "../../records/cbt_records.h"
#include "../../rules/cbt_rules.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"

#define KBO_CBT_DRAFT_ORDER_MAX_ROWS 4096
#define KBO_CBT_DRAFT_ORDER_MAX_ROUND_ROWS 256
#define KBO_CBT_DRAFT_ORDER_MAX_MOVES 64

typedef struct KboCbtDraftOrderSlot {
    uint8_t* row;
    uint16_t pick;
    uint32_t original_team_id;
    uint32_t owner_team_id;
} KboCbtDraftOrderSlot;

typedef struct KboCbtDraftPenaltyInfo {
    uint32_t season;
    uint32_t team_id;
    uint32_t stages;
} KboCbtDraftPenaltyInfo;

static volatile LONG g_kbo_cbt_draft_order_busy = 0;
static PVOID volatile g_kbo_cbt_draft_order_observed_state = NULL;
static uintptr_t g_kbo_cbt_draft_order_last_state = 0;
static uint64_t g_kbo_cbt_draft_order_last_signature = 0;

static int kbo_cbt_draft_order_slot_compare(const void* a, const void* b)
{
    const KboCbtDraftOrderSlot* lhs = (const KboCbtDraftOrderSlot*)a;
    const KboCbtDraftOrderSlot* rhs = (const KboCbtDraftOrderSlot*)b;
    return (int)lhs->pick - (int)rhs->pick;
}

static int kbo_cbt_draft_order_penalty_for_team(
    uint32_t team_id,
    KboCbtDraftPenaltyInfo* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (team_id == 0u) {
        return 0;
    }

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);

    KboCbtRecord records[KBO_CBT_RECORDS_MAX];
    int record_count = kbo_cbt_load_records(records, KBO_CBT_RECORDS_MAX, NULL, 0);
    if (record_count <= 0) {
        return 0;
    }

    int best_index = -1;
    uint32_t best_season = 0u;
    for (int i = 0; i < record_count; i++) {
        const KboCbtRecord* rec = &records[i];
        if (rec->team_id == team_id && rec->season > best_season) {
            best_index = i;
            best_season = rec->season;
        }
    }
    if (best_index < 0) {
        return 0;
    }

    const KboCbtRecord* rec = &records[best_index];
    if (rec->overage <= 0 || rec->consecutive_count < rules.draft_penalty_min_consecutive) {
        return 0;
    }

    if (out != NULL) {
        out->season = rec->season;
        out->team_id = team_id;
        out->stages = rules.draft_penalty_stages;
    }
    return rules.draft_penalty_stages > 0u;
}

static int kbo_cbt_draft_order_team_is_main_kbo(uint32_t team_id)
{
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) == OOTP27_KBO_MAIN_LEAGUE_ID;
}

static uint64_t kbo_cbt_draft_order_signature(const KboCbtDraftOrderSlot* slots, int slot_count)
{
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < slot_count; i++) {
        uint64_t v = ((uint64_t)slots[i].pick << 48)
            ^ ((uint64_t)slots[i].owner_team_id << 16)
            ^ (uint64_t)slots[i].original_team_id;
        h ^= v;
        h *= 1099511628211ull;
    }
    h ^= (uint64_t)(uint32_t)slot_count;
    h *= 1099511628211ull;
    return h;
}

static int kbo_cbt_collect_round_slots(
    uintptr_t draft_state,
    KboCbtDraftOrderSlot* slots,
    int max_slots)
{
    if (draft_state == 0u || slots == NULL || max_slots <= 0) {
        return 0;
    }
    if (!memory_range_readable((void*)draft_state, OOTP27_DRAFT_STATE_READABLE_BYTES)) {
        return 0;
    }

    uint8_t* state = (uint8_t*)draft_state;
    uintptr_t vector = *(uintptr_t*)(state + OOTP27_DRAFT_STATE_ORDER_VECTOR_OFFSET);
    int32_t count = *(int32_t*)(state + OOTP27_DRAFT_STATE_ORDER_COUNT_OFFSET);
    if (vector == 0u || count <= 0 || count > KBO_CBT_DRAFT_ORDER_MAX_ROWS) {
        return 0;
    }
    if (!memory_range_readable((void*)vector, (SIZE_T)count * sizeof(uintptr_t))) {
        return 0;
    }

    int slot_count = 0;
    for (int32_t i = 0; i < count; i++) {
        uintptr_t row_ptr = *(uintptr_t*)(vector + (uintptr_t)i * sizeof(uintptr_t));
        if (row_ptr == 0u || !memory_range_readable((void*)row_ptr, OOTP27_DRAFT_ORDER_ROW_READABLE_BYTES)) {
            continue;
        }

        uint8_t* row = (uint8_t*)row_ptr;
        uint16_t round = *(uint16_t*)(row + OOTP27_DRAFT_ORDER_ROW_ROUND_OFFSET);
        uint8_t supplemental = *(uint8_t*)(row + OOTP27_DRAFT_ORDER_ROW_SUPPLEMENTAL_OFFSET);
        if (round != KBO_CBT_DRAFT_ORDER_TARGET_ROUND || supplemental != 0u) {
            continue;
        }

        uint16_t pick = *(uint16_t*)(row + OOTP27_DRAFT_ORDER_ROW_PICK_OFFSET);
        uint32_t owner = *(uint32_t*)(row + OOTP27_DRAFT_ORDER_ROW_OWNER_TEAM_OFFSET);
        uint32_t original = *(uint32_t*)(row + OOTP27_DRAFT_ORDER_ROW_ORIGINAL_TEAM_OFFSET);
        if (pick == 0u || owner == 0u) {
            continue;
        }
        if (slot_count >= max_slots) {
            kbo_log_runtimef(
                "KBO CBT draft order skipped reason=round_slot_limit state=%p count=%d max=%d",
                (void*)draft_state,
                count,
                max_slots);
            return 0;
        }

        slots[slot_count].row = row;
        slots[slot_count].pick = pick;
        slots[slot_count].owner_team_id = owner;
        slots[slot_count].original_team_id = original;
        slot_count++;
    }

    if (slot_count > 1) {
        qsort(slots, (size_t)slot_count, sizeof(slots[0]), kbo_cbt_draft_order_slot_compare);
    }
    return slot_count;
}

static int kbo_cbt_find_owner_index(const uint32_t* owners, int count, uint32_t team_id)
{
    if (owners == NULL || count <= 0 || team_id == 0u) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (owners[i] == team_id) {
            return i;
        }
    }
    return -1;
}

void kbo_cbt_note_draft_order_state(uintptr_t draft_state)
{
    if (draft_state == 0u) {
        return;
    }
    InterlockedExchangePointer(
        &g_kbo_cbt_draft_order_observed_state,
        (PVOID)draft_state);
}

int kbo_cbt_apply_primary_league_draft_order_penalties(uint32_t league_id, const char* source)
{
    if (league_id == 0u) {
        return 0;
    }

    uintptr_t league_ptr = kbo_find_league_ptr_from_id(league_id);
    if (league_ptr == 0u || !memory_range_readable(
            (void*)(league_ptr + OOTP27_LEAGUE_PRIMARY_DRAFT_STATE_OFFSET),
            OOTP27_DRAFT_STATE_READABLE_BYTES)) {
        kbo_log_runtimef(
            "KBO CBT draft order primary league apply skipped source=%s league_id=%u reason=league_state_unavailable",
            source != NULL ? source : "",
            league_id);
        return 0;
    }

    uintptr_t draft_state = league_ptr + OOTP27_LEAGUE_PRIMARY_DRAFT_STATE_OFFSET;
    kbo_cbt_note_draft_order_state(draft_state);
    return kbo_cbt_apply_draft_order_penalties(draft_state, source);
}

int kbo_cbt_apply_pending_draft_order_penalties(const char* source)
{
    uintptr_t draft_state = (uintptr_t)InterlockedCompareExchangePointer(
        &g_kbo_cbt_draft_order_observed_state,
        NULL,
        NULL);
    if (draft_state == 0u) {
        kbo_log_runtimef(
            "KBO CBT draft order pending apply skipped source=%s reason=no_observed_draft_state",
            source != NULL ? source : "");
        return 0;
    }
    return kbo_cbt_apply_draft_order_penalties(draft_state, source);
}

int kbo_cbt_apply_draft_order_penalties(uintptr_t draft_state, const char* source)
{
    if (draft_state == 0u || !kbo_fix_enabled()) {
        return 0;
    }
    if (read_kbo_localappdata_flag_file("disable_kbo_competitive_balance_tax.txt")
            || read_kbo_localappdata_flag_file("disable_kbo_cbt_draft_order_penalty.txt")) {
        return 0;
    }
    if (InterlockedCompareExchange(&g_kbo_cbt_draft_order_busy, 1, 0) != 0) {
        return 0;
    }

    KboCbtDraftOrderSlot slots[KBO_CBT_DRAFT_ORDER_MAX_ROUND_ROWS];
    int slot_count = kbo_cbt_collect_round_slots(
        draft_state,
        slots,
        KBO_CBT_DRAFT_ORDER_MAX_ROUND_ROWS);
    if (slot_count <= 1) {
        InterlockedExchange(&g_kbo_cbt_draft_order_busy, 0);
        return 0;
    }

    uint64_t before_signature = kbo_cbt_draft_order_signature(slots, slot_count);
    if (g_kbo_cbt_draft_order_last_state == draft_state
            && g_kbo_cbt_draft_order_last_signature == before_signature) {
        kbo_log_runtimef(
            "KBO CBT draft order penalty skipped source=%s reason=already_adjusted_state state=%p rows=%d",
            source != NULL ? source : "",
            (void*)draft_state,
            slot_count);
        InterlockedExchange(&g_kbo_cbt_draft_order_busy, 0);
        return 0;
    }

    uint32_t owners[KBO_CBT_DRAFT_ORDER_MAX_ROUND_ROWS];
    uint32_t originals[KBO_CBT_DRAFT_ORDER_MAX_ROUND_ROWS];
    int sync_originals = 1;
    for (int i = 0; i < slot_count; i++) {
        owners[i] = slots[i].owner_team_id;
        originals[i] = slots[i].original_team_id;
        if (owners[i] != originals[i]) {
            sync_originals = 0;
        }
    }

    KboCbtDraftPenaltyInfo penalties[KBO_CBT_DRAFT_ORDER_MAX_MOVES];
    int penalty_count = 0;
    for (int i = 0; i < slot_count && penalty_count < KBO_CBT_DRAFT_ORDER_MAX_MOVES; i++) {
        uint32_t team_id = slots[i].owner_team_id;
        if (!kbo_cbt_draft_order_team_is_main_kbo(team_id)) {
            continue;
        }

        KboCbtDraftPenaltyInfo info;
        if (!kbo_cbt_draft_order_penalty_for_team(team_id, &info) || info.stages == 0u) {
            continue;
        }

        int already_seen = 0;
        for (int j = 0; j < penalty_count; j++) {
            if (penalties[j].team_id == team_id) {
                already_seen = 1;
                break;
            }
        }
        if (already_seen) {
            continue;
        }
        penalties[penalty_count++] = info;
    }

    KboCbtDraftOrderMove moves[KBO_CBT_DRAFT_ORDER_MAX_MOVES];
    int move_count = 0;
    for (int p = 0; p < penalty_count; p++) {
        KboCbtDraftPenaltyInfo* penalty = &penalties[p];
        int from_index = kbo_cbt_find_owner_index(owners, slot_count, penalty->team_id);
        if (from_index < 0) {
            continue;
        }
        int to_index = from_index + (int)penalty->stages;
        if (to_index >= slot_count) {
            to_index = slot_count - 1;
        }
        if (to_index <= from_index) {
            kbo_log_runtimef(
                "KBO CBT draft order penalty skipped source=%s season=%u team=%u stages=%u reason=already_last from_pick=%u",
                source != NULL ? source : "",
                penalty->season,
                penalty->team_id,
                penalty->stages,
                slots[from_index].pick);
            continue;
        }

        uint32_t moved_owner = owners[from_index];
        uint32_t moved_original = originals[from_index];
        for (int i = from_index; i < to_index; i++) {
            owners[i] = owners[i + 1];
            if (sync_originals) {
                originals[i] = originals[i + 1];
            }
        }
        owners[to_index] = moved_owner;
        if (sync_originals) {
            originals[to_index] = moved_original;
        }

        if (move_count < KBO_CBT_DRAFT_ORDER_MAX_MOVES) {
            moves[move_count].season = penalty->season;
            moves[move_count].team_id = penalty->team_id;
            moves[move_count].stages = penalty->stages;
            moves[move_count].from_pick = slots[from_index].pick;
            moves[move_count].to_pick = slots[to_index].pick;
            move_count++;
        }
    }

    if (move_count <= 0) {
        InterlockedExchange(&g_kbo_cbt_draft_order_busy, 0);
        return 0;
    }

    for (int i = 0; i < slot_count; i++) {
        *(uint32_t*)(slots[i].row + OOTP27_DRAFT_ORDER_ROW_OWNER_TEAM_OFFSET) = owners[i];
        if (sync_originals) {
            *(uint32_t*)(slots[i].row + OOTP27_DRAFT_ORDER_ROW_ORIGINAL_TEAM_OFFSET) = originals[i];
        }
        slots[i].owner_team_id = owners[i];
        if (sync_originals) {
            slots[i].original_team_id = originals[i];
        }
    }

    g_kbo_cbt_draft_order_last_state = draft_state;
    g_kbo_cbt_draft_order_last_signature = kbo_cbt_draft_order_signature(slots, slot_count);

    for (int i = 0; i < move_count; i++) {
        (void)kbo_cbt_draft_order_append_ledger(&moves[i], source);
        kbo_log_runtimef(
            "KBO CBT draft order penalty applied source=%s season=%u team=%u round=%u stages=%u pick=%u->%u state=%p rows=%d sync_originals=%d",
            source != NULL ? source : "",
            moves[i].season,
            moves[i].team_id,
            KBO_CBT_DRAFT_ORDER_TARGET_ROUND,
            moves[i].stages,
            moves[i].from_pick,
            moves[i].to_pick,
            (void*)draft_state,
            slot_count,
            sync_originals);
    }

    InterlockedExchange(&g_kbo_cbt_draft_order_busy, 0);
    return move_count;
}
