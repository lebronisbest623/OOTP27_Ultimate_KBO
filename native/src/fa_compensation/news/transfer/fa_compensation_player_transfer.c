#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../fa_compensation_news_transfer.h"
#include "../fa_compensation_news_transfer_internal.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/constants/kbo_date_constants.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/sql/history_transactions/core_sql_history_transactions.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/assignment/assignment/team_assignment.h"
#include "../../../team/lookup/team_lookup.h"

static int kbo_record_fa_compensation_transfer_player_history(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    uint32_t transfer_yyyymmdd,
    const char* source)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u || transfer_yyyymmdd == 0u) {
        return 0;
    }

    uint32_t year = transfer_yyyymmdd / 10000u;
    uint32_t month = (transfer_yyyymmdd / 100u) % 100u;
    uint32_t day = transfer_yyyymmdd % 100u;
    if (year < KBO_SEASON_YEAR_MIN || month == 0u || month > 12u || day == 0u || day > 31u) {
        return 0;
    }

    char original_team_name[96] = {0};
    char signing_team_name[96] = {0};
    char history_text[512] = {0};
    kbo_fa_compensation_copy_team_history_name(
        rec->original_team_id,
        original_team_name,
        sizeof(original_team_name));
    kbo_fa_compensation_copy_team_history_name(
        rec->signing_team_id,
        signing_team_name,
        sizeof(signing_team_name));
    snprintf(
        history_text,
        sizeof(history_text),
        "[G]Joined %s from %s as the KBO FA compensation player for %s.",
        original_team_name[0] != '\0' ? original_team_name : "his new KBO organization",
        signing_team_name[0] != '\0' ? signing_team_name : "his previous KBO organization",
        rec->player_name[0] != '\0' ? rec->player_name : "the FA signing");

    int recorded = insert_kbo_player_history_sql(
        selected->player_id,
        year,
        month,
        day,
        history_text,
        source != NULL ? source : "fa_compensation_transfer");
    kbo_log_runtimef(
        "KBO FA compensation transfer player history source=%s fa_player=%u selected=%u date=%u original_team=%u signing_team=%u recorded=%d",
        source != NULL ? source : "",
        rec->player_id,
        selected->player_id,
        transfer_yyyymmdd,
        rec->original_team_id,
        rec->signing_team_id,
        recorded);
    return recorded;
}

int kbo_transfer_fa_compensation_player_to_original_team(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    uint32_t transfer_yyyymmdd,
    const char* source)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u || rec->original_team_id == 0u) {
        return 0;
    }

    uint32_t before_current_team = 0u;
    uint32_t before_current_league = 0u;
    uint8_t* player = kbo_find_player_by_id(selected->player_id, &before_current_team, &before_current_league);
    uint8_t* destination_team = find_kbo_team_by_numeric_id_any_league(rec->original_team_id, 1);
    if (player == NULL || destination_team == NULL) {
        kbo_log_runtimef(
            "KBO FA compensation transfer failed reason=lookup fa_player=%u selected=%u player=%p team=%p original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            player,
            destination_team,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    uint32_t before_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (before_current_team != rec->signing_team_id && before_active_team != rec->signing_team_id) {
        kbo_log_runtimef(
            "KBO FA compensation transfer skipped reason=player_not_on_signing_team fa_player=%u selected=%u current=%u active=%u signing_team=%u original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            before_current_team,
            before_active_team,
            rec->signing_team_id,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    int called_pre_change = 0;
    int called_register = 0;
    int called_attach = 0;
    kbo_assign_player_to_team_like_ootp(
        player,
        destination_team,
        rec->league_id,
        &called_pre_change,
        &called_register,
        &called_attach);

    uint32_t after_current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (after_current_team != rec->original_team_id && after_active_team != rec->original_team_id) {
        kbo_log_runtimef(
            "KBO FA compensation transfer failed reason=post_verify fa_player=%u selected=%u current=%u->%u active=%u->%u original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            before_current_team,
            after_current_team,
            before_active_team,
            after_active_team,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    int history_recorded = kbo_record_fa_compensation_transfer_player_history(
        rec,
        selected,
        transfer_yyyymmdd,
        "fa_compensation_transfer");

    kbo_log_runtimef(
        "KBO FA compensation player transferred fa_player=%u selected=%u name=%s signing_team=%u original_team=%u current=%u->%u active=%u->%u pre=%d register=%d attach=%d history=%d source=%s",
        rec->player_id,
        selected->player_id,
        selected->player_name,
        rec->signing_team_id,
        rec->original_team_id,
        before_current_team,
        after_current_team,
        before_active_team,
        after_active_team,
        called_pre_change,
        called_register,
        called_attach,
        history_recorded,
        source != NULL ? source : "");
    return 1;
}
