#ifndef KBOFIX_SRC_CUSTOM_EVENTS_SECONDARY_DRAFT_INTERNAL_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_SECONDARY_DRAFT_INTERNAL_H_

#include "secondary_draft.h"

#include <stddef.h>
#include <stdint.h>

#define KBO_SECONDARY_DRAFT_TEAM_OWNER_MAP_MAX 512
#define KBO_SECONDARY_DRAFT_FINANCIALS_BLOCK_OFFSET 0x2510u
#define KBO_SECONDARY_DRAFT_FINANCIALS_CASH_OFFSET 0xc0u
#define KBO_SECONDARY_DRAFT_FINANCIALS_READABLE_BYTES \
    (KBO_SECONDARY_DRAFT_FINANCIALS_CASH_OFFSET + sizeof(int32_t))
#define KBO_SECONDARY_DRAFT_FINANCIAL_FIELD_ABS_LIMIT 2000000000

typedef struct KboSecondaryDraftTeam {
    uint8_t* team;
    uint32_t team_id;
    uint32_t org_team_id;
    uint32_t league_id;
    uint32_t wins;
    uint32_t losses;
    uint32_t ties;
    uint32_t games;
    int loss_count;
    char name[96];
} KboSecondaryDraftTeam;

typedef struct KboSecondaryDraftTeamOwnerMapEntry {
    uint32_t team_id;
    int owner_index;
} KboSecondaryDraftTeamOwnerMapEntry;

typedef struct KboSecondaryDraftCandidate {
    uintptr_t player_ptr;
    uint32_t player_id;
    uint32_t owner_team_id;
    uint32_t current_team_id;
    uint32_t service_team_id;
    int owner_index;
    int32_t value_score;
    uint16_t age;
    uint16_t service_days;
    int total_seasons;
    int total_seasons_known;
    uint8_t position_role;
    uint8_t military_reserved;
    uint8_t protected_player;
    uint8_t selected;
    char player_name[96];
    char reason[64];
} KboSecondaryDraftCandidate;

int kbo_secondary_draft_sql_run_exists(uint32_t season);
int kbo_secondary_draft_sql_result_count(uint32_t season);
int kbo_secondary_draft_sql_result_player_exists(uint32_t season, uint32_t player_id);
int kbo_secondary_draft_sql_news_mark_exists(uint32_t season, const char* news_key);
int kbo_secondary_draft_sql_mark_news(uint32_t season, const char* news_key, const char* source);
int kbo_secondary_draft_sql_mark_run(
    uint32_t season,
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    int pick_count,
    int candidate_count,
    int protected_count,
    int64_t cash_total,
    const char* source);
int kbo_secondary_draft_sql_append_pick(
    uint32_t season,
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    const KboSecondaryDraftPickRow* pick,
    const char* source);
int kbo_secondary_draft_sql_protected_count(uint32_t season, uint32_t team_id);
int kbo_secondary_draft_sql_team_submitted(uint32_t season, uint32_t team_id, int* out_count);
int kbo_secondary_draft_sql_load_protected_player_ids(
    uint32_t season,
    uint32_t team_id,
    uint32_t* ids,
    int max_ids);
int kbo_secondary_draft_sql_write_protected_player(
    uint32_t season,
    uint32_t team_id,
    uint32_t player_id,
    const char* player_name,
    const char* team_name,
    const char* source);
int kbo_secondary_draft_sql_submit_team(
    uint32_t season,
    uint32_t team_id,
    const char* team_name,
    int player_count,
    const char* source);
int kbo_secondary_draft_id_list_contains(const uint32_t* ids, int count, uint32_t player_id);
int kbo_secondary_draft_load_window_for_draft_date(uint32_t draft_yyyymmdd, KboSecondaryDraftWindow* out);

void kbo_secondary_draft_copy_team_name(uint8_t* team, uint32_t team_id, char* out, size_t out_size);
int kbo_secondary_draft_collect_main_teams(
    uint32_t league_id,
    KboSecondaryDraftTeam* teams,
    int max_teams);
int kbo_secondary_draft_team_order_cmp(const void* left, const void* right);
int kbo_secondary_draft_team_index_by_id(
    const KboSecondaryDraftTeam* teams,
    int team_count,
    uint32_t team_id);
int kbo_secondary_draft_build_team_owner_map(
    const KboSecondaryDraftTeam* teams,
    int team_count,
    KboSecondaryDraftTeamOwnerMapEntry* entries,
    int max_entries);
int kbo_secondary_draft_owner_index_for_player_from_map(
    uint8_t* player,
    const KboSecondaryDraftTeamOwnerMapEntry* entries,
    int entry_count,
    uint32_t* out_owner_team_id,
    uint32_t* out_service_team_id,
    int* out_military_reserved,
    int* out_non_military_loan);
int kbo_secondary_draft_collect_candidates(
    uint32_t season,
    const KboSecondaryDraftTeam* teams,
    int team_count,
    KboSecondaryDraftCandidate* candidates,
    int max_candidates);
int kbo_secondary_draft_auto_submit_missing_protection_lists(
    uint32_t season,
    const KboSecondaryDraftTeam* teams,
    int team_count,
    KboSecondaryDraftCandidate* candidates,
    int candidate_count,
    const char* source);
int kbo_secondary_draft_mark_protected_players(
    KboSecondaryDraftCandidate* candidates,
    int candidate_count,
    int team_count,
    uint32_t season);
int kbo_secondary_draft_best_candidate_for_team(
    KboSecondaryDraftCandidate* candidates,
    int candidate_count,
    const KboSecondaryDraftTeam* teams,
    int drafting_team_index);
int kbo_secondary_draft_apply_pick(
    KboSecondaryDraftCandidate* candidate,
    KboSecondaryDraftTeam* from_team,
    KboSecondaryDraftTeam* to_team,
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    uint32_t round,
    uint32_t pick_no,
    KboSecondaryDraftPickRow* out_pick);
int kbo_secondary_draft_emit_results_news(
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    const KboSecondaryDraftPickRow* picks,
    int pick_count,
    int candidate_count,
    int protected_count,
    int64_t cash_total,
    const char* source);

#endif
