#ifndef KBOFIX_SRC_ASIAN_GAMES_ROSTER_POLICY_H_
#define KBOFIX_SRC_ASIAN_GAMES_ROSTER_POLICY_H_

#include <stdint.h>

#include "../state/asian_games_state.h"

typedef struct KboAsianGamesRosterPolicy {
    int32_t roster_size;
    int32_t pitcher_target;
    int32_t catcher_target;
    int32_t infielder_target;
    int32_t outfielder_target;
    int32_t max_wildcards;
    int32_t team_min_players;
    int32_t team_max_players;
    int32_t wildcard_age_min;
    int32_t score_talent_weight;
    int32_t score_overall_weight;
    int32_t score_ratings_weight;
    int32_t score_career_weight;
    int32_t score_young_age_max;
    int32_t score_young_bonus;
    int32_t score_prime_age_max;
    int32_t score_prime_bonus;
    int32_t score_age_decline_after;
    int32_t score_age_decline_penalty_per_year;
    int32_t score_non_exempt_bonus;
    int32_t score_current_team_bonus;
    int32_t main_league_for_minor_inclusion;
    int32_t included_minor_league_id;
    int32_t cross_bucket_pitcher_min;
    int32_t cross_bucket_catcher_min;
    int32_t cross_bucket_infielder_min;
    int32_t cross_bucket_outfielder_min;
    int32_t fallback_return_days;
    int32_t ortools_timeout_ms;
    char service_team_keyword_1[32];
    char service_team_keyword_2[32];
    char service_team_keyword_3[32];
} KboAsianGamesRosterPolicy;

#define KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MIN 0
#define KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MAX 80
#define KBO_ASIAN_GAMES_MAX_WILDCARDS_MIN 0
#define KBO_ASIAN_GAMES_MAX_WILDCARDS_MAX KBO_ASIAN_GAMES_ROSTER_SIZE

const KboAsianGamesRosterPolicy* kbo_asian_games_roster_policy(void);
int kbo_asian_games_policy_roster_size(void);
int kbo_asian_games_policy_max_wildcards(void);
int kbo_asian_games_policy_clamp_max_wildcards(int value);
int kbo_set_asian_games_policy_max_wildcards(int value);
int kbo_asian_games_policy_wildcard_age_min(void);
int kbo_asian_games_policy_clamp_wildcard_age_min(int value);
int kbo_set_asian_games_policy_wildcard_age_min(int value);
int kbo_asian_games_policy_is_wildcard_age_with_min(uint16_t age, int wildcard_age_min);
int kbo_asian_games_policy_is_wildcard_age(uint16_t age);
int kbo_asian_games_policy_minor_league_included(uint32_t league_id, uint32_t* out_league_id);

#endif
