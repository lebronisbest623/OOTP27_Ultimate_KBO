#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/logging/core_log.h"
#include "../../core/sql/history_transactions/core_sql_history_transactions.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/assignment/assignment/team_assignment.h"
#include "../../team/lookup/team_lookup.h"
#include "../players/loans/military_active_loan.h"
#include "../players/loans/military_native_loan.h"
#include "../players/state/military_player_state.h"
#include "military_return.h"
#include "history/military_return_history.h"
#include "../selection/news/military_selection_news.h"

static LONG g_military_loan_return_log_count = 0;

int kbo_return_completed_military_loan_player(
    uint8_t* player,
    const char* source,
    uint32_t vector_offset,
    int require_registered)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int active_index = find_active_kbo_military_loan_index(player_id);
    if (require_registered && active_index < 0) {
        return 0;
    }

    uint32_t sang_id = 0;
    uint32_t kpb_id  = 0;
    uint32_t registered_service_team_id  = 0;
    uint32_t registered_original_team_id = 0;
    uint32_t registered_original_league_id = 0;

    if (active_index >= 0) {
        KboMilitaryActiveLoan* active = kbo_active_military_loan_at(active_index);
        if (active != NULL) {
            active->player_ptr              = (uintptr_t)player;
            registered_service_team_id      = active->service_team_id;
            registered_original_team_id     = active->original_team_id;
            registered_original_league_id   = active->original_league_id;
        } else if (require_registered) {
            return 0;
        }
    }
    if (registered_service_team_id == 0u) {
        uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
        uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
        sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
        kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    uint32_t service_team_id = 0;
    if (registered_service_team_id != 0) {
        service_team_id = registered_service_team_id;
    } else if (current_team_id != 0
               && (current_team_id == sang_id || current_team_id == kpb_id)) {
        service_team_id = current_team_id;
    } else if (loan_team_id != 0
               && (loan_team_id == sang_id || loan_team_id == kpb_id)) {
        service_team_id = loan_team_id;
    } else {
        return 0;
    }

    uint32_t original_team_id = 0u;
    uint32_t original_league_id = 0u;
    if (!kbo_military_resolve_original_team(
            player,
            service_team_id,
            sang_id,
            kpb_id,
            &original_team_id,
            &original_league_id)) {
        original_team_id = registered_original_team_id;
        original_league_id = registered_original_league_id;
    }
    if (original_league_id == 0u) {
        original_league_id = registered_original_league_id != 0u
            ? registered_original_league_id
            : *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    }
    if (original_team_id == 0u
            || original_team_id == service_team_id
            || original_team_id == sang_id
            || original_team_id == kpb_id) {
        return 0;
    }
    kbo_military_repair_original_team_memory(
        player,
        original_team_id,
        original_league_id,
        service_team_id,
        sang_id,
        kpb_id);

    int32_t days_left = kbo_military_effective_days_left(player);
    if (days_left > 0) {
        kbo_clear_military_unavailable_flags(player);
        return 0;
    }

    uint8_t* original_team = find_kbo_team_by_numeric_id_any_league(original_team_id, 0);
    if (original_team == NULL) {
        return 0;
    }

    int called_pre_change = 0;
    int called_register   = 0;
    int called_attach     = 0;
    int native_loan_before  = kbo_player_native_on_loan(player);
    int native_loan_cleared = native_loan_before ? kbo_clear_native_player_loan(player) : 0;
    uint32_t fallback_league_id = original_league_id != 0
        ? original_league_id
        : *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);

    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != original_team_id) {
        kbo_assign_player_to_team_like_ootp(
            player, original_team, fallback_league_id,
            &called_pre_change, &called_register, &called_attach);
    }

    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET)   = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0;
    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET]   = 0;
    player[OOTP27_PLAYER_LOAN_CLEARED_MARKER_OFFSET] = 1;
    player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET]    = 0;
    complete_kbo_military_service_status(player);
    unregister_active_kbo_military_loan(player_id);

    uint8_t old_restricted     = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t old_secondary      = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t old_injury_active  = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    uint8_t old_mil_exempt     = player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET];
    uint8_t old_mil_active     = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];

    uint32_t year = 0;
    uint32_t month = 1;
    uint32_t day = 1;
    uint32_t current_yyyymmdd = 0u;
    if (kbo_current_date_tick_latest_published_date(&current_yyyymmdd)) {
        year = current_yyyymmdd / 10000u;
        month = (current_yyyymmdd / 100u) % 100u;
        day = current_yyyymmdd % 100u;
    } else {
        year = 2001;
    }
    char service_team_name[96] = {0};
    char original_team_name[96] = {0};
    char history_text[256] = {0};
    uint8_t* service_team = find_kbo_team_by_numeric_id_any_league(service_team_id, 0);
    kbo_military_copy_team_history_name(
        service_team,
        service_team_name,
        sizeof(service_team_name),
        service_team_id == sang_id ? "Sangmu Baseball Team" : "Korean Police Baseball Team");
    kbo_military_copy_team_history_name(
        original_team,
        original_team_name,
        sizeof(original_team_name),
        "his original KBO organization");
    snprintf(
        history_text,
        sizeof(history_text),
        "Completed military service with %s and returned to %s.",
        service_team_name,
        original_team_name);
    uint32_t history_yyyymmdd = year * 10000u + month * 100u + day;
    int history_inserted = 0;
    int history_skipped_duplicate = 0;
    if (kbo_mark_military_return_history_once(player_id, history_yyyymmdd)) {
        history_inserted = insert_kbo_player_history_sql(
            player_id,
            year,
            month,
            day,
            history_text,
            "military_service_return");
    } else {
        history_skipped_duplicate = 1;
    }

    LONG log_index = InterlockedIncrement(&g_military_loan_return_log_count);
    if (log_index <= 120) {
        kbo_log_runtimef(
            "KBO military assignment returned #%ld source=%s player_id=%u"
            " service_team=%u original_team=%u"
            " current_team=%u current_league=%u org_team=%u days_left=%d"
            " native_loan_before=%d native_loan_cleared=%d history_inserted=%d history_duplicate=%d"
            " old_restricted=%u old_secondary=%u old_injury=%u"
            " old_mil_exempt=%u mil_exempt=%u old_mil_active=%u mil_active=%u"
            " pre_change=%d register=%d attach=%d player=%p vector_off=0x%x",
            log_index,
            source != NULL ? source : "",
            player_id,
            service_team_id,
            original_team_id,
            *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
            *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
            *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
            days_left,
            native_loan_before,
            native_loan_cleared,
            history_inserted,
            history_skipped_duplicate,
            (uint32_t)old_restricted,
            (uint32_t)old_secondary,
            (uint32_t)old_injury_active,
            (uint32_t)old_mil_exempt,
            (uint32_t)player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET],
            (uint32_t)old_mil_active,
            (uint32_t)player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET],
            called_pre_change,
            called_register,
            called_attach,
            player,
            vector_offset);
    }

    return 1;
}
