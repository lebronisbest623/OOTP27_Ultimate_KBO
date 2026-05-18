#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ui.h"
#include "../../ai/independent_acquisition_ai_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../foreign/injury/api/foreign_injury_labels.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../lookup/team_lookup.h"

#define KBO_INDEPENDENT_ACQUISITION_UI_TEAM_ORG_CACHE_MAX 128

typedef struct KboIndependentAcquisitionUiTeamOrgCacheEntry {
    uint32_t team_id;
    uint32_t org_team_id;
} KboIndependentAcquisitionUiTeamOrgCacheEntry;

typedef struct KboIndependentAcquisitionUiTeamOrgCache {
    KboIndependentAcquisitionUiTeamOrgCacheEntry entries[
        KBO_INDEPENDENT_ACQUISITION_UI_TEAM_ORG_CACHE_MAX];
    int count;
} KboIndependentAcquisitionUiTeamOrgCache;

typedef struct KboIndependentAcquisitionUiTeamMatch {
    uint32_t team_id;
    uint32_t org_team_id;
} KboIndependentAcquisitionUiTeamMatch;

typedef struct KboIndependentAcquisitionUiSellerMatch {
    KboIndependentFuturesTeamLeague seller;
    KboIndependentAcquisitionUiTeamMatch match;
} KboIndependentAcquisitionUiSellerMatch;

typedef struct KboIndependentAcquisitionUiPlayerAssignmentSnapshot {
    uint32_t team_ids[3];
    uint32_t org_team_ids[3];
    int count;
} KboIndependentAcquisitionUiPlayerAssignmentSnapshot;

static uint32_t kbo_independent_acquisition_ui_org_team_id(
    uint32_t team_id,
    KboIndependentAcquisitionUiTeamOrgCache* cache)
{
    if (team_id == 0u) {
        return 0u;
    }
    if (cache != NULL) {
        for (int i = 0; i < cache->count; i++) {
            if (cache->entries[i].team_id == team_id) {
                return cache->entries[i].org_team_id;
            }
        }
    }

    uint32_t org_team_id = team_id;
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL
            && memory_range_readable(
                team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET,
                sizeof(uint32_t))) {
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u) {
            org_team_id = parent_team_id;
        }
    }

    if (cache != NULL && cache->count < KBO_INDEPENDENT_ACQUISITION_UI_TEAM_ORG_CACHE_MAX) {
        cache->entries[cache->count].team_id = team_id;
        cache->entries[cache->count].org_team_id = org_team_id;
        cache->count++;
    }
    return org_team_id;
}

static KboIndependentAcquisitionUiTeamMatch kbo_independent_acquisition_ui_team_match(
    uint32_t team_id,
    KboIndependentAcquisitionUiTeamOrgCache* cache)
{
    KboIndependentAcquisitionUiTeamMatch match;
    memset(&match, 0, sizeof(match));
    match.team_id = team_id;
    match.org_team_id = kbo_independent_acquisition_ui_org_team_id(team_id, cache);
    return match;
}

static int kbo_independent_acquisition_ui_add_player_assignment(
    KboIndependentAcquisitionUiPlayerAssignmentSnapshot* snapshot,
    uint32_t team_id,
    KboIndependentAcquisitionUiTeamOrgCache* cache)
{
    if (snapshot == NULL || team_id == 0u) {
        return 0;
    }
    for (int i = 0; i < snapshot->count; i++) {
        if (snapshot->team_ids[i] == team_id) {
            return 1;
        }
    }
    if (snapshot->count >= 3) {
        return 0;
    }
    snapshot->team_ids[snapshot->count] = team_id;
    snapshot->org_team_ids[snapshot->count] =
        kbo_independent_acquisition_ui_org_team_id(team_id, cache);
    snapshot->count++;
    return 1;
}

static int kbo_independent_acquisition_ui_build_player_assignment_snapshot(
    uint8_t* player,
    KboIndependentAcquisitionUiTeamOrgCache* cache,
    KboIndependentAcquisitionUiPlayerAssignmentSnapshot* out_snapshot)
{
    if (out_snapshot != NULL) {
        memset(out_snapshot, 0, sizeof(*out_snapshot));
    }
    if (player == NULL
            || out_snapshot == NULL
            || !memory_range_readable(
                player,
                OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET + sizeof(uint32_t))) {
        return 0;
    }

    kbo_independent_acquisition_ui_add_player_assignment(
        out_snapshot,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        cache);
    kbo_independent_acquisition_ui_add_player_assignment(
        out_snapshot,
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        cache);
    kbo_independent_acquisition_ui_add_player_assignment(
        out_snapshot,
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET),
        cache);
    return out_snapshot->count > 0;
}

