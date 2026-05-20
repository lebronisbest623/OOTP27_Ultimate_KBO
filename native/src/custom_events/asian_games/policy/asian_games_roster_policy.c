#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "asian_games_roster_policy.h"
#include "../state/asian_games_state.h"

#include "../../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../../core/league_roles/kbo_league_roles.h"
#include "../../../core/policy/core_policy.h"

#define KBO_ASIAN_GAMES_ROSTER_POLICY_FILE "asian_games_roster_policy.json"
#define KBO_ASIAN_GAMES_MAX_WILDCARDS_KEY "max_wildcards"
#define KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_KEY "wildcard_age_min"
#define KBO_ASIAN_GAMES_LEGACY_WILDCARD_AGE_MAX_KEY "wildcard_age_max"

static INIT_ONCE g_kbo_asian_games_roster_policy_once = INIT_ONCE_STATIC_INIT;
static KboAsianGamesRosterPolicy g_kbo_asian_games_roster_policy;

static int32_t kbo_asian_games_policy_int(const char* key, int32_t fallback, int32_t min_value, int32_t max_value)
{
    return kbo_read_clamped_policy_int(KBO_ASIAN_GAMES_ROSTER_POLICY_FILE, key, fallback, min_value, max_value);
}

int kbo_asian_games_policy_clamp_max_wildcards(int value)
{
    if (value < KBO_ASIAN_GAMES_MAX_WILDCARDS_MIN) {
        return KBO_ASIAN_GAMES_MAX_WILDCARDS_MIN;
    }
    if (value > KBO_ASIAN_GAMES_MAX_WILDCARDS_MAX) {
        return KBO_ASIAN_GAMES_MAX_WILDCARDS_MAX;
    }
    return value;
}

int kbo_asian_games_policy_clamp_wildcard_age_min(int value)
{
    if (value < KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MIN) {
        return KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MIN;
    }
    if (value > KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MAX) {
        return KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MAX;
    }
    return value;
}

static int32_t kbo_asian_games_policy_default_wildcard_age_min(void)
{
    int32_t legacy_max = kbo_asian_games_policy_int(
        KBO_ASIAN_GAMES_LEGACY_WILDCARD_AGE_MAX_KEY,
        24,
        KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MIN,
        KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MAX);
    int32_t fallback = legacy_max < KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MAX
        ? legacy_max + 1
        : KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MAX;
    return kbo_asian_games_policy_int(
        KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_KEY,
        fallback,
        KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MIN,
        KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MAX);
}

static void kbo_asian_games_policy_string(const char* key, const char* fallback, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (!kbo_read_localappdata_named_json_string_value(KBO_ASIAN_GAMES_ROSTER_POLICY_FILE, key, out, out_size)
            && fallback != NULL) {
        snprintf(out, out_size, "%s", fallback);
    }
}

