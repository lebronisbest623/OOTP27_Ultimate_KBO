#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "secondary_draft_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/logging/core_log.h"
#include "../../core/sql/history_transactions/core_sql_history_transactions.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/assignment/assignment/team_assignment.h"
#include "../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../team/lookup/team_lookup.h"

static uint32_t kbo_secondary_draft_cash_for_round(uint32_t round)
{
    if (round == 1u) {
        return 400000000u;
    }
    if (round == 2u) {
        return 300000000u;
    }
    if (round == 3u) {
        return 200000000u;
    }
    return 100000000u;
}

static int32_t* kbo_secondary_draft_team_cash_ptr(uint8_t* team)
{
    if (team == NULL
            || !memory_range_readable(
                team + KBO_SECONDARY_DRAFT_FINANCIALS_BLOCK_OFFSET,
                KBO_SECONDARY_DRAFT_FINANCIALS_READABLE_BYTES)) {
        return NULL;
    }
    int32_t* cash = (int32_t*)(
        team
        + KBO_SECONDARY_DRAFT_FINANCIALS_BLOCK_OFFSET
        + KBO_SECONDARY_DRAFT_FINANCIALS_CASH_OFFSET);
    if (*cash <= -KBO_SECONDARY_DRAFT_FINANCIAL_FIELD_ABS_LIMIT
            || *cash >= KBO_SECONDARY_DRAFT_FINANCIAL_FIELD_ABS_LIMIT) {
        return NULL;
    }
    return cash;
}

static int kbo_secondary_draft_apply_cash_transfer(
    uint8_t* buyer_team,
    uint8_t* seller_team,
    uint32_t amount)
{
    if (amount == 0u || amount > (uint32_t)INT_MAX) {
        return 0;
    }
    int32_t cash_amount = (int32_t)amount;
    int32_t* buyer_cash = kbo_secondary_draft_team_cash_ptr(buyer_team);
    int32_t* seller_cash = kbo_secondary_draft_team_cash_ptr(seller_team);
    if (buyer_cash == NULL || seller_cash == NULL || *buyer_cash < cash_amount) {
        return 0;
    }
    int64_t seller_new_cash64 = (int64_t)(*seller_cash) + (int64_t)cash_amount;
    if (seller_new_cash64 <= -KBO_SECONDARY_DRAFT_FINANCIAL_FIELD_ABS_LIMIT
            || seller_new_cash64 >= KBO_SECONDARY_DRAFT_FINANCIAL_FIELD_ABS_LIMIT) {
        return 0;
    }
    *buyer_cash -= cash_amount;
    *seller_cash = (int32_t)seller_new_cash64;
    return 1;
}

static int kbo_secondary_draft_record_player_history(
    const KboSecondaryDraftPick* pick,
    uint32_t event_yyyymmdd)
{
    if (pick == NULL || pick->player_id == 0u || event_yyyymmdd == 0u) {
        return 0;
    }
    char history_text[384] = {0};
    snprintf(
        history_text,
        sizeof(history_text),
        "[G]Selected by %s from %s with pick %u in round %u of the KBO secondary draft.",
        pick->to_team_name,
        pick->from_team_name,
        pick->pick_no,
        pick->round);
    return insert_kbo_player_history_sql(
        pick->player_id,
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        history_text,
        "secondary_draft");
}

int kbo_secondary_draft_apply_pick(
    KboSecondaryDraftCandidate* candidate,
    KboSecondaryDraftTeam* from_team,
    KboSecondaryDraftTeam* to_team,
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    uint32_t round,
    uint32_t pick_no,
    KboSecondaryDraftPick* out_pick)
{
    if (out_pick != NULL) {
        memset(out_pick, 0, sizeof(*out_pick));
    }
    if (candidate == NULL || from_team == NULL || to_team == NULL || out_pick == NULL) {
        return 0;
    }
    if (!kbo_player_pointer_plausible(candidate->player_ptr)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)candidate->player_ptr;
    if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) != candidate->player_id
            || !kbo_secondary_draft_player_status_ok(player)) {
        return 0;
    }
    if (kbo_secondary_draft_owner_index_for_player(player, from_team, 1) < 0
            && !kbo_player_current_assignment_matches_team_or_affiliate(player, from_team->team_id)) {
        return 0;
    }

    uint8_t* to_team_ptr = to_team->team != NULL
        ? to_team->team
        : find_kbo_team_by_numeric_id_any_league(to_team->team_id, 1);
    uint8_t* from_team_ptr = from_team->team != NULL
        ? from_team->team
        : find_kbo_team_by_numeric_id_any_league(from_team->team_id, 1);
    if (to_team_ptr == NULL || !memory_range_readable(to_team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    int pre = 0;
    int reg = 0;
    int attach = 0;
    kbo_assign_player_to_team_like_ootp(player, to_team_ptr, league_id, &pre, &reg, &attach);
    int moved = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == to_team->team_id;
    if (!moved) {
        kbo_log_runtimef(
            "KBO secondary draft pick failed reason=assignment_not_applied player=%u from=%u to=%u round=%u pick=%u",
            candidate->player_id,
            from_team->team_id,
            to_team->team_id,
            round,
            pick_no);
        return 0;
    }

    uint32_t cash_amount = kbo_secondary_draft_cash_for_round(round);
    int cash_applied = kbo_secondary_draft_apply_cash_transfer(to_team_ptr, from_team_ptr, cash_amount);

    out_pick->round = round;
    out_pick->pick_no = pick_no;
    out_pick->player_id = candidate->player_id;
    out_pick->from_team_id = from_team->team_id;
    out_pick->to_team_id = to_team->team_id;
    out_pick->cash_amount = cash_amount;
    out_pick->moved = moved;
    out_pick->cash_applied = cash_applied;
    snprintf(out_pick->player_name, sizeof(out_pick->player_name), "%s", candidate->player_name);
    snprintf(out_pick->from_team_name, sizeof(out_pick->from_team_name), "%s", from_team->name);
    snprintf(out_pick->to_team_name, sizeof(out_pick->to_team_name), "%s", to_team->name);

    int history_inserted = kbo_secondary_draft_record_player_history(out_pick, event_yyyymmdd);
    kbo_log_runtimef(
        "KBO secondary draft pick player=%u name=%s from=%u to=%u round=%u pick=%u value=%d cash=%u cash_applied=%d history=%d pre=%d reg=%d attach=%d",
        out_pick->player_id,
        out_pick->player_name,
        out_pick->from_team_id,
        out_pick->to_team_id,
        out_pick->round,
        out_pick->pick_no,
        candidate->value_score,
        out_pick->cash_amount,
        out_pick->cash_applied,
        history_inserted,
        pre,
        reg,
        attach);
    return 1;
}
