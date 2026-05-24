#ifndef KBOFIX_SRC_CUSTOM_EVENTS_SECONDARY_DRAFT_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_SECONDARY_DRAFT_H_

#include <stdint.h>

#define KBO_SECONDARY_DRAFT_UI_MAX_SEASONS 32
#define KBO_SECONDARY_DRAFT_UI_MAX_ROWS 256
#define KBO_SECONDARY_DRAFT_UI_MAX_CANDIDATES 256
#define KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT 35

typedef struct KboSecondaryDraftRunSummary {
    uint32_t season;
    uint32_t event_yyyymmdd;
    uint32_t league_id;
    int pick_count;
    int candidate_count;
    int protected_count;
    int64_t cash_total;
} KboSecondaryDraftRunSummary;

typedef struct KboSecondaryDraftResultRow {
    uint32_t season;
    uint32_t event_yyyymmdd;
    uint32_t league_id;
    uint32_t pick_no;
    uint32_t round;
    uint32_t player_id;
    uint32_t from_team_id;
    uint32_t to_team_id;
    uint32_t cash_amount;
    int cash_applied;
    int moved;
    char player_name[96];
    char from_team_name[96];
    char to_team_name[96];
} KboSecondaryDraftResultRow;

typedef struct KboSecondaryDraftCandidateRow {
    uint32_t season;
    uint32_t player_id;
    uint32_t team_id;
    uint32_t age;
    uint32_t service_days;
    int total_seasons;
    int total_seasons_known;
    int32_t value_score;
    int auto_protected;
    int saved_protected;
    int submitted;
    int already_drafted;
    int eligible;
    char player_name[96];
    char team_name[96];
    char status_label[48];
} KboSecondaryDraftCandidateRow;

typedef struct KboSecondaryDraftWindow {
    uint32_t season;
    uint32_t league_id;
    uint32_t protection_open_yyyymmdd;
    uint32_t protection_deadline_yyyymmdd;
    uint32_t draft_yyyymmdd;
} KboSecondaryDraftWindow;

int kbo_secondary_draft_ensure_schema(const char* source);
int kbo_secondary_draft_completion_valid(uint32_t league_id, uint32_t event_yyyymmdd);
int kbo_handle_secondary_draft_event(uint32_t event_yyyymmdd, const char* source);
int kbo_secondary_draft_register_window(
    uint32_t season,
    uint32_t league_id,
    uint32_t protection_open_yyyymmdd,
    uint32_t protection_deadline_yyyymmdd,
    uint32_t draft_yyyymmdd,
    const char* source);
int kbo_secondary_draft_emit_window_news(
    uint32_t announcement_yyyymmdd,
    const KboSecondaryDraftWindow* window,
    const char* source);
int kbo_secondary_draft_load_window(uint32_t season, KboSecondaryDraftWindow* out);
int kbo_secondary_draft_protection_window_open(uint32_t season);
int kbo_secondary_draft_draft_window_open(uint32_t season);
int kbo_secondary_draft_load_seasons(uint32_t* seasons, int max_seasons);
int kbo_secondary_draft_load_run_summary(uint32_t season, KboSecondaryDraftRunSummary* out);
int kbo_secondary_draft_load_result_rows(
    uint32_t season,
    KboSecondaryDraftResultRow* rows,
    int max_rows);
int kbo_secondary_draft_collect_team_list_rows(
    uint32_t season,
    uint32_t team_id,
    KboSecondaryDraftCandidateRow* rows,
    int max_rows,
    int* out_submitted,
    int* out_saved_count);
int kbo_secondary_draft_load_team_protection_status(
    uint32_t season,
    uint32_t team_id,
    int* out_saved_count,
    int* out_submitted);
int kbo_secondary_draft_collect_draft_pool_rows(
    uint32_t season,
    uint32_t drafting_team_id,
    KboSecondaryDraftCandidateRow* rows,
    int max_rows);
int kbo_secondary_draft_set_protected_player(
    uint32_t season,
    uint32_t team_id,
    uint32_t player_id,
    int protect,
    const char* source);
int kbo_secondary_draft_autofill_protected_list(
    uint32_t season,
    uint32_t team_id,
    const char* source);
int kbo_secondary_draft_submit_protected_list(
    uint32_t season,
    uint32_t team_id,
    const char* source);
int kbo_secondary_draft_manual_pick_player(
    uint32_t season,
    uint32_t drafting_team_id,
    uint32_t player_id,
    const char* source);

#endif