static BOOL CALLBACK kbo_asian_games_roster_policy_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboAsianGamesRosterPolicy* p = &g_kbo_asian_games_roster_policy;
    memset(p, 0, sizeof(*p));
    p->roster_size = kbo_asian_games_policy_int("roster_size", 24, 1, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->pitcher_target = kbo_asian_games_policy_int("pitcher_target", 11, 0, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->catcher_target = kbo_asian_games_policy_int("catcher_target", 2, 0, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->infielder_target = kbo_asian_games_policy_int("infielder_target", 6, 0, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->outfielder_target = kbo_asian_games_policy_int("outfielder_target", 5, 0, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->max_wildcards = kbo_asian_games_policy_int(
        KBO_ASIAN_GAMES_MAX_WILDCARDS_KEY,
        3,
        KBO_ASIAN_GAMES_MAX_WILDCARDS_MIN,
        KBO_ASIAN_GAMES_MAX_WILDCARDS_MAX);
    p->team_min_players = kbo_asian_games_policy_int("team_min_players", 1, 0, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->team_max_players = kbo_asian_games_policy_int("team_max_players", 3, 0, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->wildcard_age_min = kbo_asian_games_policy_default_wildcard_age_min();
    p->score_talent_weight = kbo_asian_games_policy_int("score_talent_weight", 45, 0, 10000);
    p->score_overall_weight = kbo_asian_games_policy_int("score_overall_weight", 30, 0, 10000);
    p->score_ratings_weight = kbo_asian_games_policy_int("score_ratings_weight", 15, 0, 10000);
    p->score_career_weight = kbo_asian_games_policy_int("score_career_weight", 10, 0, 10000);
    p->score_young_age_max = kbo_asian_games_policy_int("score_young_age_max", 24, 0, 80);
    p->score_young_bonus = kbo_asian_games_policy_int("score_young_bonus", 1200, -1000000, 1000000);
    p->score_prime_age_max = kbo_asian_games_policy_int("score_prime_age_max", 27, 0, 80);
    p->score_prime_bonus = kbo_asian_games_policy_int("score_prime_bonus", 200, -1000000, 1000000);
    p->score_age_decline_after = kbo_asian_games_policy_int("score_age_decline_after", 27, 0, 80);
    p->score_age_decline_penalty_per_year = kbo_asian_games_policy_int("score_age_decline_penalty_per_year", 120, 0, 1000000);
    p->score_non_exempt_bonus = kbo_asian_games_policy_int("score_non_exempt_bonus", 900, -1000000, 1000000);
    p->score_current_team_bonus = kbo_asian_games_policy_int("score_current_team_bonus", 150, -1000000, 1000000);
    p->main_league_for_minor_inclusion = kbo_asian_games_policy_int(
        "main_league_for_minor_inclusion",
        (int32_t)kbo_league_role_independent_league_id(),
        0,
        1000000);
    p->included_minor_league_id = kbo_asian_games_policy_int(
        "included_minor_league_id",
        (int32_t)kbo_league_role_college_league_id(),
        0,
        1000000);
    p->cross_bucket_pitcher_min = kbo_asian_games_policy_int("cross_bucket_pitcher_min", 9, 0, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->cross_bucket_catcher_min = kbo_asian_games_policy_int("cross_bucket_catcher_min", 2, 0, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->cross_bucket_infielder_min = kbo_asian_games_policy_int("cross_bucket_infielder_min", 5, 0, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->cross_bucket_outfielder_min = kbo_asian_games_policy_int("cross_bucket_outfielder_min", 4, 0, KBO_ASIAN_GAMES_ROSTER_SIZE);
    p->fallback_return_days = kbo_asian_games_policy_int("fallback_return_days", 16, 1, 365);
    p->ortools_timeout_ms = kbo_asian_games_policy_int("ortools_timeout_ms", 8000, 100, 600000);
    kbo_asian_games_policy_string("service_team_keyword_1", "Sangmu", p->service_team_keyword_1, sizeof(p->service_team_keyword_1));
    kbo_asian_games_policy_string("service_team_keyword_2", "Police", p->service_team_keyword_2, sizeof(p->service_team_keyword_2));
    kbo_asian_games_policy_string("service_team_keyword_3", "Ulsan", p->service_team_keyword_3, sizeof(p->service_team_keyword_3));
    if (p->score_prime_age_max < p->score_young_age_max) {
        p->score_prime_age_max = p->score_young_age_max;
    }
    if (p->score_age_decline_after < p->score_prime_age_max) {
        p->score_age_decline_after = p->score_prime_age_max;
    }
    if (p->team_max_players < p->team_min_players) {
        p->team_max_players = p->team_min_players;
    }
    return TRUE;
}

const KboAsianGamesRosterPolicy* kbo_asian_games_roster_policy(void)
{
    InitOnceExecuteOnce(
        &g_kbo_asian_games_roster_policy_once,
        kbo_asian_games_roster_policy_init_once,
        NULL,
        NULL);
    return &g_kbo_asian_games_roster_policy;
}

int kbo_asian_games_policy_roster_size(void)
{
    return kbo_asian_games_roster_policy()->roster_size;
}

int kbo_asian_games_policy_max_wildcards(void)
{
    return kbo_asian_games_roster_policy()->max_wildcards;
}

int kbo_set_asian_games_policy_max_wildcards(int value)
{
    int clamped = kbo_asian_games_policy_clamp_max_wildcards(value);
    if (!kbo_write_localappdata_named_json_int_value(
            KBO_ASIAN_GAMES_ROSTER_POLICY_FILE,
            KBO_ASIAN_GAMES_MAX_WILDCARDS_KEY,
            clamped)) {
        return 0;
    }
    (void)kbo_asian_games_roster_policy();
    g_kbo_asian_games_roster_policy.max_wildcards = clamped;
    return 1;
}

int kbo_asian_games_policy_wildcard_age_min(void)
{
    return kbo_asian_games_roster_policy()->wildcard_age_min;
}

int kbo_set_asian_games_policy_wildcard_age_min(int value)
{
    int clamped = kbo_asian_games_policy_clamp_wildcard_age_min(value);
    if (!kbo_write_localappdata_named_json_int_value(
            KBO_ASIAN_GAMES_ROSTER_POLICY_FILE,
            KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_KEY,
            clamped)) {
        return 0;
    }
    (void)kbo_asian_games_roster_policy();
    g_kbo_asian_games_roster_policy.wildcard_age_min = clamped;
    return 1;
}

int kbo_asian_games_policy_is_wildcard_age_with_min(uint16_t age, int wildcard_age_min)
{
    return age >= (uint16_t)kbo_asian_games_policy_clamp_wildcard_age_min(wildcard_age_min);
}

int kbo_asian_games_policy_is_wildcard_age(uint16_t age)
{
    return kbo_asian_games_policy_is_wildcard_age_with_min(
        age,
        kbo_asian_games_policy_wildcard_age_min());
}

int kbo_asian_games_policy_minor_league_included(uint32_t league_id, uint32_t* out_league_id)
{
    const KboAsianGamesRosterPolicy* policy = kbo_asian_games_roster_policy();
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (league_id != (uint32_t)policy->main_league_for_minor_inclusion
            || policy->included_minor_league_id <= 0) {
        return 0;
    }
    if (out_league_id != NULL) {
        *out_league_id = (uint32_t)policy->included_minor_league_id;
    }
    return 1;
}