static int kbo_independent_acquisition_ui_assignment_snapshot_matches_team(
    const KboIndependentAcquisitionUiPlayerAssignmentSnapshot* snapshot,
    const KboIndependentAcquisitionUiTeamMatch* match)
{
    if (snapshot == NULL || match == NULL || match->org_team_id == 0u) {
        return 0;
    }
    for (int i = 0; i < snapshot->count; i++) {
        if (snapshot->team_ids[i] == match->team_id
                || snapshot->team_ids[i] == match->org_team_id
                || snapshot->org_team_ids[i] == match->org_team_id) {
            return 1;
        }
    }
    return 0;
}

static const KboIndependentAcquisitionUiSellerMatch* kbo_independent_acquisition_ui_seller_for_player(
    const KboIndependentAcquisitionUiPlayerAssignmentSnapshot* snapshot,
    const KboIndependentAcquisitionUiSellerMatch* sellers,
    int seller_count)
{
    if (snapshot == NULL || sellers == NULL || seller_count <= 0) {
        return NULL;
    }
    for (int i = 0; i < seller_count; i++) {
        if (sellers[i].seller.team_id != 0u
                && kbo_independent_acquisition_ui_assignment_snapshot_matches_team(
                    snapshot,
                    &sellers[i].match)) {
            return &sellers[i];
        }
    }
    return NULL;
}

static void kbo_independent_acquisition_ui_slot_label(
    uint8_t slot_type,
    int foreign_player,
    int asian_quota,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (!foreign_player) {
        snprintf(out, out_size, "국내");
        return;
    }
    if (slot_type != 0u) {
        snprintf(out, out_size, "%s", kbo_foreign_injury_slot_label(slot_type));
        return;
    }
    snprintf(out, out_size, "%s", asian_quota ? "아시아" : "외국인");
}

static void kbo_independent_acquisition_ui_insert_offer_row(
    KboIndependentAcquisitionUiOfferRow* rows,
    int* count,
    int max_rows,
    const KboIndependentAcquisitionUiOfferRow* row)
{
    if (rows == NULL || count == NULL || max_rows <= 0 || row == NULL) {
        return;
    }
    if (*count < max_rows) {
        rows[*count] = *row;
        (*count)++;
        return;
    }

    int min_index = 0;
    for (int i = 1; i < max_rows; i++) {
        if (rows[i].request_score < rows[min_index].request_score) {
            min_index = i;
        }
    }
    if (row->request_score > rows[min_index].request_score) {
        rows[min_index] = *row;
    }
}

static int kbo_independent_acquisition_ui_offer_row_cmp(const void* a, const void* b)
{
    const KboIndependentAcquisitionUiOfferRow* left = (const KboIndependentAcquisitionUiOfferRow*)a;
    const KboIndependentAcquisitionUiOfferRow* right = (const KboIndependentAcquisitionUiOfferRow*)b;
    if (left->already_decided != right->already_decided) {
        return (int)left->already_decided - (int)right->already_decided;
    }
    if (left->already_requested != right->already_requested) {
        return (int)left->already_requested - (int)right->already_requested;
    }
    if (left->request_score < right->request_score) {
        return 1;
    }
    if (left->request_score > right->request_score) {
        return -1;
    }
    return 0;
}

static int kbo_independent_acquisition_ui_request_row_exists(
    const KboIndependentAcquisitionQueuedRequest* requests,
    int request_count,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (requests == NULL
            || request_count <= 0
            || buyer_team_id == 0u
            || seller_team_id == 0u
            || player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < request_count; i++) {
        if (requests[i].buyer_team_id == buyer_team_id
                && requests[i].seller_team_id == seller_team_id
                && requests[i].player_id == player_id) {
            return 1;
        }
    }
    return 0;
}

