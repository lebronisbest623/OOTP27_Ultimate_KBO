#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_seller_transfer.h"

#include "../../../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../../../core/logging/core_log.h"
#include "../../../../../../runtime_memory/runtime_memory.h"
#include "../../../../../assignment/assignment/team_assignment.h"
#include "../../../../../assignment/org_query/team_org_assignment_query.h"
#include "../../../../../lookup/team_lookup.h"

int kbo_independent_acquisition_seller_apply_transfer(
    uint32_t today,
    KboIndependentAcquisitionQueuedRequest* selected,
    uint8_t* player,
    int seller_limit_reached,
    int pacing_blocked,
    const char* source,
    int* out_cash_charged,
    int32_t* out_old_cash,
    int32_t* out_new_cash,
    int* out_seller_cash_credited,
    int32_t* out_seller_old_cash,
    int32_t* out_seller_new_cash,
    int32_t* out_seller_transfer_fee,
    int32_t* out_cash_cost)
{
    if (out_cash_charged != NULL) { *out_cash_charged = 0; }
    if (out_old_cash != NULL) { *out_old_cash = 0; }
    if (out_new_cash != NULL) { *out_new_cash = 0; }
    if (out_seller_cash_credited != NULL) { *out_seller_cash_credited = 0; }
    if (out_seller_old_cash != NULL) { *out_seller_old_cash = 0; }
    if (out_seller_new_cash != NULL) { *out_seller_new_cash = 0; }
    if (out_seller_transfer_fee != NULL) { *out_seller_transfer_fee = 0; }
    if (out_cash_cost != NULL) { *out_cash_cost = 0; }
    if (selected == NULL) {
        return 0;
    }

    uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(selected->buyer_team_id, 1);
    uint8_t* seller_team = find_kbo_team_by_numeric_id_any_league(selected->seller_team_id, 1);
    int moved = 0;
    int cash_charged = 0;
    int32_t old_cash = 0;
    int32_t new_cash = 0;
    int seller_cash_credited = 0;
    int32_t seller_old_cash = 0;
    int32_t seller_new_cash = 0;
    int32_t seller_transfer_fee = 0;
    int32_t cash_cost = selected->cash_cost;
    if (cash_cost <= 0 && player != NULL) {
        cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
        selected->cash_cost = cash_cost;
    }
    if (player != NULL) {
        seller_transfer_fee = kbo_independent_acquisition_seller_transfer_fee_for_player(player);
    }

    if (!seller_limit_reached
            && !pacing_blocked
            && player != NULL
            && buyer_team != NULL
            && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            && memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            && kbo_independent_acquisition_player_status_ok(player)
            && kbo_player_current_assignment_matches_team_or_affiliate(player, selected->seller_team_id)
            && !kbo_player_current_assignment_matches_team_or_affiliate(player, selected->buyer_team_id)
            && kbo_independent_acquisition_team_has_cash(buyer_team, cash_cost)) {
        int pre = 0;
        int reg = 0;
        int attach = 0;
        uint32_t buyer_league_id = *(uint32_t*)(buyer_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        kbo_assign_player_to_team_like_ootp(player, buyer_team, buyer_league_id, &pre, &reg, &attach);
        moved = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == selected->buyer_team_id;
        if (moved) {
            cash_charged = kbo_independent_acquisition_charge_team_cash(
                    buyer_team,
                    cash_cost,
                    &old_cash,
                    &new_cash);
            if (!cash_charged) {
                kbo_log_runtimef(
                    "independent acquisition seller AI cash charge failed source=%s buyer=%u player=%u cost=%d",
                    source != NULL ? source : "",
                    selected->buyer_team_id,
                    selected->player_id,
                    cash_cost);
            }
            if (cash_charged && seller_team != NULL && seller_transfer_fee > 0) {
                seller_cash_credited = kbo_independent_acquisition_credit_team_cash(
                    seller_team,
                    seller_transfer_fee,
                    &seller_old_cash,
                    &seller_new_cash);
                if (!seller_cash_credited) {
                    kbo_log_runtimef(
                        "independent acquisition seller AI seller cash credit failed source=%s seller=%u player=%u transfer_fee=%d",
                        source != NULL ? source : "",
                        selected->seller_team_id,
                        selected->player_id,
                        seller_transfer_fee);
                }
            }
        }
    }
    if (moved && cash_charged) {
        kbo_emit_independent_acquisition_transfer_news(
            today,
            player,
            buyer_team,
            seller_team,
            selected->player_id,
            selected->buyer_team_id,
            selected->seller_team_id,
            cash_cost,
            source);
    }

    if (out_cash_charged != NULL) { *out_cash_charged = cash_charged; }
    if (out_old_cash != NULL) { *out_old_cash = old_cash; }
    if (out_new_cash != NULL) { *out_new_cash = new_cash; }
    if (out_seller_cash_credited != NULL) { *out_seller_cash_credited = seller_cash_credited; }
    if (out_seller_old_cash != NULL) { *out_seller_old_cash = seller_old_cash; }
    if (out_seller_new_cash != NULL) { *out_seller_new_cash = seller_new_cash; }
    if (out_seller_transfer_fee != NULL) { *out_seller_transfer_fee = seller_transfer_fee; }
    if (out_cash_cost != NULL) { *out_cash_cost = cash_cost; }
    return moved;
}
