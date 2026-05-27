#ifndef KBOFIX_SRC_CUSTOM_EVENTS_SECONDARY_DRAFT_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_SECONDARY_DRAFT_H_

#include <stddef.h>
#include <stdint.h>

#define KBO_SECONDARY_DRAFT_TEAM_MAX 16
#define KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT 35
#define KBO_SECONDARY_DRAFT_ROUNDS 5
#define KBO_SECONDARY_DRAFT_BASE_ROUNDS 3
#define KBO_SECONDARY_DRAFT_EXTRA_TEAMS 3
#define KBO_SECONDARY_DRAFT_SOURCE_LOSS_LIMIT 4
#define KBO_SECONDARY_DRAFT_CANDIDATE_MAX 8192
#define KBO_SECONDARY_DRAFT_RESULT_MAX (KBO_SECONDARY_DRAFT_TEAM_MAX * KBO_SECONDARY_DRAFT_ROUNDS)
#define KBO_SECONDARY_DRAFT_SERVICE_DAYS_PER_SEASON 145u

typedef struct KboSecondaryDraftWindow {
    uint32_t season;
    uint32_t league_id;
    uint32_t protection_open_yyyymmdd;
    uint32_t protection_deadline_yyyymmdd;
    uint32_t draft_yyyymmdd;
} KboSecondaryDraftWindow;

typedef struct KboSecondaryDraftEligibilityInput {
    uint32_t player_id;
    uint32_t owner_team_id;
    uint16_t age;
    uint16_t service_days;
    int total_seasons;
    int total_seasons_known;
    int foreign_player;
    int retired;
    int dfa;
    int draft_pool;
    int has_evaluation;
    int current_year_fa;
    int military_reserved;
    int military_history;
    int non_military_loan;
} KboSecondaryDraftEligibilityInput;

typedef struct KboSecondaryDraftEligibilityDecision {
    int eligible;
    int auto_excluded;
    int inferred_total_seasons;
    char reason[64];
} KboSecondaryDraftEligibilityDecision;

typedef struct KboSecondaryDraftRunSummary {
    uint32_t season;
    uint32_t event_yyyymmdd;
    uint32_t league_id;
    int pick_count;
    int candidate_count;
    int protected_count;
    int64_t cash_total;
} KboSecondaryDraftRunSummary;

typedef struct KboSecondaryDraftPickRow {
    uint32_t round;
    uint32_t pick_no;
    uint32_t player_id;
    uint32_t from_team_id;
    uint32_t to_team_id;
    uint32_t cash_amount;
    int moved;
    int cash_applied;
    int military_rights_transfer;
    char player_name[96];
    char from_team_name[96];
    char to_team_name[96];
} KboSecondaryDraftPickRow;

int kbo_secondary_draft_is_odd_season(uint32_t season_or_yyyymmdd);
int kbo_secondary_draft_round_team_limit(int team_count, uint32_t round);
int kbo_secondary_draft_expected_pick_count(int team_count);
uint32_t kbo_secondary_draft_cash_for_round(uint32_t round);
int kbo_secondary_draft_source_loss_allows(int loss_count);
int kbo_secondary_draft_evaluate_eligibility(
    const KboSecondaryDraftEligibilityInput* input,
    KboSecondaryDraftEligibilityDecision* out);

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

#endif