int kbo_independent_acquisition_ui_collect_offer_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiOfferRow* out_rows,
    int max_rows,
    KboIndependentAcquisitionUiContext* out_context)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);

    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)) {
        if (out_context != NULL) { *out_context = context; }
        return 0;
    }
    if (out_context != NULL) {
        *out_context = context;
    }
    if (!context.policy_enabled || !context.buyer_valid || context.seller_count <= 0) {
        return 0;
    }

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    int seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        NULL,
        NULL);
    if (seller_count <= 0) {
        return 0;
    }

    KboIndependentAcquisitionUiTeamOrgCache team_org_cache;
    memset(&team_org_cache, 0, sizeof(team_org_cache));
    KboIndependentAcquisitionUiSellerMatch seller_matches[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    memset(seller_matches, 0, sizeof(seller_matches));

    int32_t seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;
    int available_seller_count = 0;
    for (int i = 0; i < seller_count; i++) {
        int transfers = kbo_independent_acquisition_transferred_count(context.season, sellers[i].team_id);
        if (transfers >= seller_transfer_limit) {
            continue;
        }
        seller_matches[available_seller_count].seller = sellers[i];
        seller_matches[available_seller_count].match =
            kbo_independent_acquisition_ui_team_match(sellers[i].team_id, &team_org_cache);
        available_seller_count++;
    }
    seller_count = available_seller_count;
    if (seller_count <= 0) {
        return 0;
    }

    uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(buyer_team_id, 1);
    if (buyer_team == NULL || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    KboIndependentAcquisitionBuyerState buyer;
    kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
    if (buyer.team_id == 0u) {
        return 0;
    }

    KboIndependentAcquisitionUiTeamMatch buyer_match =
        kbo_independent_acquisition_ui_team_match(buyer.team_id, &team_org_cache);
    KboIndependentAcquisitionQueuedRequest requests[KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE];
    memset(requests, 0, sizeof(requests));
    int request_count = kbo_independent_acquisition_load_requests(
        context.season,
        requests,
        KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE);

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || player_vector == 0u
            || player_count <= 0
            || player_count > 200000
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_independent_acquisition_player_status_ok(player)) {
            continue;
        }

        KboIndependentAcquisitionUiPlayerAssignmentSnapshot assignment;
        if (!kbo_independent_acquisition_ui_build_player_assignment_snapshot(
                player,
                &team_org_cache,
                &assignment)
                || kbo_independent_acquisition_ui_assignment_snapshot_matches_team(
                    &assignment,
                    &buyer_match)) {
            continue;
        }

        const KboIndependentAcquisitionUiSellerMatch* seller =
            kbo_independent_acquisition_ui_seller_for_player(
                &assignment,
                seller_matches,
                seller_count);
        if (seller == NULL) {
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        int32_t cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
        if (cash_cost <= 0) {
            continue;
        }
        int no_cash = buyer.cash_available < cash_cost;

        uint32_t effective_before = buyer.effective_foreign_count;
        uint32_t effective_after = buyer.effective_foreign_count;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int foreign_player = kbo_player_is_foreign_for_kbo_rights(player);
        int asian_quota = foreign_player && kbo_player_is_asian_quota_candidate(player);
        int policy_blocked = 0;
        if (foreign_player) {
            int allowed = kbo_custom_foreign_policy_team_allows_candidate(
                    buyer.team_id,
                    player,
                    &effective_before,
                    &effective_after,
                    &effective_limit,
                    &slot_type,
                    &injured_player_id);
            policy_blocked = !allowed;
        }

        KboIndependentAcquisitionUiOfferRow row;
        memset(&row, 0, sizeof(row));
        row.player_ptr = player_ptr;
        row.player_id = player_id;
        row.seller_team_id = seller->seller.team_id;
        row.seller_league_id = seller->seller.league_id;
        row.nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        row.effective_before = effective_before;
        row.effective_after = effective_after;
        row.effective_limit = effective_limit;
        row.injured_player_id = injured_player_id;
        row.age = memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))
            ? *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)
            : 0u;
        row.pitcher = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u ? 1u : 0u;
        row.foreign_player = foreign_player ? 1u : 0u;
        row.asian_quota = asian_quota ? 1u : 0u;
        row.slot_type = slot_type;
        row.offer_blocked = (no_cash || policy_blocked) ? 1u : 0u;
        row.already_requested = kbo_independent_acquisition_ui_request_row_exists(
            requests,
            request_count,
            buyer.team_id,
            seller->seller.team_id,
            player_id) ? 1u : 0u;
        row.already_decided = kbo_independent_acquisition_decision_exists(
            context.season,
            seller->seller.team_id,
            player_id) ? 1u : 0u;
        row.value_score = kbo_foreign_waiver_value_score(player);
        row.cash_cost = cash_cost;
        row.request_score = kbo_independent_acquisition_score_candidate_for_buyer(
            &buyer,
            player,
            effective_before,
            effective_limit);
        kbo_independent_acquisition_ui_slot_label(
            row.slot_type,
            row.foreign_player,
            row.asian_quota,
            row.slot_label,
            sizeof(row.slot_label));
        if (no_cash) {
            snprintf(row.status_label, sizeof(row.status_label), "자금 부족");
        } else if (policy_blocked) {
            snprintf(row.status_label, sizeof(row.status_label), "영입 불가");
        } else {
            snprintf(row.status_label, sizeof(row.status_label), "가능");
        }
        kbo_independent_acquisition_ui_insert_offer_row(out_rows, &count, max_rows, &row);
    }

    if (count > 1) {
        qsort(out_rows, (size_t)count, sizeof(out_rows[0]), kbo_independent_acquisition_ui_offer_row_cmp);
    }
    return count;
}
