#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define KBO_FOREIGN_INJURY_SLOT_REGULAR 1
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA 2

#include "../src/core/dates/core_text_date.h"
#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/core/season/phase/season_phase.h"
#include "../src/core/core_flags/json/json_bool_parser.h"
#include "../src/core/news/templates/core_news_templates.h"
#include "../src/core/news/links/core_news_links.h"
#include "../src/core/csv/core_csv.h"
#include "../src/core/sql/escape/core_sql_escape.h"
#include "../src/core/dates/tick/current_date_tick_capture.h"
#include "../src/foreign/replacement_seed/parse/foreign_replacement_seed_parse.h"
#include "../src/captain/season/captain_season.h"
#include "../src/captain/seed/parse/captain_seed_parse.h"
#include "../src/patch_helpers/patch_helpers.h"
#include "../src/awards/schedule/award_schedule_probe_module.h"
#include "../src/core/core_flags/api/flags_api.h"

static uint32_t g_test_current_yyyymmdd = 0u;
static char g_test_current_save_path[MAX_PATH] = {0};

int kbo_current_date_is_valid(uint32_t* out_year, uint32_t* out_month, uint32_t* out_day)
{
    if (out_year != NULL) {
        *out_year = 0u;
    }
    if (out_month != NULL) {
        *out_month = 0u;
    }
    if (out_day != NULL) {
        *out_day = 0u;
    }
    return 0;
}

int kbo_get_current_yyyymmdd(uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = g_test_current_yyyymmdd;
    }
    return g_test_current_yyyymmdd != 0u;
}

int kbo_get_current_save_path(char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    if (out == NULL || out_size == 0u || g_test_current_save_path[0] == '\0') {
        return 0;
    }
    snprintf(out, out_size, "%s", g_test_current_save_path);
    return out[0] != '\0';
}

#include "../src/military_service/calendar/military_service_date.h"
#include "../src/military_service/seed/parse/military_service_seed_parse.h"
#include "../src/military_service/players/team_policy/military_service_team_policy_parse.h"
#include "../src/team/classification/parse/team_classification_seed_parse.h"
#include "../src/team/names/team_string.h"
#include "../src/allstar/csv/allstar_csv_parse.h"
#include "../src/allstar/allstar_native_events/schedule/schedule_dates.h"
#include "../src/foreign/common/dates/foreign_waiver_date.h"
#include "../src/core/core_flags/keys/flag_key.h"
#include "../src/fa_filing/fa_filing_parts/fa_filing_csv_parse.h"
#include "../src/fa_salary_snapshot/csv/salary_snapshot_csv_parse.h"
#include "../src/fa_market_classification/internal/fa_market_policy_internal.h"
#include "../src/core/logging/rule_audit.h"
#include "../src/core/files/atomic/core_atomic_file.h"
#include "../src/custom_events/asian_games/policy/asian_games_roster_policy.h"
#include "../src/military_service/players/loans/military_native_loan.h"
#include "../src/team/assignment/roster_arrays/team_roster_arrays.h"
#include "../src/team/assignment/org_query/team_org_assignment_query.h"
#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../src/foreign/injury/api/foreign_injury.h"
#include "../src/foreign/quota/counts/org/foreign_quota_counts_internal.h"
#include "../src/amateur_player_quality/api/amateur_player_quality.h"
#include "../src/amateur_player_quality/assignment/policy/amateur_assignment_policy_values.h"
#include "../src/team/independent_acquisition/ai/independent_acquisition_score.h"
#include "../src/hotkey_window/support/assets/nations/ui_nation_table.h"

int kbo_amateur_player_is_hitter(uint8_t* player);
uint32_t kbo_amateur_player_assignment_league_id(uint8_t* player);
uint32_t kbo_amateur_player_assignment_team_id(uint8_t* player);
int kbo_amateur_player_position_bucket(uint8_t* player);
const char* kbo_amateur_position_bucket_label(int bucket);
int32_t kbo_amateur_assignment_target_max_players(uint32_t league_id);
int32_t kbo_amateur_quality_score(uint8_t* player);
int32_t kbo_amateur_assignment_target_reputation(uint32_t league_id, int32_t quality_score);
int kbo_amateur_assignment_team_tier(uint32_t league_id, uint8_t reputation);
int kbo_amateur_assignment_player_tier(uint32_t league_id, int32_t quality_score);
int kbo_amateur_assignment_tier_allowed(int player_tier, int team_tier);
int kbo_amateur_assignment_effective_player_tier(int player_tier, int max_team_tier);
void kbo_apply_amateur_reputation_balanced_deltas(
    uint32_t league_id,
    KboAmateurReputationUpdateRow* rows,
    int row_count,
    int32_t* out_raw_delta_sum,
    int32_t* out_balance_adjustment,
    int32_t* out_final_delta_sum);

static uint32_t g_test_fa_market_team_leagues[512];
static int g_test_fa_market_team_independent_kinds[512];
static uint8_t g_test_team_8[OOTP27_KBO_TEAM_READABLE_BYTES];
static uint8_t g_test_team_11[OOTP27_KBO_TEAM_READABLE_BYTES];
static int g_test_team_lookup_enabled = 0;
static uintptr_t g_test_player_vector_storage[16];
static int32_t g_test_player_vector_count = 0;
static int g_test_player_vector_enabled = 0;
static int g_test_player_vector_lookup_count = 0;
static const uint32_t TEST_COLLEGE_LEAGUE_ID = 201u;
static const uint32_t TEST_HIGH_SCHOOL_LEAGUE_ID = 203u;
static const uint32_t TEST_INDEPENDENT_LEAGUE_ID = 200u;

static void test_configure_team_org_lookup(void)
{
    memset(g_test_team_8, 0, sizeof(g_test_team_8));
    memset(g_test_team_11, 0, sizeof(g_test_team_11));
    *(uint32_t*)(g_test_team_8 + OOTP27_KBO_TEAM_ID_OFFSET) = 8u;
    *(uint32_t*)(g_test_team_8 + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(g_test_team_11 + OOTP27_KBO_TEAM_ID_OFFSET) = 11u;
    *(uint32_t*)(g_test_team_11 + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET) = 8u;
    g_test_team_lookup_enabled = 1;
}

uint8_t* find_kbo_team_by_numeric_id_any_league(uint32_t team_id, int allow_deleted)
{
    (void)allow_deleted;
    if (!g_test_team_lookup_enabled) {
        return NULL;
    }
    if (team_id == 8u) {
        return g_test_team_8;
    }
    if (team_id == 11u) {
        return g_test_team_11;
    }
    return NULL;
}

const KboFaMarketPolicy* kbo_fa_market_policy(void)
{
    static const KboFaMarketPolicy policy = {
        .undrafted_college_league_id = TEST_COLLEGE_LEAGUE_ID,
        .undrafted_college_draft_subtype = 1,
        .undrafted_college_age_max = 25,
        .undrafted_high_school_league_id = TEST_HIGH_SCHOOL_LEAGUE_ID,
        .undrafted_high_school_age_max = 20,
        .independent_league_id = TEST_INDEPENDENT_LEAGUE_ID,
        .player_age_min = 16,
        .player_age_max = 60
    };
    return &policy;
}

uint32_t kbo_fa_market_get_team_league_id(uint32_t team_id)
{
    return team_id < (uint32_t)(sizeof(g_test_fa_market_team_leagues) / sizeof(g_test_fa_market_team_leagues[0]))
        ? g_test_fa_market_team_leagues[team_id]
        : 0u;
}

int kbo_team_classification_independent_kind_for_team(uint32_t team_id)
{
    return team_id < (uint32_t)(sizeof(g_test_fa_market_team_independent_kinds) / sizeof(g_test_fa_market_team_independent_kinds[0]))
        ? g_test_fa_market_team_independent_kinds[team_id]
        : KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE;
}

int kbo_fa_rules_load(KboFaRules* rules)
{
    if (rules != NULL) {
        memset(rules, 0, sizeof(*rules));
    }
    return 0;
}

int kbo_fa_rules_case_is_compensable(const KboFaRules* rules, const char* case_label)
{
    (void)rules;
    (void)case_label;
    return 0;
}

int kbo_fa_rules_grade_is_compensable(const KboFaRules* rules, const char* grade)
{
    (void)rules;
    (void)grade;
    return 0;
}

void kbo_fa_rules_calculate_compensation(
    const KboFaRules* rules,
    const char* grade,
    int32_t previous_salary,
    uint32_t* out_cash_with_player,
    uint32_t* out_cash_only,
    uint32_t* out_protect_count,
    uint8_t* out_requires_player)
{
    (void)rules;
    (void)grade;
    (void)previous_salary;
    if (out_cash_with_player != NULL) { *out_cash_with_player = 0u; }
    if (out_cash_only != NULL) { *out_cash_only = 0u; }
    if (out_protect_count != NULL) { *out_protect_count = 0u; }
    if (out_requires_player != NULL) { *out_requires_player = 0u; }
}

const KboFaSalarySnapshotGrade* kbo_find_fa_salary_snapshot_grade(
    const KboFaSalarySnapshotGrade* rows,
    int row_count,
    uint32_t player_id)
{
    (void)rows;
    (void)row_count;
    (void)player_id;
    return NULL;
}

static void test_core_text_and_sql_helpers(void)
{
    char date[9] = {0};
    char escaped[32] = {0};

    assert(ascii_equals_ignore_case("KBO", "kbo"));
    assert(!ascii_equals_ignore_case("KBO", "KB"));
    assert(!ascii_equals_ignore_case(NULL, "kbo"));

    assert(kbo_format_history_date(date, sizeof(date), 2024u, 2u, 29u));
    assert(strcmp(date, "20240229") == 0);
    assert(!kbo_format_history_date(date, sizeof(date), 2023u, 2u, 29u));
    assert(!kbo_format_history_date(date, 8u, 2024u, 1u, 1u));
    assert(!kbo_format_history_date(NULL, sizeof(date), 2024u, 1u, 1u));
    assert(!kbo_format_history_date(date, sizeof(date), 1799u, 12u, 31u));

    assert(kbo_sql_escape_literal(escaped, sizeof(escaped), "Kim's\tLine\nDrop"));
    assert(strcmp(escaped, "Kim''s\tLineDrop") == 0);
    assert(kbo_sql_escape_literal(escaped, sizeof(escaped), NULL));
    assert(strcmp(escaped, "") == 0);
    strcpy(escaped, "untouched");
    assert(!kbo_sql_escape_literal(escaped, 5u, "abcdef"));
    assert(strcmp(escaped, "") == 0);
    assert(!kbo_sql_escape_literal(NULL, sizeof(escaped), "x"));
    printf("test_core_text_and_sql_helpers: PASS\n");
}

static void test_ootp_display_string_decode(void)
{
    const char internal_allstar[] = "\x01,c62c,c2a4,d0c0\x01";
    char out[96] = {0};

    assert(copy_limited_ootp_display_string("KIA Tigers", out, sizeof(out)));
    assert(strcmp(out, "KIA Tigers") == 0);

    assert(copy_limited_ootp_display_string(internal_allstar, out, sizeof(out)));
    assert(strcmp(out, "올스타") == 0);
    assert(kbo_ootp_text_has_non_ascii(out));
    assert(!kbo_ootp_text_has_non_ascii("KIA Tigers"));
    assert(!kbo_ootp_text_has_non_ascii(NULL));

    uint8_t team[OOTP27_KBO_TEAM_READABLE_BYTES] = {0};
    *(const char**)(team + 0x70u + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET) = internal_allstar;
    assert(team_contains_ootp_string_text(team, "올스타"));
    assert(team_contains_ootp_string_text(team, "스타"));
    assert(!team_contains_ootp_string_text(team, "Future Stars"));

    printf("test_ootp_display_string_decode: PASS\n");
}

static void test_json_flags_parser(void)
{
    const char json[] =
        "{\r\n"
        "  \"enable_foreign_waiver_ai\": true,\r\n"
        "  \"enable_launcher_injection\": \"YES\",\r\n"
        "  \"disable_foreign_injury_replacement\": 0,\r\n"
        "  \"intl_established_fa_multiplier\": \" 5 \",\r\n"
        "  \"nested\": {\"ignored\": false}\r\n"
        "}\r\n";
    int value = 0;
    const char* start = NULL;
    const char* end = NULL;

    assert(kbo_find_flag_value_in_json(json, (DWORD)strlen(json), "enable_foreign_waiver_ai", &value));
    assert(value == 1);
    assert(kbo_find_flag_value_in_json(json, (DWORD)strlen(json), "enable_launcher_injection", &value));
    assert(value == 1);
    assert(kbo_find_flag_value_in_json(json, (DWORD)strlen(json), "disable_foreign_injury_replacement", &value));
    assert(value == 0);
    assert(kbo_find_int_value_in_json(json, (DWORD)strlen(json), "intl_established_fa_multiplier", &value));
    assert(value == 5);
    assert(kbo_find_json_value_span(json, (DWORD)strlen(json), "intl_established_fa_multiplier", &start, &end));
    assert((size_t)(end - start) == strlen("\" 5 \""));
    assert(strncmp(start, "\" 5 \"", (size_t)(end - start)) == 0);
    assert(!kbo_find_json_value_span(json, (DWORD)strlen(json), " 5 ", &start, &end));
    assert(!kbo_find_flag_value_in_json(json, (DWORD)strlen(json), "ignored", &value));

    const char cbt_rules_json[] =
        "{\r\n"
        "  \"enabled\": true,\r\n"
        "  \"top_player_count\": 40,\r\n"
        "  \"thresholds\": [\r\n"
        "    { \"season\": 2026, \"threshold\": 10300000 },\r\n"
        "    { \"season\": 2027, \"threshold\": 10800000 }\r\n"
        "  ],\r\n"
        "  \"tax_rate_1\": 50,\r\n"
        "  \"exception_deadline_days_after_opening\": 6,\r\n"
        "  \"announcement_days_after_opening\": 7\r\n"
        "}\r\n";
    assert(kbo_find_int_value_in_json(cbt_rules_json, (DWORD)strlen(cbt_rules_json), "tax_rate_1", &value));
    assert(value == 50);
    assert(kbo_find_int_value_in_json(cbt_rules_json, (DWORD)strlen(cbt_rules_json), "announcement_days_after_opening", &value));
    assert(value == 7);
    assert(!kbo_find_int_value_in_json(cbt_rules_json, (DWORD)strlen(cbt_rules_json), "season", &value));

    const char ui_json[] =
        "{\r\n"
        "  \"MOD INFO\": \"모드 정보\",\r\n"
        "  \"Ultimate KBO is built only to provide the best KBO experience in OOTP.\": \"Ultimate KBO 모드\"\r\n"
        "}\r\n";
    char text_value[128] = {0};
    assert(kbo_find_string_value_in_json(ui_json, (DWORD)strlen(ui_json), "MOD INFO", text_value, sizeof(text_value)));
    assert(strcmp(text_value, "모드 정보") == 0);
    assert(!kbo_find_string_value_in_json(ui_json, (DWORD)strlen(ui_json), "모드 정보", text_value, sizeof(text_value)));

    const char bom_json[] =
        "\xEF\xBB\xBF{\r\n"
        "  \"enable_experimental_runtime_hooks\": true,\r\n"
        "  \"disable_kbo_no_minor_contract_patch\": true\r\n"
        "}\r\n";
    assert(kbo_find_flag_value_in_json(
        bom_json,
        (DWORD)strlen(bom_json),
        "enable_experimental_runtime_hooks",
        &value));
    assert(value == 1);
    assert(kbo_find_flag_value_in_json(
        bom_json,
        (DWORD)strlen(bom_json),
        "disable_kbo_no_minor_contract_patch",
        &value));
    assert(value == 1);

    assert(!kbo_find_flag_value_in_json("{ nope", 6u, "enable_foreign_waiver_ai", &value));
    printf("test_json_flags_parser: PASS\n");
}

static void test_award_schedule_policy_parse_event_types(void)
{
    const char json[] =
        "{\n"
        "  \"awards\": [\n"
        "    {\n"
        "      \"id\": \"mvp\",\n"
        "      \"label\": \"KBO Most Valuable Player Award\",\n"
        "      \"label_en\": \"Most Valuable Player Award announced\",\n"
        "      \"label_ko\": \"KBO 최우수선수상 발표\",\n"
        "      \"event_types\": [22, 31],\n"
        "      \"match_titles\": [],\n"
        "      \"default_month\": 11,\n"
        "      \"default_day\": 26,\n"
        "      \"year_overrides\": [{ \"year\": 2027, \"month\": 12, \"day\": 1 }]\n"
        "    }\n"
        "  ]\n"
        "}\n";

    KboAwardSchedulePolicy policy;
    memset(&policy, 0, sizeof(policy));

    assert(kbo_award_schedule_parse_policy(json, &policy));
    assert(policy.rule_count == 1u);
    assert(strcmp(policy.rules[0].id, "mvp") == 0);
    assert(strcmp(policy.rules[0].label_en, "Most Valuable Player Award announced") == 0);
    assert(strcmp(policy.rules[0].label_ko, "KBO 최우수선수상 발표") == 0);
    assert(policy.rules[0].title_count == 0u);
    assert(policy.rules[0].event_type_count == 2u);
    assert(policy.rules[0].event_types[0] == 22u);
    assert(policy.rules[0].event_types[1] == 31u);
    assert(policy.rules[0].default_month == 11u);
    assert(policy.rules[0].default_day == 26u);
    assert(policy.rules[0].override_count == 1u);
    assert(policy.rules[0].overrides[0].year == 2027u);
    assert(policy.rules[0].overrides[0].month == 12u);
    assert(policy.rules[0].overrides[0].day == 1u);

    printf("test_award_schedule_policy_parse_event_types: PASS\n");
}

static void test_news_template_render(void)
{
    KboNewsTemplateVar vars[] = {
        { "season", "2027" },
        { "team_link", "<Doosan Bears:team#1>" },
        { "player_link", "<Yang Ui-ji:player#10>" },
    };
    char out[256] = {0};

    assert(kbo_news_template_render(
        "KBO captains for {season}: {team_link} named {player_link}.",
        vars,
        (int)(sizeof(vars) / sizeof(vars[0])),
        out,
        sizeof(out)));
    assert(strcmp(out, "KBO captains for 2027: <Doosan Bears:team#1> named <Yang Ui-ji:player#10>.") == 0);

    assert(kbo_news_template_render("{missing} stays visible", vars, 1, out, sizeof(out)));
    assert(strcmp(out, "{missing} stays visible") == 0);

    printf("test_news_template_render: PASS\n");
}

static void test_news_related_link_parse(void)
{
    KboNewsRelatedIds ids;
    kbo_news_related_ids_collect_pair(
        &ids,
        "<Lead Player:player#123> headlines for <Lead Team:team#7>",
        "Body repeats <Lead Player:player#123>, adds <Other Player:player#456>, "
        "ignores <Zero Player:player#0>, keeps <Other Team:team#8>, "
        "and rejects malformed <Bad:player#55 text.");

    assert(ids.player_count == 2);
    assert(ids.player_ids[0] == 123u);
    assert(ids.player_ids[1] == 456u);
    assert(ids.team_count == 2);
    assert(ids.team_ids[0] == 7u);
    assert(ids.team_ids[1] == 8u);

    kbo_news_related_ids_collect_pair(
        &ids,
        "<Jack Cushing:player#456>, Trenton Brooks injury replacement",
        "<Kiwoom Heroes:team#42> filled <Trenton Brooks:player#123>'s absence with "
        "<Jack Cushing:player#456>.");
    assert(ids.player_count == 2);
    assert(ids.player_ids[0] == 456u);
    assert(ids.player_ids[1] == 123u);
    assert(ids.team_count == 1);
    assert(ids.team_ids[0] == 42u);

    kbo_news_related_ids_collect_pair(
        &ids,
        "<Compensation Pick:player#777> moves as FA compensation pick",
        "The compensation process tied to <Free Agent:player#888>'s move is complete. "
        "<Original Team:team#11> selected <Compensation Pick:player#777>.");
    assert(ids.player_count == 2);
    assert(ids.player_ids[0] == 777u);
    assert(ids.player_ids[1] == 888u);
    assert(ids.team_count == 1);
    assert(ids.team_ids[0] == 11u);
    printf("test_news_related_link_parse: PASS\n");
}

static void test_news_strip_link_markup(void)
{
    char out[256] = {0};

    assert(kbo_news_strip_link_markup(
        "<Jack Cushing:player#456>, <Kiwoom Heroes:team#42> injury replacement",
        out,
        sizeof(out)));
    assert(strcmp(out, "Jack Cushing, Kiwoom Heroes injury replacement") == 0);

    assert(kbo_news_strip_link_markup(
        "Keeps malformed <Bad:player#55 text and strips <Good:player#77>.",
        out,
        sizeof(out)));
    assert(strcmp(out, "Keeps malformed <Bad:player#55 text and strips Good.") == 0);

    assert(kbo_news_strip_link_markup("Plain title", out, sizeof(out)));
    assert(strcmp(out, "Plain title") == 0);

    printf("test_news_strip_link_markup: PASS\n");
}

static void test_date_serial(void)
{
    assert(kbo_date_serial(2024u, 3u, 16u) == kbo_date_serial(2024u, 3u, 15u) + 1u);
    assert(kbo_date_serial(2024u, 1u, 1u) == kbo_date_serial(2023u, 12u, 31u) + 1u);
    assert(kbo_date_serial(2000u, 2u, 29u) != 0u);
    assert(kbo_date_serial(1900u, 2u, 29u) == 0u);
    assert(kbo_date_serial(2024u, 4u, 31u) == 0u);
    assert(kbo_date_serial(0u, 1u, 1u) == 0u);
    assert(kbo_date_serial(2024u, 13u, 1u) == 0u);
    assert(kbo_yyyymmdd_valid(19800101u));
    assert(kbo_yyyymmdd_valid(22001231u));
    assert(kbo_yyyymmdd_valid(20240229u));
    assert(!kbo_yyyymmdd_valid(19791231u));
    assert(!kbo_yyyymmdd_valid(22010101u));
    assert(!kbo_yyyymmdd_valid(20230229u));
    assert(kbo_yyyymmdd_add_days(20240228u, 1u) == 20240229u);
    assert(kbo_yyyymmdd_add_days(20240229u, 1u) == 20240301u);
    assert(kbo_yyyymmdd_add_days(20231231u, 1u) == 20240101u);
    assert(kbo_yyyymmdd_add_days(20240101u, 0u) == 20240101u);
    assert(kbo_yyyymmdd_add_days(20230229u, 1u) == 0u);
    assert(kbo_yyyymmdd_add_days(22001231u, 1u) == 0u);
    assert(kbo_current_date_serial() == 0u);
    printf("test_date_serial: PASS\n");
}

static void kbo_test_reset_current_date_tick_state(void)
{
    InterlockedExchange(&g_kbo_current_date_tick_event_write_cursor, 0);
    InterlockedExchange(&g_kbo_current_date_tick_event_published_sequence, 0);
    InterlockedExchange(&g_kbo_current_date_tick_last_published_date, 0);
    memset(g_kbo_current_date_tick_event_dates, 0, sizeof(g_kbo_current_date_tick_event_dates));
    memset(g_kbo_current_date_tick_event_site_rvas, 0, sizeof(g_kbo_current_date_tick_event_site_rvas));
    memset(g_kbo_current_date_tick_event_source_kinds, 0, sizeof(g_kbo_current_date_tick_event_source_kinds));
    memset(g_kbo_current_date_tick_event_save_epochs, 0, sizeof(g_kbo_current_date_tick_event_save_epochs));
    kbo_date_boundary_reset("test", "reset_current_date_tick_state");
    g_test_current_yyyymmdd = 0u;
    g_test_current_save_path[0] = '\0';
}

static void test_current_date_tick_consumer_retries_save_enter_until_date_ready(void)
{
    kbo_test_reset_current_date_tick_state();
    snprintf(g_test_current_save_path, sizeof(g_test_current_save_path), "C:\\test\\saved_games\\New Game.lg");

    KboCurrentDateTickConsumer consumer = {0};
    KboCurrentDateTickWork work = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "test_save_enter_retry",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    assert(!kbo_current_date_tick_consumer_next(&consumer, &work));

    g_test_current_yyyymmdd = 20260301u;
    assert(kbo_current_date_tick_consumer_next(&consumer, &work));
    assert(work.date == 20260301u);
    assert(work.event_date == 20260301u);
    assert(work.site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA);
    assert(work.sequence == 1u);
    kbo_current_date_tick_consumer_mark_processed(&consumer);

    g_test_current_yyyymmdd = 20260302u;
    assert(!kbo_current_date_tick_consumer_next(&consumer, &work));
    assert(kbo_current_date_tick_publish(20260302u, 0x1234u));
    assert(kbo_current_date_tick_consumer_next(&consumer, &work));
    assert(work.date == 20260302u);
    assert(work.event_date == 20260302u);
    assert(work.site_rva == 0x1234u);
    assert(work.sequence == 2u);
    kbo_current_date_tick_consumer_mark_processed(&consumer);

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_tick_consumer_retries_save_enter_until_date_ready: PASS\n");
}

static void test_current_date_tick_consumer_preserves_hooks_before_save_path_ready(void)
{
    kbo_test_reset_current_date_tick_state();

    KboCurrentDateTickConsumer consumer = {0};
    KboCurrentDateTickWork work = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "test_preserve_pre_save_hooks",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    assert(!kbo_current_date_tick_consumer_next(&consumer, &work));
    assert(kbo_current_date_tick_publish(20260302u, 0x2222u));
    assert(kbo_current_date_tick_publish(20260303u, 0x3333u));

    snprintf(g_test_current_save_path, sizeof(g_test_current_save_path), "C:\\test\\saved_games\\New Game.lg");
    g_test_current_yyyymmdd = 20260303u;

    assert(kbo_current_date_tick_consumer_next(&consumer, &work));
    assert(work.date == 20260302u);
    assert(work.event_date == 20260302u);
    assert(work.site_rva == 0x2222u);
    assert(work.sequence == 1u);
    kbo_current_date_tick_consumer_mark_processed(&consumer);

    assert(kbo_current_date_tick_consumer_next(&consumer, &work));
    assert(work.date == 20260303u);
    assert(work.event_date == 20260303u);
    assert(work.site_rva == 0x3333u);
    assert(work.sequence == 2u);
    kbo_current_date_tick_consumer_mark_processed(&consumer);

    assert(!kbo_current_date_tick_consumer_next(&consumer, &work));

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_tick_consumer_preserves_hooks_before_save_path_ready: PASS\n");
}

static void test_current_date_tick_consumer_requires_published_dates(void)
{
    kbo_test_reset_current_date_tick_state();
    snprintf(g_test_current_save_path, sizeof(g_test_current_save_path), "C:\\test\\saved_games\\New Game.lg");

    KboCurrentDateTickConsumer consumer = {0};
    KboCurrentDateTickWork work = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "test_published_dates_only",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    g_test_current_yyyymmdd = 20260301u;
    assert(kbo_current_date_tick_consumer_next(&consumer, &work));
    assert(work.date == 20260301u);
    assert(work.event_date == 20260301u);
    assert(work.site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA);
    assert(work.sequence == 1u);
    kbo_current_date_tick_consumer_mark_processed(&consumer);

    g_test_current_yyyymmdd = 20260305u;
    assert(!kbo_current_date_tick_consumer_next(&consumer, &work));

    assert(!kbo_current_date_tick_publish(20260305u, 0x5555u));
    assert(!kbo_current_date_tick_consumer_next(&consumer, &work));

    assert(kbo_current_date_tick_publish(20260302u, 0x2222u));
    assert(kbo_current_date_tick_consumer_next(&consumer, &work));
    assert(work.date == 20260302u);
    assert(work.event_date == 20260302u);
    assert(work.site_rva == 0x2222u);
    assert(work.sequence == 2u);
    assert(work.gap == 0);
    kbo_current_date_tick_consumer_mark_processed(&consumer);
    assert(!kbo_current_date_tick_consumer_next(&consumer, &work));

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_tick_consumer_requires_published_dates: PASS\n");
}

static void test_current_date_tick_publish_rejects_non_adjacent_live_dates(void)
{
    kbo_test_reset_current_date_tick_state();

    KboCurrentDateTickCursor cursor = {0};
    KboCurrentDateTickEvent event = {0};
    uint32_t latest = 0u;
    kbo_current_date_tick_cursor_init(&cursor);

    assert(kbo_current_date_tick_publish(20260430u, 0x1111u));
    assert(!kbo_current_date_tick_publish(20260530u, 0x2222u));
    assert(kbo_current_date_tick_latest_published_date(&latest));
    assert(latest == 20260430u);
    assert(kbo_current_date_tick_publish(20260501u, 0x3333u));

    assert(kbo_current_date_tick_next(&cursor, &event));
    assert(event.sequence == 1u);
    assert(event.date == 20260430u);
    assert(event.site_rva == 0x1111u);

    assert(kbo_current_date_tick_next(&cursor, &event));
    assert(event.sequence == 2u);
    assert(event.date == 20260501u);
    assert(event.site_rva == 0x3333u);

    assert(!kbo_current_date_tick_next(&cursor, &event));

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_tick_publish_rejects_non_adjacent_live_dates: PASS\n");
}

static void test_current_date_tick_live_candidate_publishable_filters_transient_dates(void)
{
    kbo_test_reset_current_date_tick_state();

    uint32_t previous = 0u;
    uint32_t expected = 0u;
    assert(kbo_current_date_tick_live_candidate_publishable(
        20260430u,
        KBO_CURRENT_DATE_TICK_WATCHPOINT_SITE_RVA,
        &previous,
        &expected));
    assert(previous == 0u);
    assert(expected == 0u);

    assert(kbo_current_date_tick_publish(
        20260430u,
        KBO_CURRENT_DATE_TICK_WATCHPOINT_SITE_RVA));

    previous = 0u;
    expected = 0u;
    assert(!kbo_current_date_tick_live_candidate_publishable(
        20260530u,
        KBO_CURRENT_DATE_TICK_WATCHPOINT_SITE_RVA,
        &previous,
        &expected));
    assert(previous == 20260430u);
    assert(expected == 20260501u);

    previous = 0u;
    expected = 0u;
    assert(kbo_current_date_tick_live_candidate_publishable(
        20260501u,
        KBO_CURRENT_DATE_TICK_WATCHPOINT_SITE_RVA,
        &previous,
        &expected));
    assert(previous == 20260430u);
    assert(expected == 20260501u);

    previous = 0u;
    expected = 0u;
    assert(kbo_current_date_tick_live_candidate_publishable(
        20260430u,
        KBO_CURRENT_DATE_TICK_WATCHPOINT_SITE_RVA,
        &previous,
        &expected));
    assert(previous == 20260430u);

    previous = 0u;
    expected = 0u;
    assert(!kbo_current_date_tick_live_candidate_publishable(
        20260431u,
        KBO_CURRENT_DATE_TICK_WATCHPOINT_SITE_RVA,
        &previous,
        &expected));

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_tick_live_candidate_publishable_filters_transient_dates: PASS\n");
}

static void test_current_date_tick_latest_date_helpers_use_published_date(void)
{
    kbo_test_reset_current_date_tick_state();

    uint32_t year = 1u;
    uint32_t month = 1u;
    uint32_t day = 1u;
    char history_date[16] = {0};
    assert(!kbo_current_date_tick_latest_components(&year, &month, &day));
    assert(year == 0u);
    assert(month == 0u);
    assert(day == 0u);
    assert(kbo_current_date_tick_latest_history_date(
        history_date,
        sizeof(history_date),
        2000u));
    assert(strcmp(history_date, "20000101") == 0);

    g_test_current_yyyymmdd = 20261231u;
    assert(kbo_current_date_tick_publish(
        20260430u,
        KBO_CURRENT_DATE_TICK_WATCHPOINT_SITE_RVA));

    assert(kbo_current_date_tick_latest_components(&year, &month, &day));
    assert(year == 2026u);
    assert(month == 4u);
    assert(day == 30u);

    memset(history_date, 0, sizeof(history_date));
    assert(kbo_current_date_tick_latest_history_date(
        history_date,
        sizeof(history_date),
        2000u));
    assert(strcmp(history_date, "20260430") == 0);

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_tick_latest_date_helpers_use_published_date: PASS\n");
}

static void test_current_date_boundary_context_records_source_and_epoch(void)
{
    kbo_test_reset_current_date_tick_state();
    snprintf(g_test_current_save_path, sizeof(g_test_current_save_path), "C:\\test\\saved_games\\Boundary A.lg");

    KboCurrentDateTickCursor cursor = {0};
    KboCurrentDateTickEvent event = {0};
    KboDateBoundaryContext context = {0};
    kbo_current_date_tick_cursor_init(&cursor);

    assert(kbo_current_date_tick_publish(20260401u, 0x4444u));
    assert(kbo_current_date_tick_latest_boundary_context(&context));
    assert(context.date == 20260401u);
    assert(context.sequence == 1u);
    assert(context.source_kind == KBO_DATE_BOUNDARY_SOURCE_LIVE_POST_ADVANCE);
    assert((context.flags & KBO_DATE_BOUNDARY_FLAG_ACCEPTED) != 0u);
    assert((context.flags & KBO_DATE_BOUNDARY_FLAG_TRUSTED) != 0u);
    assert((context.flags & KBO_DATE_BOUNDARY_FLAG_FIRST_DATE) != 0u);
    assert((context.flags & KBO_DATE_BOUNDARY_FLAG_SAVE_SCOPE_KNOWN) != 0u);
    assert((context.flags & KBO_DATE_BOUNDARY_FLAG_SAVE_SCOPE_CHANGED) != 0u);
    assert(strcmp(context.save_path, g_test_current_save_path) == 0);

    assert(kbo_current_date_tick_next(&cursor, &event));
    assert(event.date == context.date);
    assert(event.source_kind == context.source_kind);
    assert(event.save_epoch == context.save_epoch);

    uint32_t first_epoch = context.save_epoch;
    assert(kbo_current_date_tick_publish(20260402u, 0x5555u));
    assert(kbo_current_date_tick_latest_boundary_context(&context));
    assert(context.date == 20260402u);
    assert(context.previous_date == 20260401u);
    assert(context.expected_next_date == 20260402u);
    assert(context.save_epoch == first_epoch);
    assert((context.flags & KBO_DATE_BOUNDARY_FLAG_SAVE_SCOPE_CHANGED) == 0u);

    snprintf(g_test_current_save_path, sizeof(g_test_current_save_path), "C:\\test\\saved_games\\Boundary B.lg");
    assert(kbo_current_date_tick_publish(20450401u, 0x6666u));
    assert(kbo_current_date_tick_latest_boundary_context(&context));
    assert(context.date == 20450401u);
    assert(context.previous_date == 0u);
    assert(context.save_epoch != first_epoch);
    assert((context.flags & KBO_DATE_BOUNDARY_FLAG_SAVE_SCOPE_CHANGED) != 0u);
    assert((context.flags & KBO_DATE_BOUNDARY_FLAG_FIRST_DATE) != 0u);

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_boundary_context_records_source_and_epoch: PASS\n");
}

static int g_test_current_date_tick_sync_calls = 0;
static uint32_t g_test_current_date_tick_sync_date = 0u;
static uint32_t g_test_current_date_tick_sync_site = 0u;
static int g_test_current_date_tick_sync_result = 1;
static int g_test_current_date_tick_phase_order[8];
static int g_test_current_date_tick_phase_order_count = 0;
static int g_test_current_date_tick_phase_context_ids[3] = {1, 2, 3};

static int kbo_test_current_date_tick_sync_consumer(
    uint32_t date,
    uint32_t site_rva,
    void* context)
{
    int* result = (int*)context;
    ++g_test_current_date_tick_sync_calls;
    g_test_current_date_tick_sync_date = date;
    g_test_current_date_tick_sync_site = site_rva;
    return result != NULL ? *result : 1;
}

static int kbo_test_current_date_tick_phase_consumer(
    uint32_t date,
    uint32_t site_rva,
    void* context)
{
    (void)date;
    (void)site_rva;
    int* id = (int*)context;
    if (id != NULL
            && g_test_current_date_tick_phase_order_count
                < (int)(sizeof(g_test_current_date_tick_phase_order)
                    / sizeof(g_test_current_date_tick_phase_order[0]))) {
        g_test_current_date_tick_phase_order[g_test_current_date_tick_phase_order_count++] = *id;
    }
    return 1;
}

static void test_current_date_tick_sync_consumers_run_by_phase(void)
{
    kbo_test_reset_current_date_tick_state();
    memset(g_test_current_date_tick_phase_order, 0, sizeof(g_test_current_date_tick_phase_order));
    g_test_current_date_tick_phase_order_count = 0;

    assert(kbo_current_date_tick_register_sync_consumer_ex(
        "test_phase_domain",
        KBO_CURRENT_DATE_TICK_SYNC_PHASE_DOMAIN,
        kbo_test_current_date_tick_phase_consumer,
        &g_test_current_date_tick_phase_context_ids[2]));
    assert(kbo_current_date_tick_register_sync_consumer_ex(
        "test_phase_context",
        KBO_CURRENT_DATE_TICK_SYNC_PHASE_CONTEXT,
        kbo_test_current_date_tick_phase_consumer,
        &g_test_current_date_tick_phase_context_ids[0]));
    assert(kbo_current_date_tick_register_sync_consumer_ex(
        "test_phase_events",
        KBO_CURRENT_DATE_TICK_SYNC_PHASE_EVENTS,
        kbo_test_current_date_tick_phase_consumer,
        &g_test_current_date_tick_phase_context_ids[1]));

    assert(kbo_current_date_tick_publish_and_dispatch(20260403u, 0x7777u));
    assert(g_test_current_date_tick_phase_order_count == 3);
    assert(g_test_current_date_tick_phase_order[0] == 1);
    assert(g_test_current_date_tick_phase_order[1] == 2);
    assert(g_test_current_date_tick_phase_order[2] == 3);

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_tick_sync_consumers_run_by_phase: PASS\n");
}

static void test_current_date_tick_publish_and_dispatch_waits_sync_consumers(void)
{
    kbo_test_reset_current_date_tick_state();
    g_test_current_date_tick_sync_calls = 0;
    g_test_current_date_tick_sync_date = 0u;
    g_test_current_date_tick_sync_site = 0u;
    g_test_current_date_tick_sync_result = 0;

    assert(kbo_current_date_tick_register_sync_consumer(
        "test_sync_consumer",
        kbo_test_current_date_tick_sync_consumer,
        &g_test_current_date_tick_sync_result));

    assert(kbo_current_date_tick_publish(20260404u, KBO_CURRENT_DATE_TICK_WATCHPOINT_SITE_RVA));
    assert(!kbo_current_date_tick_publish_and_dispatch(20260404u, 0x15ddf9cu));
    assert(g_test_current_date_tick_sync_calls == 1);
    assert(g_test_current_date_tick_sync_date == 20260404u);
    assert(g_test_current_date_tick_sync_site == 0x15ddf9cu);

    g_test_current_date_tick_sync_result = 1;
    assert(kbo_current_date_tick_publish_and_dispatch(20260404u, 0x15ddf9cu));
    assert(g_test_current_date_tick_sync_calls == 2);

    assert(kbo_current_date_tick_publish_and_dispatch(20260404u, 0x15ddf9cu));
    assert(g_test_current_date_tick_sync_calls == 2);

    assert(!kbo_current_date_tick_publish_and_dispatch(20260406u, 0x15ddf9cu));
    assert(g_test_current_date_tick_sync_calls == 2);

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_tick_publish_and_dispatch_waits_sync_consumers: PASS\n");
}

static void test_current_date_tick_save_enter_does_not_advance_live_date(void)
{
    kbo_test_reset_current_date_tick_state();
    snprintf(g_test_current_save_path, sizeof(g_test_current_save_path), "C:\\test\\saved_games\\New Game.lg");

    KboCurrentDateTickConsumer first_consumer = {0};
    KboCurrentDateTickWork work = {0};
    kbo_current_date_tick_consumer_init(
        &first_consumer,
        "test_save_enter_initial",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    g_test_current_yyyymmdd = 20260301u;
    assert(kbo_current_date_tick_consumer_next(&first_consumer, &work));
    assert(work.date == 20260301u);
    assert(work.site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA);
    kbo_current_date_tick_consumer_mark_processed(&first_consumer);

    KboCurrentDateTickConsumer late_consumer = {0};
    kbo_current_date_tick_consumer_init(
        &late_consumer,
        "test_save_enter_late",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    uint32_t latest = 0u;
    g_test_current_yyyymmdd = 20260305u;
    assert(!kbo_current_date_tick_consumer_next(&late_consumer, &work));
    assert(kbo_current_date_tick_latest_published_date(&latest));
    assert(latest == 20260301u);

    assert(kbo_current_date_tick_publish(20260302u, 0x2222u));
    assert(kbo_current_date_tick_consumer_next(&late_consumer, &work));
    assert(work.date == 20260302u);
    assert(work.event_date == 20260302u);
    assert(work.site_rva == 0x2222u);

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_tick_save_enter_does_not_advance_live_date: PASS\n");
}

static void test_current_date_tick_force_resync_allows_save_enter_rebase(void)
{
    kbo_test_reset_current_date_tick_state();
    snprintf(g_test_current_save_path, sizeof(g_test_current_save_path), "C:\\test\\saved_games\\New Game.lg");

    KboCurrentDateTickConsumer consumer = {0};
    KboCurrentDateTickWork work = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "test_force_resync",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    g_test_current_yyyymmdd = 20260409u;
    assert(kbo_current_date_tick_consumer_next(&consumer, &work));
    assert(work.date == 20260409u);
    assert(work.site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA);
    assert(work.sequence == 1u);

    kbo_current_date_tick_force_resync("test_force_resync", "league_year_mismatch");
    kbo_current_date_tick_consumer_skip_to_latest(&consumer);

    g_test_current_yyyymmdd = 20450922u;
    assert(kbo_current_date_tick_consumer_next(&consumer, &work));
    assert(work.date == 20450922u);
    assert(work.site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA);
    assert(work.sequence == 1u);
    kbo_current_date_tick_consumer_mark_processed(&consumer);

    kbo_test_reset_current_date_tick_state();
    printf("test_current_date_tick_force_resync_allows_save_enter_rebase: PASS\n");
}

static void test_foreign_waiver_date_helpers(void)
{
    uint32_t parsed = 0u;

    assert(kbo_days_in_month(2024u, 2u) == 29);
    assert(kbo_days_in_month(2023u, 2u) == 28);
    assert(kbo_days_in_month(2024u, 0u) == 0);
    assert(kbo_add_one_month_yyyymmdd(20240131u) == 20240229u);
    assert(kbo_add_one_month_yyyymmdd(20230228u) == 20230328u);
    assert(kbo_add_one_month_yyyymmdd(20231231u) == 20240131u);
    assert(kbo_add_one_month_yyyymmdd(19791231u) == 0u);
    assert(kbo_add_days_yyyymmdd(20240227u, 2u) == 20240229u);
    assert(kbo_add_days_yyyymmdd(20240228u, 2u) == 20240301u);
    assert(kbo_add_days_yyyymmdd(20241231u, 1u) == 20250101u);
    assert(kbo_add_years_yyyymmdd(20200229u, 1u) == 20210228u);
    assert(kbo_add_years_yyyymmdd(21991231u, 10u) == 22001231u);
    assert(kbo_add_years_yyyymmdd(22010101u, 1u) == 0u);

    assert(kbo_parse_yyyymmdd("20240229", &parsed) && parsed == 20240229u);
    assert(kbo_parse_yyyymmdd("2024-02-29", &parsed) && parsed == 20240229u);
    assert(kbo_parse_yyyymmdd("20240229 extra", &parsed) && parsed == 20240229u);
    assert(!kbo_parse_yyyymmdd("2024/02/29", &parsed));
    assert(!kbo_parse_yyyymmdd("2024-2-29", &parsed));
    assert(!kbo_parse_yyyymmdd(NULL, &parsed));
    assert(!kbo_parse_yyyymmdd("20240229", NULL));
    assert(!kbo_get_foreign_waiver_current_yyyymmdd(&parsed));
    printf("test_foreign_waiver_date_helpers: PASS\n");
}

static void test_military_csv_parse(void)
{
    char token[] = "  player-01\r\n";
    uint32_t value = 0u;

    kbo_csv_trim_token_in_place(token);
    assert(strcmp(token, "player-01") == 0);
    assert(kbo_military_ascii_is_seed_id_char('A'));
    assert(kbo_military_ascii_is_seed_id_char('_'));
    assert(kbo_military_ascii_is_seed_id_char('-'));
    assert(!kbo_military_ascii_is_seed_id_char('.'));

    assert(kbo_military_parse_u32_full_token(" 4294967295", &value));
    assert(value == 0xffffffffu);
    assert(!kbo_military_parse_u32_full_token("4294967296", &value));
    assert(!kbo_military_parse_u32_full_token("12x", &value));
    assert(kbo_military_parse_u32_full_token(" 12", &value));
    assert(value == 12u);
    assert(!kbo_military_parse_u32_full_token("", &value));

    assert(kbo_military_parse_yyyymmdd("2024-02-29") == 20240229u);
    assert(kbo_military_parse_yyyymmdd("report:20240229") == 20240229u);
    assert(kbo_military_parse_yyyymmdd("2023-02-29") == 0u);
    assert(kbo_military_parse_yyyymmdd("2024-04-31") == 0u);
    assert(kbo_military_parse_yyyymmdd("202402") == 0u);
    printf("test_military_csv_parse: PASS\n");
}

static void test_military_date_round_trip(void)
{
    uint32_t leap_serial = kbo_military_yyyymmdd_to_serial(20240229u);
    uint32_t today = kbo_military_yyyymmdd_to_serial(20240101u);

    assert(leap_serial != 0u);
    assert(kbo_military_serial_to_yyyymmdd(leap_serial) == 20240229u);
    assert(kbo_military_yyyymmdd_to_serial(20230229u) == 0u);
    assert(kbo_military_days_in_month(2024u, 2u) == 29u);
    assert(kbo_military_days_in_month(2023u, 2u) == 28u);
    assert(kbo_military_days_in_month(2023u, 13u) == 0u);

    assert(kbo_military_yyyymmdd_add_days(20240228u, 1) == 20240229u);
    assert(kbo_military_yyyymmdd_add_days(20240228u, 2) == 20240301u);
    assert(kbo_military_yyyymmdd_add_days(20241231u, 1) == 20250101u);
    assert(kbo_military_yyyymmdd_add_days(20240229u, -1) == 0u);

    assert(kbo_military_days_left_from_return_serial(today + 10u, today) == 10);
    assert(kbo_military_days_left_from_return_serial(today, today) == 0);
    assert(kbo_military_days_left_from_return_serial(today + 40000u, today) == 32767);
    printf("test_military_date_round_trip: PASS\n");
}

static void test_military_seed_line_parse(void)
{
    KboMilitaryServiceSeed seed;

    assert(!kbo_parse_military_service_seed_line(NULL, &seed));
    assert(!kbo_parse_military_service_seed_line("# comment", &seed));
    assert(!kbo_parse_military_service_seed_line("source_key,service,original,return,player", &seed));

    assert(kbo_parse_military_service_seed_line("player-A, SANG, LG, 2025-03-01, 12345", &seed));
    assert(strcmp(seed.key, "player-A") == 0);
    assert(seed.player_id == 12345u);
    assert(strcmp(seed.service_team_code, "SANG") == 0);
    assert(strcmp(seed.original_team_code, "LG") == 0);
    assert(seed.service_return_yyyymmdd == 20250301u);
    assert(seed.service_total_days == KBO_MILITARY_SERVICE_DAYS);

    assert(kbo_parse_military_service_seed_line("  \"numeric-77\" , KPB , KT , 2024/02/29 , 67890  ", &seed));
    assert(strcmp(seed.key, "numeric-77") == 0);
    assert(seed.player_id == 67890u);
    assert(seed.service_return_yyyymmdd == 20240229u);

    assert(kbo_parse_military_service_seed_line("legacy, SANG, DOO, 2024-01-01, 30, 24680", &seed));
    assert(seed.player_id == 24680u);
    assert(seed.service_start_yyyymmdd == 20240101u);
    assert(seed.service_total_days == 30);
    assert(seed.service_return_yyyymmdd == 20240131u);

    assert(kbo_parse_military_service_seed_line("98765, SANG, HANWHA, 2025-12-31", &seed));
    assert(seed.player_id == 98765u);
    assert(strcmp(seed.key, "98765") == 0);
    assert(seed.service_return_yyyymmdd == 20251231u);
    printf("test_military_seed_line_parse: PASS\n");
}

static void test_military_service_team_policy_parse(void)
{
    KboMilitaryServiceTeamPolicyRow row;

    assert(!kbo_parse_military_service_team_policy_line(NULL, &row));
    assert(!kbo_parse_military_service_team_policy_line("# team_csv_id,enabled", &row));
    assert(!kbo_parse_military_service_team_policy_line("team_csv_id,enabled", &row));

    assert(kbo_parse_military_service_team_policy_line("SANG,1", &row));
    assert(strcmp(row.team_csv_id, "SANG") == 0);
    assert(row.enabled == 1);

    assert(kbo_parse_military_service_team_policy_line(" KPB , 0 ", &row));
    assert(strcmp(row.team_csv_id, "KPB") == 0);
    assert(row.enabled == 0);

    assert(kbo_parse_military_service_team_policy_line("\"ALT\", enabled", &row));
    assert(strcmp(row.team_csv_id, "ALT") == 0);
    assert(row.enabled == 1);

    assert(!kbo_parse_military_service_team_policy_line("BAD,maybe", &row));
    printf("test_military_service_team_policy_parse: PASS\n");
}

static void test_team_classification_seed_parse(void)
{
    KboTeamClassificationSeedRow row;

    assert(!kbo_parse_team_classification_seed_line(NULL, &row));
    assert(!kbo_parse_team_classification_seed_line("# team_csv_id,enabled,team_type,league_level,display_name", &row));
    assert(!kbo_parse_team_classification_seed_line("team_csv_id,enabled,team_type,league_level,display_name", &row));

    assert(kbo_parse_team_classification_seed_line("ULS,1,independent,futures,Ulsan Whales", &row));
    assert(strcmp(row.team_csv_id, "ULS") == 0);
    assert(row.enabled == 1);
    assert(strcmp(row.team_type, "independent") == 0);
    assert(strcmp(row.league_level, "futures") == 0);
    assert(strcmp(row.display_name, "Ulsan Whales") == 0);
    assert(kbo_team_classification_seed_row_independent_kind(&row) == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES);
    assert(kbo_team_classification_seed_row_is_independent_futures(&row));

    assert(kbo_parse_team_classification_seed_line("\"ALT\", enabled, independent, minor, Alt Club", &row));
    assert(strcmp(row.team_csv_id, "ALT") == 0);
    assert(kbo_team_classification_seed_row_is_independent_futures(&row));

    assert(kbo_parse_team_classification_seed_line("IND,1,independent,independent,Independent League Club", &row));
    assert(kbo_team_classification_seed_row_independent_kind(&row) == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE);
    assert(!kbo_team_classification_seed_row_is_independent_futures(&row));

    assert(kbo_parse_team_classification_seed_line("IND2,1,independent,independent_league,Independent League Club 2", &row));
    assert(kbo_team_classification_seed_row_independent_kind(&row) == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE);
    assert(!kbo_team_classification_seed_row_is_independent_futures(&row));

    assert(kbo_parse_team_classification_seed_line("SANG,1,military,futures,Sangmu", &row));
    assert(kbo_team_classification_seed_row_independent_kind(&row) == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE);
    assert(!kbo_team_classification_seed_row_is_independent_futures(&row));

    assert(kbo_parse_team_classification_seed_line("ULS,0,independent,futures,Ulsan Whales", &row));
    assert(kbo_team_classification_seed_row_independent_kind(&row) == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE);
    assert(!kbo_team_classification_seed_row_is_independent_futures(&row));

    assert(!kbo_parse_team_classification_seed_line("BAD,maybe,independent,futures,Bad", &row));
    printf("test_team_classification_seed_parse: PASS\n");
}

static void test_fa_market_independent_source_precedence(void)
{
    memset(g_test_fa_market_team_leagues, 0, sizeof(g_test_fa_market_team_leagues));
    memset(g_test_fa_market_team_independent_kinds, 0, sizeof(g_test_fa_market_team_independent_kinds));

    KboFaMarketClassification row = {0};
    row.player_id = 30328u;
    row.nation_id = OOTP27_KBO_KOREA_NATION_ID;
    row.original_team_id = 41u;
    row.original_league_id = 200u;
    row.draft_league_id = 203u;
    row.age = 19u;
    row.generation_grade = 1u;
    g_test_fa_market_team_leagues[41] = 200u;

    assert(kbo_fa_market_row_is_undrafted_domestic(&row));
    assert(kbo_fa_market_row_independent_source_kind(&row) == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE);

    KboFaMarketHistoryCase history = {0};
    history.found = 1;
    history.became_free_agent = 1;
    snprintf(history.history_date, sizeof(history.history_date), "20260910");
    snprintf(history.history_text, sizeof(history.history_text), "[G]Became a free agent.");
    assert(kbo_fa_market_apply_history_case(&row, &history));
    assert(strcmp(row.case_label, "DOMESTIC_INDEPENDENT_LEAGUE_FA") == 0);
    assert(strstr(row.reason, "true independent-league") != NULL);

    memset(&row, 0, sizeof(row));
    row.player_id = 30329u;
    row.nation_id = OOTP27_KBO_KOREA_NATION_ID;
    row.original_team_id = 28u;
    row.original_league_id = 101u;
    row.draft_league_id = 203u;
    row.age = 19u;
    row.generation_grade = 1u;
    g_test_fa_market_team_leagues[28] = 101u;
    g_test_fa_market_team_independent_kinds[28] = KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES;

    assert(kbo_fa_market_row_is_undrafted_domestic(&row));
    assert(kbo_fa_market_row_independent_source_kind(&row) == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES);
    assert(kbo_fa_market_apply_history_case(&row, &history));
    assert(strcmp(row.case_label, "DOMESTIC_INDEPENDENT_FUTURES_FA") == 0);
    assert(strstr(row.reason, "independent futures-team") != NULL);

    printf("test_fa_market_independent_source_precedence: PASS\n");
}

static void test_fa_market_carryover_unsigned_metadata(void)
{
    memset(g_test_fa_market_team_leagues, 0, sizeof(g_test_fa_market_team_leagues));
    memset(g_test_fa_market_team_independent_kinds, 0, sizeof(g_test_fa_market_team_independent_kinds));

    KboFaMarketClassification row = {0};
    row.player_id = 40400u;
    row.nation_id = OOTP27_KBO_KOREA_NATION_ID;
    row.original_team_id = 7u;
    row.current_team_id = 0u;
    row.age = 31u;

    KboFaMarketHistoryCase history = {0};
    history.found = 1;
    history.became_free_agent = 1;
    snprintf(history.history_date, sizeof(history.history_date), "20261120");
    snprintf(history.history_text, sizeof(history.history_text), "[G]Became a free agent.");

    assert(kbo_fa_market_apply_history_case(&row, &history));
    assert(strcmp(row.case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0);
    assert(row.fa_filing_date == 20261120u);
    assert(row.fa_filing_season == 2026u);
    assert(!kbo_fa_market_history_is_carryover_unsigned(&row, &history, 20261201u));
    assert(kbo_fa_market_history_is_carryover_unsigned(&row, &history, 20271120u));
    assert(kbo_fa_market_display_team_id(&row) == 7u);

    snprintf(row.case_label, sizeof(row.case_label), "KBO_FA_CARRYOVER_UNSIGNED");
    assert(kbo_fa_market_display_team_id(&row) == 7u);

    printf("test_fa_market_carryover_unsigned_metadata: PASS\n");
}

static void test_allstar_csv_parse(void)
{
    assert(kbo_parse_allstar_side("Nanum") == 1u);
    assert(kbo_parse_allstar_side("Dream") == 2u);
    assert(kbo_parse_allstar_side("North") == 1u);
    assert(kbo_parse_allstar_side("South") == 2u);
    assert(kbo_parse_allstar_side("Western") == 1u);
    assert(kbo_parse_allstar_side("Eastern") == 2u);
    assert(kbo_parse_allstar_side("unknown") == 0u);

    int year_col = -1;
    int team_id_col = -1;
    int name_col = -1;
    int allstar_col = -1;
    char fields[KBO_ALLSTAR_CSV_MAX_COLUMNS][KBO_ALLSTAR_CSV_FIELD_SIZE];
    int field_count = kbo_csv_read_trimmed_line_fields(
        "yearID,teamID,name,allstar_team",
        (char*)fields,
        sizeof(fields[0]),
        KBO_ALLSTAR_CSV_MAX_COLUMNS);
    kbo_csv_find_allstar_team_columns_from_fields(
        fields,
        field_count,
        &year_col,
        &team_id_col,
        &name_col,
        &allstar_col);
    assert(year_col == 0);
    assert(team_id_col == 1);
    assert(name_col == 2);
    assert(allstar_col == 3);

    uint16_t year = 0u;
    char team_id[16] = {0};
    char team_name[96] = {0};
    char current_city[64] = {0};
    uint8_t side = 0u;
    field_count = kbo_csv_read_trimmed_line_fields(
        "2026,LG,LG Twins,Nanum",
        (char*)fields,
        sizeof(fields[0]),
        KBO_ALLSTAR_CSV_MAX_COLUMNS);
    kbo_csv_extract_allstar_team_fields_from_fields(
        fields,
        field_count,
        year_col,
        team_id_col,
        name_col,
        allstar_col,
        &year,
        team_id,
        sizeof(team_id),
        team_name,
        sizeof(team_name),
        current_city,
        sizeof(current_city),
        &side);
    assert(year == 2026u);
    assert(strcmp(team_id, "LG") == 0);
    assert(strcmp(team_name, "LG Twins") == 0);
    assert(strcmp(current_city, "LG") == 0);
    assert(side == 1u);

    field_count = kbo_csv_read_trimmed_line_fields(
        "\"year\",\"team_id\",\"name\",\"all_star_division\"",
        (char*)fields,
        sizeof(fields[0]),
        KBO_ALLSTAR_CSV_MAX_COLUMNS);
    kbo_csv_find_allstar_team_columns_from_fields(
        fields,
        field_count,
        &year_col,
        &team_id_col,
        &name_col,
        &allstar_col);
    assert(year_col == 0);
    assert(team_id_col == 1);
    assert(name_col == 2);
    assert(allstar_col == 3);

    field_count = kbo_csv_read_trimmed_line_fields(
        "2027,\"SSG\",\"SSG Landers, Incheon\",\"dong-gun\"",
        (char*)fields,
        sizeof(fields[0]),
        KBO_ALLSTAR_CSV_MAX_COLUMNS);
    kbo_csv_extract_allstar_team_fields_from_fields(
        fields,
        field_count,
        year_col,
        team_id_col,
        name_col,
        allstar_col,
        &year,
        team_id,
        sizeof(team_id),
        team_name,
        sizeof(team_name),
        current_city,
        sizeof(current_city),
        &side);
    assert(year == 2027u);
    assert(strcmp(team_id, "SSG") == 0);
    assert(strcmp(team_name, "SSG Landers, Incheon") == 0);
    assert(strcmp(current_city, "SSG") == 0);
    assert(side == 2u);

    field_count = kbo_csv_read_trimmed_line_fields(
        "1700,KIWOOM,Kiwoom Heroes,unknown",
        (char*)fields,
        sizeof(fields[0]),
        KBO_ALLSTAR_CSV_MAX_COLUMNS);
    kbo_csv_extract_allstar_team_fields_from_fields(
        fields,
        field_count,
        year_col,
        team_id_col,
        name_col,
        allstar_col,
        &year,
        team_id,
        sizeof(team_id),
        team_name,
        sizeof(team_name),
        current_city,
        sizeof(current_city),
        &side);
    assert(year == 0u);
    assert(side == 0u);
    printf("test_allstar_csv_parse: PASS\n");
}

static void kbo_test_allstar_schedule_callback_placeholder(void)
{
}

static void test_allstar_schedule_date_slots_ignore_serializer_callbacks(void)
{
    uint8_t* league = (uint8_t*)VirtualAlloc(NULL, 0x900u, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t* descriptor = (uint8_t*)VirtualAlloc(NULL, 0x20u, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(league != NULL);
    assert(descriptor != NULL);

    *(uint16_t*)(league + OOTP27_SEASON_START_DATE_YEAR_OFFSET) = 2026u;
    league[OOTP27_SEASON_START_DATE_DAY_OFFSET] = 28u;
    league[OOTP27_SEASON_START_DATE_MONTH_OFFSET] = 3u;
    assert(kbo_allstar_season_start_date_ready(league));

    *(uint16_t*)(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET) = 2026u;
    league[OOTP27_ALLSTAR_DATE_DAY_OFFSET] = 11u;
    league[OOTP27_ALLSTAR_DATE_MONTH_OFFSET] = 7u;
    assert(kbo_allstar_schedule_date_ready(league));

    *(uintptr_t*)(descriptor + 0x10u) = (uintptr_t)&kbo_test_allstar_schedule_callback_placeholder;
    *(uintptr_t*)(league + OOTP27_SEASON_START_DATE_YEAR_OFFSET) = (uintptr_t)descriptor;
    assert(!kbo_allstar_season_start_date_ready(league));

    *(uintptr_t*)(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET) = (uintptr_t)descriptor;
    assert(!kbo_allstar_schedule_date_ready(league));

    VirtualFree(descriptor, 0, MEM_RELEASE);
    VirtualFree(league, 0, MEM_RELEASE);
    printf("test_allstar_schedule_date_slots_ignore_serializer_callbacks: PASS\n");
}

static void test_foreign_replacement_seed_parse(void)
{
    KboForeignReplacementPlayerSeed seed;
    char token[] = " \tseed-key\r\n";

    kbo_csv_trim_token_in_place(token);
    assert(strcmp(token, "seed-key") == 0);
    assert(kbo_ascii_is_seed_id_char('Z'));
    assert(kbo_ascii_is_seed_id_char('7'));
    assert(kbo_ascii_is_seed_id_char('_'));
    assert(!kbo_ascii_is_seed_id_char('.'));

    assert(kbo_parse_foreign_replacement_seed_slot_type("asian") == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA);
    assert(kbo_parse_foreign_replacement_seed_slot_type("2") == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA);
    assert(kbo_parse_foreign_replacement_seed_slot_type("foreign") == KBO_FOREIGN_INJURY_SLOT_REGULAR);
    assert(kbo_parse_foreign_replacement_seed_slot_type("regular") == KBO_FOREIGN_INJURY_SLOT_REGULAR);
    assert(kbo_parse_foreign_replacement_seed_slot_type("bogus") == 0u);

    assert(!kbo_parse_foreign_replacement_player_seed_line(NULL, &seed));
    assert(!kbo_parse_foreign_replacement_player_seed_line("; comment", &seed));
    assert(!kbo_parse_foreign_replacement_player_seed_line("player_id,slot_type", &seed));

    assert(kbo_parse_foreign_replacement_player_seed_line("  \"slug-01\" , asian_quota ", &seed));
    assert(strcmp(seed.key, "slug-01") == 0);
    assert(seed.player_id == 0u);
    assert(seed.slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA);

    assert(kbo_parse_foreign_replacement_player_seed_line("123456, regular, ignored", &seed));
    assert(strcmp(seed.key, "123456") == 0);
    assert(seed.player_id == 123456u);
    assert(seed.slot_type == KBO_FOREIGN_INJURY_SLOT_REGULAR);

    assert(kbo_parse_foreign_replacement_player_seed_line("abc, unknown", &seed));
    assert(strcmp(seed.key, "abc") == 0);
    assert(seed.slot_type == 0u);
    printf("test_foreign_replacement_seed_parse: PASS\n");
}

static void test_captain_seed_parse(void)
{
    KboCaptainSeed seed;
    uint32_t u32 = 0u;
    int32_t i32 = 0;

    char token[] = "  LG\r\n";
    kbo_csv_trim_token_in_place(token);
    assert(strcmp(token, "LG") == 0);
    assert(kbo_captain_parse_u32_full_token("4294967295", &u32));
    assert(u32 == 0xffffffffu);
    assert(!kbo_captain_parse_u32_full_token("4294967296", &u32));
    assert(kbo_captain_parse_i32_full_token("-25", &i32));
    assert(i32 == -25);

    assert(!kbo_parse_captain_seed_line(NULL, &seed));
    assert(!kbo_parse_captain_seed_line("# comment", &seed));
    assert(!kbo_parse_captain_seed_line("season,league_id,team_id,team_code,captain_player_id,captain_player_key", &seed));

    assert(kbo_parse_captain_seed_line("2026,100,0,LG,12345,,Hong Captain,250,active,comment", &seed));
    assert(seed.season == 2026u);
    assert(seed.league_id == 100u);
    assert(seed.team_id == 0u);
    assert(strcmp(seed.team_code, "LG") == 0);
    assert(seed.player_id == 12345u);
    assert(seed.player_key[0] == '\0');
    assert(strcmp(seed.player_name, "Hong Captain") == 0);
    assert(seed.priority == 250);
    assert(seed.active == 1u);

    assert(kbo_parse_captain_seed_line("2026,0,0,LG,,park--000hae,Park Hae-min,1000,active,2026 KBO captain seed", &seed));
    assert(seed.season == 2026u);
    assert(seed.league_id == 0u);
    assert(strcmp(seed.team_code, "LG") == 0);
    assert(seed.player_id == 0u);
    assert(strcmp(seed.player_key, "park--000hae") == 0);
    assert(strcmp(seed.player_name, "Park Hae-min") == 0);
    assert(seed.priority == 1000);
    assert(seed.active == 1u);

    assert(kbo_parse_captain_seed_line("SSG, park--001sun, Park Sung-han, 50, disabled", &seed));
    assert(seed.season == 0u);
    assert(seed.league_id == 0u);
    assert(seed.team_id == 0u);
    assert(strcmp(seed.team_code, "SSG") == 0);
    assert(seed.player_id == 0u);
    assert(strcmp(seed.player_key, "park--001sun") == 0);
    assert(seed.priority == 50);
    assert(seed.active == 0u);

    assert(kbo_parse_captain_seed_line("10, 99999", &seed));
    assert(seed.team_id == 10u);
    assert(seed.player_id == 99999u);
    assert(seed.priority == 100);
    printf("test_captain_seed_parse: PASS\n");
}

static void test_captain_effective_season(void)
{
    assert(kbo_captain_effective_season(20270310u, 2026u) == 2027u);
    assert(kbo_captain_calendar_season_recovery_active(20270310u, 2026u, 0u));
    assert(!kbo_captain_calendar_season_recovery_active(20270310u, 2026u, 2u));
    assert(!kbo_captain_calendar_season_recovery_active(20270310u, 2026u, 3u));
    assert(kbo_captain_calendar_preseason_window_active(20270310u, 2026u, 0u));
    assert(kbo_captain_calendar_preseason_window_active(20270323u, 2026u, 0u));
    assert(!kbo_captain_calendar_preseason_window_active(20270218u, 2026u, 0u));
    assert(!kbo_captain_calendar_preseason_window_active(20270310u, 2026u, 2u));
    assert(kbo_captain_effective_season(20270310u, 2027u) == 2027u);
    assert(!kbo_captain_calendar_season_recovery_active(20270310u, 2027u, 0u));
    assert(kbo_captain_effective_season(20261201u, 2026u) == 2026u);
    assert(kbo_captain_effective_season(20270310u, 0u) == 2027u);
    assert(kbo_captain_effective_season(20271310u, 2026u) == 2026u);
    printf("test_captain_effective_season: PASS\n");
}

static void test_season_phase_effective_rules(void)
{
    int corrected = 0;
    assert(kbo_season_phase_effective_from_values(
        20260310u,
        2026u,
        KBO_SEASON_PHASE_OFFSEASON_RESET,
        20260328u,
        0u,
        &corrected) == KBO_SEASON_PHASE_PRESEASON);
    assert(corrected);

    corrected = 0;
    assert(kbo_season_phase_effective_from_values(
        20260528u,
        2026u,
        KBO_SEASON_PHASE_OFFSEASON_RESET,
        20260328u,
        0u,
        &corrected) == KBO_SEASON_PHASE_REGULAR_SEASON);
    assert(corrected);

    corrected = 0;
    assert(kbo_season_phase_effective_from_values(
        20261012u,
        2026u,
        KBO_SEASON_PHASE_OFFSEASON_RESET,
        20260328u,
        0u,
        &corrected) == KBO_SEASON_PHASE_POSTSEASON);
    assert(corrected);

    corrected = 0;
    assert(kbo_season_phase_effective_from_values(
        20261012u,
        2027u,
        KBO_SEASON_PHASE_POSTSEASON,
        20270328u,
        0u,
        &corrected) == KBO_SEASON_PHASE_OFFSEASON_RESET);
    assert(corrected);

    corrected = 0;
    assert(kbo_season_phase_effective_from_values(
        20261020u,
        2026u,
        KBO_SEASON_PHASE_POSTSEASON,
        20260328u,
        20261020u,
        &corrected) == KBO_SEASON_PHASE_OFFSEASON_RESET);
    assert(corrected);

    corrected = 0;
    assert(kbo_season_phase_effective_from_values(
        20260701u,
        2026u,
        KBO_SEASON_PHASE_REGULAR_SEASON,
        20260328u,
        0u,
        &corrected) == KBO_SEASON_PHASE_REGULAR_SEASON);
    assert(!corrected);

    KboSeasonPhaseInfo info = {0};
    info.capture_valid = 1;
    info.capture_date = 20261020u;
    info.captured_phase = KBO_SEASON_PHASE_OFFSEASON_STARTED;
    info.capture_site_rva = OOTP27_SEASON_PHASE_WRITE_1_RVA;
    assert(kbo_season_phase_info_has_today_offseason_transition_capture(&info, 20261020u));
    info.capture_site_rva = OOTP27_SEASON_PHASE_INIT_0A_RVA;
    assert(!kbo_season_phase_info_has_today_offseason_transition_capture(&info, 20261020u));
    info.capture_site_rva = OOTP27_SEASON_PHASE_WRITE_1_RVA;
    assert(!kbo_season_phase_info_has_today_offseason_transition_capture(&info, 20261021u));

    printf("test_season_phase_effective_rules: PASS\n");
}

static void test_core_csv_parse(void)
{
    const char* cursor = " 123, 456";
    uint32_t value = 0;

    assert(kbo_csv_parse_u32_field(&cursor, &value));
    assert(value == 123u);
    assert(*cursor == ',');
    assert(kbo_csv_parse_u32_field(&cursor, &value));
    assert(value == 456u);

    cursor = "4294967296";
    assert(!kbo_csv_parse_u32_field(&cursor, &value));

    cursor = "abc";
    assert(!kbo_csv_parse_u32_field(&cursor, &value));
    assert(!kbo_csv_parse_u32_field(NULL, &value));
    assert(!kbo_csv_parse_u32_field(&cursor, NULL));

    char csv[] = " plain , \"quoted, with comma\" ,\"\"\"escaped\"\"\",last\n";
    char* cur = csv;
    char field[64];
    assert(kbo_csv_parse_field(&cur, field, sizeof(field)));
    assert(strcmp(field, "plain") == 0);
    assert(kbo_csv_parse_field(&cur, field, sizeof(field)));
    assert(strcmp(field, "quoted, with comma") == 0);
    assert(kbo_csv_parse_field(&cur, field, sizeof(field)));
    assert(strcmp(field, "\"escaped\"") == 0);
    assert(kbo_csv_parse_field(&cur, field, sizeof(field)));
    assert(strcmp(field, "last") == 0);

    char small[4];
    char src[] = "abcdef,rest";
    cur = src;
    assert(kbo_csv_parse_field(&cur, small, sizeof(small)));
    assert(strcmp(small, "abc") == 0);
    assert(strcmp(cur, "rest") == 0);

    assert(!kbo_csv_parse_field(NULL, field, sizeof(field)));
    assert(!kbo_csv_parse_field(&cur, NULL, sizeof(field)));
    assert(!kbo_csv_parse_field(&cur, field, 0u));
    printf("test_core_csv_parse: PASS\n");
}

static void test_masked_pattern_matching(void)
{
    const uint8_t data[] = {0x48u, 0x8Bu, 0x11u, 0x22u, 0x90u};
    const uint8_t pattern[] = {0x48u, 0x8Bu, 0x00u, 0x00u, 0x90u};
    const uint8_t wildcard_mask[] = {1u, 1u, 0u, 0u, 1u};
    const uint8_t exact_mask[] = {1u, 1u, 1u, 1u, 1u};

    assert(kbo_memory_matches_masked_pattern(data, pattern, wildcard_mask, sizeof(data)));
    assert(!kbo_memory_matches_masked_pattern(data, pattern, exact_mask, sizeof(data)));
    assert(!kbo_memory_matches_masked_pattern(NULL, pattern, wildcard_mask, sizeof(data)));
    assert(!kbo_memory_matches_masked_pattern(data, NULL, wildcard_mask, sizeof(data)));
    assert(!kbo_memory_matches_masked_pattern(data, pattern, NULL, sizeof(data)));
    assert(!kbo_memory_matches_masked_pattern(data, pattern, wildcard_mask, 0u));
    printf("test_masked_pattern_matching: PASS\n");
}

static void test_patch_bytes_writers(void)
{
    uint8_t buf[8] = {0};

    write_u64(buf, 0x1122334455667788ull);
    assert(buf[0] == 0x88u);
    assert(buf[1] == 0x77u);
    assert(buf[2] == 0x66u);
    assert(buf[3] == 0x55u);
    assert(buf[4] == 0x44u);
    assert(buf[5] == 0x33u);
    assert(buf[6] == 0x22u);
    assert(buf[7] == 0x11u);

    write_u64(buf, 0u);
    for (int i = 0; i < 8; i++) {
        assert(buf[i] == 0u);
    }

    write_u64(buf, 0xffffffffffffffffull);
    for (int i = 0; i < 8; i++) {
        assert(buf[i] == 0xffu);
    }

    uint8_t buf32[4] = {0xAAu, 0xAAu, 0xAAu, 0xAAu};
    write_u32(buf32, 0xDEADBEEFu);
    assert(buf32[0] == 0xEFu);
    assert(buf32[1] == 0xBEu);
    assert(buf32[2] == 0xADu);
    assert(buf32[3] == 0xDEu);

    printf("test_patch_bytes_writers: PASS\n");
}

static void test_patch_bytes_jump_recognizers(void)
{
    /* movabs r11, imm64 ; jmp r11  →  49 BB <8 bytes> 41 FF E3 */
    const uint8_t r11_jump[13] = {
        0x49u, 0xBBu,
        0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
        0x41u, 0xFFu, 0xE3u
    };
    assert(is_r11_absolute_jump_patch(r11_jump));

    /* movabs rax, imm64 ; jmp rax  →  48 B8 <8 bytes> FF E0 */
    const uint8_t rax_jump[12] = {
        0x48u, 0xB8u,
        0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
        0xFFu, 0xE0u
    };
    assert(is_rax_absolute_jump_patch(rax_jump));

    /* jmp [rip+0]  →  FF 25 00 00 00 00 */
    const uint8_t rip_jump[6] = {0xFFu, 0x25u, 0x00u, 0x00u, 0x00u, 0x00u};
    assert(is_rip_absolute_jump_patch(rip_jump));

    /* Negatives: a single wrong byte must reject the patch shape. */
    uint8_t bad_r11[13];
    memcpy(bad_r11, r11_jump, sizeof(bad_r11));
    bad_r11[12] = 0xE0u;
    assert(!is_r11_absolute_jump_patch(bad_r11));

    uint8_t bad_rax[12];
    memcpy(bad_rax, rax_jump, sizeof(bad_rax));
    bad_rax[1] = 0xB9u;
    assert(!is_rax_absolute_jump_patch(bad_rax));

    uint8_t bad_rip[6];
    memcpy(bad_rip, rip_jump, sizeof(bad_rip));
    bad_rip[5] = 0x01u;
    assert(!is_rip_absolute_jump_patch(bad_rip));

    /* The three shapes must not accept each other's bytes. */
    assert(!is_rax_absolute_jump_patch(r11_jump));
    assert(!is_rip_absolute_jump_patch(r11_jump));
    assert(!is_r11_absolute_jump_patch(rax_jump));
    assert(!is_rip_absolute_jump_patch(rax_jump));

    printf("test_patch_bytes_jump_recognizers: PASS\n");
}

static void test_fa_filing_csv_parse(void)
{
    /* parse_u32: skips leading whitespace; strtoul base 0 (so 0x.. is hex). */
    assert(kbo_fa_filing_parse_u32(NULL) == 0u);
    assert(kbo_fa_filing_parse_u32("") == 0u);
    assert(kbo_fa_filing_parse_u32("   ") == 0u);
    assert(kbo_fa_filing_parse_u32("abc") == 0u);
    assert(kbo_fa_filing_parse_u32(" 12345 ") == 12345u);
    assert(kbo_fa_filing_parse_u32("\t\r\n42") == 42u);
    assert(kbo_fa_filing_parse_u32("0x10") == 16u);
    assert(kbo_fa_filing_parse_u32("99trailing") == 99u);

    /* copy_text: truncating copy that always NUL-terminates. */
    char buf[8];
    buf[0] = 'X';
    kbo_fa_filing_copy_text(buf, sizeof(buf), "hello");
    assert(strcmp(buf, "hello") == 0);

    kbo_fa_filing_copy_text(buf, sizeof(buf), "this is too long");
    assert(strcmp(buf, "this is") == 0);
    assert(buf[7] == '\0');

    buf[0] = 'X';
    kbo_fa_filing_copy_text(buf, sizeof(buf), NULL);
    assert(buf[0] == '\0');

    /* Must not crash on zero-size or NULL out. */
    kbo_fa_filing_copy_text(buf, 0u, "ignored");
    kbo_fa_filing_copy_text(NULL, sizeof(buf), "ignored");

    printf("test_fa_filing_csv_parse: PASS\n");
}

static void test_salary_snapshot_csv_parse(void)
{
    /* parse_u32: base 10 — hex prefix is NOT honoured. */
    assert(kbo_fa_salary_snapshot_parse_u32(NULL) == 0u);
    assert(kbo_fa_salary_snapshot_parse_u32("") == 0u);
    assert(kbo_fa_salary_snapshot_parse_u32("abc") == 0u);
    assert(kbo_fa_salary_snapshot_parse_u32("12345") == 12345u);
    assert(kbo_fa_salary_snapshot_parse_u32("0") == 0u);
    /* "0x10" parses the leading "0", then the "x10" tail is dropped → result 0. */
    assert(kbo_fa_salary_snapshot_parse_u32("0x10") == 0u);

    /* parse_i32: signed base 10. */
    assert(kbo_fa_salary_snapshot_parse_i32(NULL) == 0);
    assert(kbo_fa_salary_snapshot_parse_i32("") == 0);
    assert(kbo_fa_salary_snapshot_parse_i32("xyz") == 0);
    assert(kbo_fa_salary_snapshot_parse_i32("123") == 123);
    assert(kbo_fa_salary_snapshot_parse_i32("-42") == -42);
    assert(kbo_fa_salary_snapshot_parse_i32("+7") == 7);

    printf("test_salary_snapshot_csv_parse: PASS\n");
}

static void test_core_atomic_file_round_trip(void)
{
    char temp_dir[MAX_PATH];
    DWORD dir_len = GetTempPathA(sizeof(temp_dir), temp_dir);
    assert(dir_len > 0u && dir_len < sizeof(temp_dir));

    /* Reserve a unique destination name in the temp dir, then delete the
     * placeholder file GetTempFileNameA created — kbo_atomic_open_tmp must
     * own the .tmp -> dest cycle from a clean slate. */
    char dest_path[MAX_PATH];
    UINT unique = GetTempFileNameA(temp_dir, "kbo", 0u, dest_path);
    assert(unique != 0u);
    DeleteFileA(dest_path);

    char tmp_path[MAX_PATH] = {0};

    /* NULL / zero-size guards: handle stays invalid, no file is created. */
    assert(kbo_atomic_open_tmp(NULL, tmp_path, sizeof(tmp_path)) == INVALID_HANDLE_VALUE);
    assert(kbo_atomic_open_tmp(dest_path, NULL, sizeof(tmp_path)) == INVALID_HANDLE_VALUE);
    assert(kbo_atomic_open_tmp(dest_path, tmp_path, 0u) == INVALID_HANDLE_VALUE);

    /* Happy path: open tmp, write payload, commit. */
    HANDLE file = kbo_atomic_open_tmp(dest_path, tmp_path, sizeof(tmp_path));
    assert(file != INVALID_HANDLE_VALUE);

    /* The contract is dest_path + ".tmp". Buffer is sized so the
     * "%s.tmp" concat is provably non-truncating under -Wformat-truncation. */
    char expected_tmp[MAX_PATH + 8];
    snprintf(expected_tmp, sizeof(expected_tmp), "%s.tmp", dest_path);
    assert(strcmp(tmp_path, expected_tmp) == 0);
    assert(GetFileAttributesA(tmp_path) != INVALID_FILE_ATTRIBUTES);

    const char payload[] = "hello atomic\n";
    DWORD written = 0;
    assert(WriteFile(file, payload, (DWORD)(sizeof(payload) - 1u), &written, NULL));
    assert(written == sizeof(payload) - 1u);

    /* commit closes the handle and renames tmp -> dest. */
    assert(kbo_atomic_commit(file, tmp_path, dest_path));

    /* dest now holds the exact payload; tmp is gone. */
    assert(GetFileAttributesA(tmp_path) == INVALID_FILE_ATTRIBUTES);
    HANDLE check = CreateFileA(dest_path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(check != INVALID_HANDLE_VALUE);
    char read_buf[64] = {0};
    DWORD read_len = 0;
    assert(ReadFile(check, read_buf, (DWORD)(sizeof(read_buf) - 1u), &read_len, NULL));
    assert(read_len == sizeof(payload) - 1u);
    assert(strcmp(read_buf, payload) == 0);
    CloseHandle(check);

    /* Replace-existing semantics: a second round-trip must overwrite cleanly. */
    HANDLE file2 = kbo_atomic_open_tmp(dest_path, tmp_path, sizeof(tmp_path));
    assert(file2 != INVALID_HANDLE_VALUE);
    const char payload2[] = "second";
    assert(WriteFile(file2, payload2, (DWORD)(sizeof(payload2) - 1u), &written, NULL));
    assert(kbo_atomic_commit(file2, tmp_path, dest_path));

    HANDLE check2 = CreateFileA(dest_path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(check2 != INVALID_HANDLE_VALUE);
    char read_buf2[16] = {0};
    assert(ReadFile(check2, read_buf2, (DWORD)(sizeof(read_buf2) - 1u), &read_len, NULL));
    assert(read_len == sizeof(payload2) - 1u);
    assert(strcmp(read_buf2, payload2) == 0);
    CloseHandle(check2);

    DeleteFileA(dest_path);

    /* commit() rejects bad inputs. All four guards short-circuit before
     * CloseHandle, so passing a non-INVALID dummy handle alongside a NULL
     * path is safe — the guard order is (file, tmp, tmp[0], dest). */
    assert(!kbo_atomic_commit(INVALID_HANDLE_VALUE, "x", "y"));
    assert(!kbo_atomic_commit((HANDLE)(uintptr_t)0x1, NULL, "y"));
    assert(!kbo_atomic_commit((HANDLE)(uintptr_t)0x1, "", "y"));
    assert(!kbo_atomic_commit((HANDLE)(uintptr_t)0x1, "x", NULL));

    printf("test_core_atomic_file_round_trip: PASS\n");
}

/* ---- ABI fakes: byte-level "OOTP Player object" stand-ins for policy tests ----
 *
 * The OOTP Player object lives in the game process at a known size and layout.
 * Tests build a uint8_t buffer of that exact size and write fields at the same
 * offsets the real code reads from, so policy functions cannot tell the fake
 * apart from a real OOTP Player. This is the smallest form of an ABI fake —
 * no separate adapter layer, just the offsets the ABI manifest already owns.
 */

static void kbo_test_make_loaned_player(uint8_t* p)
{
    memset(p, 0, OOTP27_PLAYER_SCAN_BYTES);
    /* All four conditions kbo_player_native_on_loan checks for. */
    *(uint32_t*)(p + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 100u;
    p[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 1u;
    p[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
    *(uint32_t*)(p + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 7u;
}

static void test_military_native_loan_on_loan_predicate(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];

    /* NULL guard. */
    assert(!kbo_player_native_on_loan(NULL));

    /* Zeroed object: clearly not on loan. */
    memset(player, 0, sizeof(player));
    assert(!kbo_player_native_on_loan(player));

    /* All four conditions met → on loan. */
    kbo_test_make_loaned_player(player);
    assert(kbo_player_native_on_loan(player));

    /* Each individual condition must break the result. */
    kbo_test_make_loaned_player(player);
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0u;
    assert(!kbo_player_native_on_loan(player));

    kbo_test_make_loaned_player(player);
    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0u;
    assert(!kbo_player_native_on_loan(player));

    kbo_test_make_loaned_player(player);
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 1u;
    assert(!kbo_player_native_on_loan(player));

    kbo_test_make_loaned_player(player);
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0u;
    assert(!kbo_player_native_on_loan(player));

    /* SUBTLE: the team-id check is a single-byte read, not a u32 read.
     * A team_id whose low byte is zero (e.g. 0x100 = 256) is treated as
     * "no team" even though league/active/dfa look loaned. Pin the current
     * behavior so any future fix surfaces here as a deliberate update. */
    kbo_test_make_loaned_player(player);
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0x00000100u;
    assert(!kbo_player_native_on_loan(player));

    /* High-byte-only team_id (e.g. 0xFF000000) still has a non-zero low byte
     * is FALSE — the low byte IS zero, so this also reads as "not loaned".
     * The predicate is sensitive only to the LSB. */
    kbo_test_make_loaned_player(player);
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0xFF000000u;
    assert(!kbo_player_native_on_loan(player));

    printf("test_military_native_loan_on_loan_predicate: PASS\n");
}

static void test_military_native_loan_clear(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];

    /* NULL guard. */
    assert(!kbo_clear_native_player_loan(NULL));

    /* Clearing a loaned player: zeros the loan triple, sets the dirty
     * marker, leaves unrelated fields alone, and on_loan() reports false. */
    kbo_test_make_loaned_player(player);
    /* Mark unrelated bytes that must NOT be touched by clear. */
    player[OOTP27_PLAYER_AGE_OFFSET] = 0x2Au;          /* age = 42 */
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 999u;
    player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] = 0u;

    assert(kbo_clear_native_player_loan(player));

    assert(player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] == 0u);
    assert(*(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == 0u);
    assert(*(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) == 0u);
    assert(player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET] == 1u);

    /* Untouched fields remain at the values the test set above. */
    assert(player[OOTP27_PLAYER_AGE_OFFSET] == 0x2Au);
    assert(*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == 999u);
    assert(player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] == 0u);

    /* Predicate now reports false. */
    assert(!kbo_player_native_on_loan(player));

    /* Idempotent: clearing an already-cleared player still reports success
     * (the post-condition !on_loan() holds). The dirty marker is set again. */
    player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET] = 0u;
    assert(kbo_clear_native_player_loan(player));
    assert(player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET] == 1u);

    printf("test_military_native_loan_clear: PASS\n");
}

static char g_rule_audit_test_dir[MAX_PATH] = {0};

static void test_write_policy_file(const char* file_name, const char* json)
{
    char path[MAX_PATH] = {0};
    size_t dir_len = strlen(g_rule_audit_test_dir);
    size_t file_len = strlen(file_name);
    assert(dir_len + 1u + file_len + 1u <= sizeof(path));
    memcpy(path, g_rule_audit_test_dir, dir_len);
    path[dir_len] = '\\';
    memcpy(path + dir_len + 1u, file_name, file_len + 1u);

    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(file != INVALID_HANDLE_VALUE);
    DWORD written = 0;
    assert(WriteFile(file, json, (DWORD)strlen(json), &written, NULL));
    CloseHandle(file);
    assert(written == (DWORD)strlen(json));
}

static void test_policy_data_begin(void)
{
    char temp_dir[MAX_PATH] = {0};
    DWORD temp_len = GetTempPathA(sizeof(temp_dir), temp_dir);
    assert(temp_len > 0 && temp_len < sizeof(temp_dir));
    char suffix[64] = {0};
    snprintf(suffix, sizeof(suffix), "kbo_policy_test_%lu", (unsigned long)GetCurrentProcessId());
    size_t temp_dir_len = strlen(temp_dir);
    size_t suffix_len = strlen(suffix);
    assert(temp_dir_len + suffix_len + 1u < sizeof(g_rule_audit_test_dir));
    memcpy(g_rule_audit_test_dir, temp_dir, temp_dir_len);
    memcpy(g_rule_audit_test_dir + temp_dir_len, suffix, suffix_len + 1u);
    CreateDirectoryA(g_rule_audit_test_dir, NULL);

    test_write_policy_file(
        "economic_defaults.json",
        "{ \"asian_quota_salary_limit\": 200000, \"intl_established_fa_multiplier\": 20 }\n");
    test_write_policy_file(
        "foreign_player_policy.json",
        "{ \"regular_value_threshold\": 85000, \"asian_value_threshold\": 60000,"
        " \"asian_quota_nation_id_1\": 98, \"asian_quota_nation_id_2\": 12,"
        " \"asian_quota_nation_id_3\": 43 }\n");
    test_write_policy_file(
        "league_roles.json",
        "{ \"main_league_id\": 100, \"independent_league_id\": 200,"
        " \"college_league_id\": 201, \"high_school_league_id\": 203 }\n");
}

static void test_policy_data_end(void)
{
    if (g_rule_audit_test_dir[0] == '\0') {
        return;
    }

    char path[MAX_PATH] = {0};
    size_t dir_len = strlen(g_rule_audit_test_dir);
    const char economic_file[] = "\\economic_defaults.json";
    const char foreign_file[] = "\\foreign_player_policy.json";
    const char league_file[] = "\\league_roles.json";
    assert(dir_len + sizeof(economic_file) <= sizeof(path));
    memcpy(path, g_rule_audit_test_dir, dir_len);
    memcpy(path + dir_len, economic_file, sizeof(economic_file));
    DeleteFileA(path);
    memset(path, 0, sizeof(path));
    assert(dir_len + sizeof(foreign_file) <= sizeof(path));
    memcpy(path, g_rule_audit_test_dir, dir_len);
    memcpy(path + dir_len, foreign_file, sizeof(foreign_file));
    DeleteFileA(path);
    memset(path, 0, sizeof(path));
    assert(dir_len + sizeof(league_file) <= sizeof(path));
    memcpy(path, g_rule_audit_test_dir, dir_len);
    memcpy(path + dir_len, league_file, sizeof(league_file));
    DeleteFileA(path);
    RemoveDirectoryA(g_rule_audit_test_dir);
    g_rule_audit_test_dir[0] = '\0';
}

static void test_nation_table_parses_all_seed_rows(void)
{
    char temp_dir[MAX_PATH] = {0};
    DWORD temp_len = GetTempPathA(sizeof(temp_dir), temp_dir);
    assert(temp_len > 0 && temp_len < sizeof(temp_dir));
    char suffix[64] = {0};
    snprintf(suffix, sizeof(suffix), "kbo_nation_table_test_%lu", (unsigned long)GetCurrentProcessId());
    size_t temp_dir_len = strlen(temp_dir);
    size_t suffix_len = strlen(suffix);
    assert(temp_dir_len + suffix_len + 1u < sizeof(g_rule_audit_test_dir));
    memcpy(g_rule_audit_test_dir, temp_dir, temp_dir_len);
    memcpy(g_rule_audit_test_dir + temp_dir_len, suffix, suffix_len + 1u);
    CreateDirectoryA(g_rule_audit_test_dir, NULL);

    char path[MAX_PATH] = {0};
    const char nations_file_name[] = "\\kbo_nations.json";
    size_t nation_dir_len = strlen(g_rule_audit_test_dir);
    assert(nation_dir_len + sizeof(nations_file_name) <= sizeof(path));
    memcpy(path, g_rule_audit_test_dir, nation_dir_len);
    memcpy(path + nation_dir_len, nations_file_name, sizeof(nations_file_name));
    const char json[] =
        "{\n"
        "  \"nations\": [\n"
        "    { \"id\": 12,  \"label\": \"Australia\", \"abbrev\": \"AUS\", \"flag_file\": \"aus.png\" },\n"
        "    { \"id\": 36,  \"label\": \"Canada\", \"abbrev\": \"CAN\", \"flag_file\": \"can.png\" },\n"
        "    { \"id\": 206, \"label\": \"United States\", \"abbrev\": \"USA\", \"flag_file\": \"usa.png\" }\n"
        "  ]\n"
        "}\n";
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(file != INVALID_HANDLE_VALUE);
    DWORD written = 0;
    assert(WriteFile(file, json, (DWORD)strlen(json), &written, NULL));
    CloseHandle(file);
    assert(written == (DWORD)strlen(json));

    assert(strcmp(kbo_nation_table_label(12u), "Australia") == 0);
    assert(strcmp(kbo_nation_table_abbrev(12u), "AUS") == 0);
    assert(strcmp(kbo_nation_table_label(36u), "Canada") == 0);
    assert(strcmp(kbo_nation_table_abbrev(206u), "USA") == 0);
    assert(strcmp(kbo_nation_table_flag_file(206u), "usa.png") == 0);

    DeleteFileA(path);
    RemoveDirectoryA(g_rule_audit_test_dir);
    g_rule_audit_test_dir[0] = '\0';
    printf("test_nation_table_parses_all_seed_rows: PASS\n");
}

static void test_rule_audit_ndjson_sink(void)
{
    char temp_dir[MAX_PATH] = {0};
    DWORD temp_len = GetTempPathA(sizeof(temp_dir), temp_dir);
    assert(temp_len > 0 && temp_len < sizeof(temp_dir));
    char suffix[64] = {0};
    snprintf(suffix, sizeof(suffix), "kbo_rule_audit_test_%lu", (unsigned long)GetCurrentProcessId());
    size_t temp_dir_len = strlen(temp_dir);
    size_t suffix_len = strlen(suffix);
    assert(temp_dir_len + suffix_len + 1u < sizeof(g_rule_audit_test_dir));
    memcpy(g_rule_audit_test_dir, temp_dir, temp_dir_len);
    memcpy(g_rule_audit_test_dir + temp_dir_len, suffix, suffix_len + 1u);
    CreateDirectoryA(g_rule_audit_test_dir, NULL);

    char audit_path[MAX_PATH] = {0};
    const char audit_file_name[] = "\\rule_audit.ndjson";
    size_t audit_dir_len = strlen(g_rule_audit_test_dir);
    assert(audit_dir_len + sizeof(audit_file_name) <= sizeof(audit_path));
    memcpy(audit_path, g_rule_audit_test_dir, audit_dir_len);
    memcpy(audit_path + audit_dir_len, audit_file_name, sizeof(audit_file_name));
    DeleteFileA(audit_path);

    KboLogFields audit_fields;
    kbo_log_fields_init(&audit_fields);
    kbo_log_field_i32(&audit_fields, "value", 7);
    kbo_log_field_str(&audit_fields, "note", "quote\"slash\\ok");
    kbo_rule_audit_emit_fields(
        "native.test",
        "record",
        "ok",
        "native_tests",
        &audit_fields);

    HANDLE file = CreateFileA(
        audit_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    assert(file != INVALID_HANDLE_VALUE);

    char buffer[2048] = {0};
    DWORD bytes_read = 0;
    BOOL read_ok = ReadFile(file, buffer, sizeof(buffer) - 1u, &bytes_read, NULL);
    CloseHandle(file);
    assert(read_ok && bytes_read > 0);
    buffer[bytes_read] = '\0';

    assert(strstr(buffer, "\"schema\":1") != NULL);
    assert(strstr(buffer, "\"channel\":\"rule_audit\"") != NULL);
    assert(strstr(buffer, "\"level\":\"INFO\"") != NULL);
    assert(strstr(buffer, "\"domain\":\"native.test\"") != NULL);
    assert(strstr(buffer, "\"event\":\"rule_decision\"") != NULL);
    assert(strstr(buffer, "\"rule\":\"native.test\"") != NULL);
    assert(strstr(buffer, "\"decision\":\"record\"") != NULL);
    assert(strstr(buffer, "\"reason\":\"ok\"") != NULL);
    assert(strstr(buffer, "\"source\":\"native_tests\"") != NULL);
    assert(strstr(buffer, "\"value\":7") != NULL);
    assert(strstr(buffer, "\"note\":\"quote\\\"slash\\\\ok\"") != NULL);
    assert(strstr(buffer, "KBO_RULE_AUDIT") == NULL);

    DeleteFileA(audit_path);
    RemoveDirectoryA(g_rule_audit_test_dir);
    g_rule_audit_test_dir[0] = '\0';
}

static void test_team_roster_arrays_contains_player(void)
{
    size_t team_size = OOTP27_TEAM_PLAYER_IDS_3700_OFFSET
        + (OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t));
    uint8_t* team = (uint8_t*)calloc(1u, team_size);
    assert(team != NULL);

    assert(!kbo_team_roster_arrays_contain_player(NULL, 42u));
    assert(!kbo_team_roster_arrays_contain_player(team, 0u));
    assert(!kbo_team_roster_arrays_contain_player(team, 42u));

    uint32_t* ids_2a80 = (uint32_t*)(team + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    ids_2a80[3] = 42u;
    assert(kbo_team_fixed_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, 42u));
    assert(kbo_team_roster_arrays_contain_player(team, 42u));
    assert(!kbo_team_roster_arrays_contain_player(team, 43u));

    ids_2a80[3] = 0u;
    uint32_t* restricted_ids = (uint32_t*)(team + OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET);
    restricted_ids[0] = 77u;
    assert(kbo_team_roster_arrays_contain_player(team, 77u));

    free(team);
    printf("test_team_roster_arrays_contains_player: PASS\n");
}

static void test_foreign_waiver_read_player_i16(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    /* Round-trip a positive and a negative i16 at known offsets. */
    *(int16_t*)(player + 100u) = -42;
    assert(kbo_read_player_i16(player, 100u) == -42);

    *(int16_t*)(player + 200u) = 32767;
    assert(kbo_read_player_i16(player, 200u) == 32767);

    *(int16_t*)(player + 300u) = -32768;
    assert(kbo_read_player_i16(player, 300u) == -32768);

    /* NULL guard. */
    assert(kbo_read_player_i16(NULL, 100u) == 0);

    /* Boundary: offset + sizeof(i16) must fit within SCAN_BYTES.
     * SCAN_BYTES - 2 fits exactly (last legal offset);
     * SCAN_BYTES - 1 would read one byte past the buffer → rejected. */
    *(int16_t*)(player + (OOTP27_PLAYER_SCAN_BYTES - 2u)) = 0x4321;
    assert(kbo_read_player_i16(player, OOTP27_PLAYER_SCAN_BYTES - 2u) == 0x4321);
    assert(kbo_read_player_i16(player, OOTP27_PLAYER_SCAN_BYTES - 1u) == 0);
    assert(kbo_read_player_i16(player, OOTP27_PLAYER_SCAN_BYTES) == 0);

    printf("test_foreign_waiver_read_player_i16: PASS\n");
}

static void test_foreign_waiver_value_score(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];

    /* NULL → 0. */
    assert(kbo_foreign_waiver_value_score(NULL) == 0);

    /* All zero → 0. */
    memset(player, 0, sizeof(player));
    assert(kbo_foreign_waiver_value_score(player) == 0);

    /* Each weight contributes individually:
     *   score = talent*55 + overall*25 + ratings*10 + career*10 */
    memset(player, 0, sizeof(player));
    *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET) = 10;
    assert(kbo_foreign_waiver_value_score(player) == 550);

    memset(player, 0, sizeof(player));
    *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET) = 10;
    assert(kbo_foreign_waiver_value_score(player) == 250);

    memset(player, 0, sizeof(player));
    *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET) = 10;
    assert(kbo_foreign_waiver_value_score(player) == 100);

    memset(player, 0, sizeof(player));
    *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET) = 10;
    assert(kbo_foreign_waiver_value_score(player) == 100);

    /* Combined realistic player. */
    memset(player, 0, sizeof(player));
    *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET)  = 100;
    *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET) = 80;
    *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET) = 70;
    *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET)  = 60;
    assert(kbo_foreign_waiver_value_score(player) == 5500 + 2000 + 700 + 600);

    /* Negative components compose linearly (i16 is signed). */
    memset(player, 0, sizeof(player));
    *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET) = -1;
    assert(kbo_foreign_waiver_value_score(player) == -55);

    printf("test_foreign_waiver_value_score: PASS\n");
}

static void test_player_is_foreign_for_kbo_rights(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];

    /* NULL → not foreign. */
    assert(!kbo_player_is_foreign_for_kbo_rights(NULL));

    /* nation_id == 0 → not foreign (unset). */
    memset(player, 0, sizeof(player));
    assert(!kbo_player_is_foreign_for_kbo_rights(player));

    /* nation_id == Korea → not foreign. */
    *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) = OOTP27_KBO_KOREA_NATION_ID;
    assert(!kbo_player_is_foreign_for_kbo_rights(player));

    /* Any other nation id → foreign for KBO rights purposes. */
    *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) = 98u;  /* Japan */
    assert(kbo_player_is_foreign_for_kbo_rights(player));

    *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) = 1u;   /* US */
    assert(kbo_player_is_foreign_for_kbo_rights(player));

    *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) = 0xFFFFFFFFu;
    assert(kbo_player_is_foreign_for_kbo_rights(player));

    printf("test_player_is_foreign_for_kbo_rights: PASS\n");
}

static void test_asian_quota_slot_candidate_ignores_unprepared_salary(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) = 98u;  /* Japan */
    assert(kbo_player_is_asian_quota_slot_candidate(player));
    assert(!kbo_player_is_asian_quota_candidate(player));

    *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) = 206u; /* United States */
    assert(!kbo_player_is_asian_quota_slot_candidate(player));
    assert(!kbo_player_is_asian_quota_candidate(player));

    memset(player, 0, sizeof(player));
    *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) = 98u;
    int32_t limit = kbo_get_asian_quota_salary_limit();
    if (limit > 0) {
        *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = limit;
        assert(kbo_player_is_asian_quota_slot_candidate(player));
        assert(kbo_player_is_asian_quota_candidate(player));
    }

    printf("test_asian_quota_slot_candidate_ignores_unprepared_salary: PASS\n");
}

static void test_foreign_injury_slot_label(void)
{
    /* The two named slot types: regular foreign vs Asian quota. */
    assert(strcmp(kbo_foreign_injury_slot_label(KBO_FOREIGN_INJURY_SLOT_REGULAR), "Regular") == 0);
    assert(strcmp(kbo_foreign_injury_slot_label(KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA), "Asian quota") == 0);

    /* Anything else falls through to "Regular". This is a single-branch
     * conditional, so unset (0), out-of-range (255), and any future slot
     * type all collapse to "Regular" today. Pin this behavior — if a new
     * slot type is added, the test will fail and force a deliberate
     * decision about its label rather than silent fall-through. */
    assert(strcmp(kbo_foreign_injury_slot_label(0u), "Regular") == 0);
    assert(strcmp(kbo_foreign_injury_slot_label(3u), "Regular") == 0);
    assert(strcmp(kbo_foreign_injury_slot_label(255u), "Regular") == 0);

    printf("test_foreign_injury_slot_label: PASS\n");
}

static void test_foreign_injury_status_label(void)
{
    /* All four lifecycle states map to their human-readable labels. */
    assert(strcmp(kbo_foreign_injury_status_label(KBO_FOREIGN_INJURY_STATUS_OPEN),    "Open") == 0);
    assert(strcmp(kbo_foreign_injury_status_label(KBO_FOREIGN_INJURY_STATUS_ACTIVE),  "Active") == 0);
    assert(strcmp(kbo_foreign_injury_status_label(KBO_FOREIGN_INJURY_STATUS_PENDING), "Decision due") == 0);
    assert(strcmp(kbo_foreign_injury_status_label(KBO_FOREIGN_INJURY_STATUS_CLOSED),  "Closed") == 0);

    /* Status 0 (unset) and any out-of-range value yield the "Unknown"
     * sentinel — distinguishable from the four real states above. */
    assert(strcmp(kbo_foreign_injury_status_label(0u), "Unknown") == 0);
    assert(strcmp(kbo_foreign_injury_status_label(5u), "Unknown") == 0);
    assert(strcmp(kbo_foreign_injury_status_label(255u), "Unknown") == 0);

    printf("test_foreign_injury_status_label: PASS\n");
}

static void test_foreign_injury_policy_helpers(void)
{
    const int min_days = 42;

    assert(!kbo_foreign_injury_duration_meets_minimum(0, min_days));
    assert(!kbo_foreign_injury_duration_meets_minimum(41, min_days));
    assert(kbo_foreign_injury_duration_meets_minimum(42, min_days));
    assert(kbo_foreign_injury_duration_meets_minimum(1239, min_days));
    assert(!kbo_foreign_injury_duration_meets_minimum(-1, min_days));
    int evidence_days = 0;
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "It will be at least 12 months before White returns to the field.",
        min_days,
        &evidence_days));
    assert(evidence_days == 360);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "The 31-year-old pitcher will be out for at least 12 months.",
        min_days,
        &evidence_days));
    assert(evidence_days == 360);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "The club expects him out for 6 weeks.",
        min_days,
        &evidence_days));
    assert(evidence_days == 42);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "Jack confirmed today that his season is officially over after suffering a torn flexor tendon.",
        min_days,
        &evidence_days));
    assert(evidence_days == 180);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "The pitcher suffered a season-ending elbow injury.",
        min_days,
        &evidence_days));
    assert(evidence_days == 180);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "He learned from the team medical staff that he would miss the rest of the season.",
        min_days,
        &evidence_days));
    assert(evidence_days == 180);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "The diagnosis confirmed he would be out the rest of the year.",
        min_days,
        &evidence_days));
    assert(evidence_days == 180);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "The pitcher was diagnosed with a stretched elbow ligament and will miss 11 months.",
        min_days,
        &evidence_days));
    assert(evidence_days == 330);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "Natsuki Toda diagnosed with a ruptured finger tendon, will miss 4 months.",
        min_days,
        &evidence_days));
    assert(evidence_days == 120);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "SP Natsuki Toda was injured while pitching. The Diagnosis: elbow strain. He's expected to miss about 3 months.",
        min_days,
        &evidence_days));
    assert(evidence_days == 90);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "RP Masama Kyoyama diagnosed with a torn labrum, will miss 10 months.",
        min_days,
        &evidence_days));
    assert(evidence_days == 300);
    assert(kbo_foreign_injury_duration_text_meets_minimum(
        "The latest news from the Lotte Giants clubhouse is this: starting pitcher Elvin Rodriguez is expected to be out of action for another 10 months.",
        min_days,
        &evidence_days));
    assert(evidence_days == 300);
    assert(!kbo_foreign_injury_duration_text_meets_minimum(
        "He is expected to miss 2 weeks.",
        min_days,
        &evidence_days));
    assert(evidence_days == 14);

    assert(!kbo_foreign_injury_expected_end_reached(0, 20260420));
    assert(!kbo_foreign_injury_expected_end_reached(20260419, 20260420));
    assert(kbo_foreign_injury_expected_end_reached(20260420, 20260420));
    assert(kbo_foreign_injury_expected_end_reached(20260421, 20260420));
    assert(!kbo_foreign_injury_expected_end_pending(0, 20260420));
    assert(kbo_foreign_injury_expected_end_pending(20260419, 20260420));
    assert(!kbo_foreign_injury_expected_end_pending(20260420, 20260420));
    assert(!kbo_foreign_injury_expected_end_pending(20260421, 20260420));
    assert(kbo_foreign_injury_expected_end_from_duration(20260602, 150) == 20261030u);
    assert(kbo_foreign_injury_expected_end_from_duration(20260621, 150) == 20261118u);
    assert(kbo_foreign_injury_expected_end_from_duration(0, 150) == 0u);
    assert(kbo_foreign_injury_expected_end_from_duration(20260602, 0) == 0u);
    assert(!kbo_foreign_injury_replacement_phase_allows_signing(KBO_SEASON_PHASE_OFFSEASON_RESET));
    assert(!kbo_foreign_injury_replacement_phase_allows_signing(KBO_SEASON_PHASE_OFFSEASON_STARTED));
    assert(!kbo_foreign_injury_replacement_phase_allows_signing(KBO_SEASON_PHASE_PRESEASON));
    assert(kbo_foreign_injury_replacement_phase_allows_signing(KBO_SEASON_PHASE_REGULAR_SEASON));
    assert(!kbo_foreign_injury_replacement_phase_allows_signing(KBO_SEASON_PHASE_POSTSEASON));
    assert(!kbo_foreign_injury_replacement_phase_allows_signing(KBO_SEASON_PHASE_UNKNOWN));
    assert(kbo_foreign_injury_replacement_phase_allows_close(KBO_SEASON_PHASE_OFFSEASON_RESET));
    assert(kbo_foreign_injury_replacement_phase_allows_close(KBO_SEASON_PHASE_OFFSEASON_STARTED));
    assert(!kbo_foreign_injury_replacement_phase_allows_close(KBO_SEASON_PHASE_PRESEASON));
    assert(kbo_foreign_injury_replacement_phase_allows_close(KBO_SEASON_PHASE_REGULAR_SEASON));
    assert(kbo_foreign_injury_replacement_phase_allows_close(KBO_SEASON_PHASE_POSTSEASON));
    assert(!kbo_foreign_injury_replacement_phase_allows_close(KBO_SEASON_PHASE_UNKNOWN));

    assert(!kbo_foreign_injury_active_record_has_roster_basis(KBO_FOREIGN_INJURY_STATUS_OPEN, 3001u, 1));
    assert(!kbo_foreign_injury_active_record_has_roster_basis(KBO_FOREIGN_INJURY_STATUS_ACTIVE, 0u, 1));
    assert(!kbo_foreign_injury_active_record_has_roster_basis(KBO_FOREIGN_INJURY_STATUS_ACTIVE, 3001u, 0));
    assert(kbo_foreign_injury_active_record_has_roster_basis(KBO_FOREIGN_INJURY_STATUS_ACTIVE, 3001u, 1));

    assert(kbo_foreign_injury_return_state_allows_close(0u, 0, 0u, 1, 0, 0, 1));
    assert(!kbo_foreign_injury_return_state_allows_close(0u, 0, 0u, 1, 0, 0, 0));
    assert(!kbo_foreign_injury_return_state_allows_close(1u, 0, 0u, 1, 0, 0, 1));
    assert(!kbo_foreign_injury_return_state_allows_close(0u, 3, 0u, 1, 0, 0, 1));
    assert(!kbo_foreign_injury_return_state_allows_close(0u, 0, 1u, 1, 0, 0, 1));
    assert(!kbo_foreign_injury_return_state_allows_close(0u, 0, 0u, 0, 0, 0, 1));
    assert(!kbo_foreign_injury_return_state_allows_close(0u, 0, 0u, 0, 1, 0, 1));
    assert(!kbo_foreign_injury_return_state_allows_close(0u, 0, 0u, 1, 1, 0, 1));
    assert(!kbo_foreign_injury_return_state_allows_close(0u, 0, 0u, 1, 0, 1, 1));

    assert(kbo_foreign_injury_open_news_allowed(20260329u, 20260329u, 20260329u, 1, 0));
    assert(kbo_foreign_injury_open_news_allowed(20260329u, 20260330u, 20260329u, 1, 1));
    assert(!kbo_foreign_injury_open_news_allowed(20260329u, 20260330u, 20260329u, 1, 0));
    assert(!kbo_foreign_injury_open_news_allowed(20260329u, 20260329u, 20260328u, 1, 1));
    assert(!kbo_foreign_injury_open_news_allowed(20260329u, 20260329u, 0u, 1, 1));
    assert(!kbo_foreign_injury_open_news_allowed(20260329u, 20260329u, 20260330u, 1, 1));
    assert(!kbo_foreign_injury_open_news_allowed(20260329u, 20260329u, 20260329u, 0, 1));

    printf("test_foreign_injury_policy_helpers: PASS\n");
}

static void test_foreign_injury_live_memory_fields(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    uint8_t injury[OOTP27_PLAYER_INJURY_OBJECT_READABLE_BYTES];
    uintptr_t active_injuries[1];
    memset(player, 0, sizeof(player));
    memset(injury, 0, sizeof(injury));
    memset(active_injuries, 0, sizeof(active_injuries));

    active_injuries[0] = (uintptr_t)injury;
    *(uintptr_t*)(player + OOTP27_PLAYER_ACTIVE_INJURY_VECTOR_OFFSET) = (uintptr_t)active_injuries;
    *(int32_t*)(player + OOTP27_PLAYER_ACTIVE_INJURY_COUNT_OFFSET) = 1;
    *(uint32_t*)(injury + OOTP27_PLAYER_INJURY_OBJECT_ID_OFFSET) = 86u;
    *(int32_t*)(injury + OOTP27_PLAYER_INJURY_OBJECT_TOTAL_DAYS_OFFSET) = 117;
    *(int32_t*)(injury + OOTP27_PLAYER_INJURY_OBJECT_DAYS_LEFT_OFFSET) = 82;

    KboForeignInjuryLiveMemory live;
    assert(kbo_foreign_injury_read_live_memory(player, &live));
    assert(live.active == 1u);
    assert(live.active_count == 1);
    assert(live.injury_id == 86u);
    assert(live.days_left == 82);
    assert(kbo_foreign_injury_live_memory_has_long_term_basis(&live, 42));

    injury[OOTP27_PLAYER_INJURY_OBJECT_PENDING_DIAGNOSIS_OFFSET] = 1u;
    assert(kbo_foreign_injury_read_live_memory(player, &live));
    assert(live.pending_diagnosis == 1u);
    assert(!kbo_foreign_injury_live_memory_has_long_term_basis(&live, 42));

    injury[OOTP27_PLAYER_INJURY_OBJECT_PENDING_DIAGNOSIS_OFFSET] = 0u;
    injury[OOTP27_PLAYER_INJURY_OBJECT_DAY_TO_DAY_OFFSET] = 1u;
    assert(kbo_foreign_injury_read_live_memory(player, &live));
    assert(live.day_to_day == 1u);
    assert(!kbo_foreign_injury_live_memory_has_long_term_basis(&live, 42));

    injury[OOTP27_PLAYER_INJURY_OBJECT_DAY_TO_DAY_OFFSET] = 0u;
    *(int32_t*)(injury + OOTP27_PLAYER_INJURY_OBJECT_DAYS_LEFT_OFFSET) = 41;
    assert(kbo_foreign_injury_read_live_memory(player, &live));
    assert(!kbo_foreign_injury_live_memory_has_long_term_basis(&live, 42));
    *(int32_t*)(injury + OOTP27_PLAYER_INJURY_OBJECT_DAYS_LEFT_OFFSET) = 42;
    assert(kbo_foreign_injury_read_live_memory(player, &live));
    assert(kbo_foreign_injury_live_memory_has_long_term_basis(&live, 42));

    *(int32_t*)(player + OOTP27_PLAYER_ACTIVE_INJURY_COUNT_OFFSET) = 0;
    player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] = 1u;
    *(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET) = 120;
    assert(kbo_foreign_injury_read_live_memory(player, &live));
    assert(live.active == 0u);
    assert(!kbo_foreign_injury_live_memory_has_long_term_basis(&live, 42));

    *(int32_t*)(player + OOTP27_PLAYER_ACTIVE_INJURY_COUNT_OFFSET) = 1;
    player[OOTP27_PLAYER_CAREER_ENDING_INJURY_OFFSET] = 1u;
    *(int32_t*)(injury + OOTP27_PLAYER_INJURY_OBJECT_DAYS_LEFT_OFFSET) = -1;
    assert(kbo_foreign_injury_read_live_memory(player, &live));
    assert(live.career_ending == 1u);
    assert(live.days_left == 365);
    assert(kbo_foreign_injury_live_memory_has_long_term_basis(&live, 42));

    printf("test_foreign_injury_live_memory_fields: PASS\n");
}

static void test_foreign_injury_episode_policy(void)
{
    KboForeignInjuryReplacement rec;
    KboForeignInjuryLiveMemory live;
    memset(&rec, 0, sizeof(rec));
    memset(&live, 0, sizeof(live));

    rec.team_id = 8u;
    rec.injured_player_id = 1001u;
    rec.replacement_player_id = 2001u;
    rec.expected_end_yyyymmdd = 20260627u;
    rec.injury_id = 500u;
    rec.slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR;
    rec.status = KBO_FOREIGN_INJURY_STATUS_CLOSED;
    rec.converted = 0u;

    live.active = 1u;
    live.day_to_day = 1u;
    live.injury_id = 777u;
    assert(!kbo_foreign_injury_closed_record_can_repair_on_date(&rec, &live, 20260724u, 0, 0));
    assert(!kbo_foreign_injury_live_memory_has_record_continuation_basis(&rec, &live, 20260724u));

    live.day_to_day = 0u;
    live.injury_object = 0x12345678u;
    live.injury_id = 500u;
    live.days_left = 60;
    assert(kbo_foreign_injury_closed_record_can_repair_on_date(&rec, &live, 20260724u, 0, 0));
    rec.converted = 1u;
    assert(!kbo_foreign_injury_closed_record_can_repair_on_date(&rec, &live, 20260724u, 0, 0));

    rec.converted = 0u;
    rec.status = KBO_FOREIGN_INJURY_STATUS_ACTIVE;
    rec.injury_id = 0u;
    rec.expected_end_yyyymmdd = 20260701u;
    live.days_left = 10;
    assert(kbo_foreign_injury_live_memory_has_record_continuation_basis(&rec, &live, 20260620u));
    assert(!kbo_foreign_injury_live_memory_has_record_continuation_basis(&rec, &live, 20260702u));

    uint8_t replacement[OOTP27_PLAYER_SCAN_BYTES];
    memset(replacement, 0, sizeof(replacement));
    *(uint32_t*)(replacement + OOTP27_PLAYER_ID_OFFSET) = 2001u;
    *(uint32_t*)(replacement + OOTP27_PLAYER_NATION_ID_OFFSET) = 206u;
    *(uint32_t*)(replacement + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = 4u;
    assert(!kbo_foreign_injury_replacement_player_attached_to_record(&rec, replacement));
    assert(!kbo_foreign_injury_replacement_player_can_restore_to_record(&rec, replacement));

    *(uint32_t*)(replacement + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = 8u;
    assert(kbo_foreign_injury_replacement_player_can_restore_to_record(&rec, replacement));

    test_configure_team_org_lookup();
    *(uint32_t*)(replacement + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(replacement + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 11u;
    assert(kbo_foreign_injury_replacement_player_attached_to_record(&rec, replacement));
    g_test_team_lookup_enabled = 0;

    printf("test_foreign_injury_episode_policy: PASS\n");
}

static void test_foreign_injury_foreign_count_exclusion(void)
{
    test_configure_team_org_lookup();
    memset(g_kbo_foreign_injury_replacements, 0, sizeof(g_kbo_foreign_injury_replacements));
    g_kbo_foreign_injury_replacement_count = 3;

    g_kbo_foreign_injury_replacements[0].team_id = 7u;
    g_kbo_foreign_injury_replacements[0].injured_player_id = 1001u;
    g_kbo_foreign_injury_replacements[0].expected_end_yyyymmdd = 20260701u;
    g_kbo_foreign_injury_replacements[0].status = KBO_FOREIGN_INJURY_STATUS_OPEN;

    g_kbo_foreign_injury_replacements[1].team_id = 7u;
    g_kbo_foreign_injury_replacements[1].injured_player_id = 1002u;
    g_kbo_foreign_injury_replacements[1].expected_end_yyyymmdd = 20260702u;
    g_kbo_foreign_injury_replacements[1].status = KBO_FOREIGN_INJURY_STATUS_ACTIVE;

    g_kbo_foreign_injury_replacements[2].team_id = 7u;
    g_kbo_foreign_injury_replacements[2].injured_player_id = 1003u;
    g_kbo_foreign_injury_replacements[2].status = KBO_FOREIGN_INJURY_STATUS_CLOSED;

    assert(kbo_foreign_injury_player_excluded_from_foreign_count_locked(7u, 1001u));
    assert(kbo_foreign_injury_player_excluded_from_foreign_count_locked(7u, 1002u));
    assert(!kbo_foreign_injury_player_excluded_from_foreign_count_locked(7u, 1003u));
    g_kbo_foreign_injury_replacements[0].expected_end_yyyymmdd = 0u;
    assert(!kbo_foreign_injury_player_excluded_from_foreign_count_locked(7u, 1001u));
    assert(!kbo_foreign_injury_player_excluded_from_foreign_count_locked(8u, 1001u));
    assert(!kbo_foreign_injury_player_excluded_from_foreign_count_locked(7u, 0u));

    g_kbo_foreign_injury_replacements[0].team_id = 11u;
    g_kbo_foreign_injury_replacements[0].injured_player_id = 2001u;
    g_kbo_foreign_injury_replacements[0].expected_end_yyyymmdd = 20260701u;
    g_kbo_foreign_injury_replacements[0].status = KBO_FOREIGN_INJURY_STATUS_OPEN;
    assert(kbo_team_ids_share_org(11u, 8u));
    assert(kbo_foreign_injury_player_excluded_from_foreign_count_locked(8u, 2001u));
    assert(kbo_foreign_injury_player_excluded_from_foreign_count_locked(11u, 2001u));

    g_kbo_foreign_injury_replacement_count = 0;
    memset(g_kbo_foreign_injury_replacements, 0, sizeof(g_kbo_foreign_injury_replacements));
    g_test_team_lookup_enabled = 0;
    printf("test_foreign_injury_foreign_count_exclusion: PASS\n");
}

static void test_foreign_org_snapshot_updates_on_assignment_change(void)
{
    uint8_t non_asian[OOTP27_PLAYER_SCAN_BYTES];
    uint8_t asian[OOTP27_PLAYER_SCAN_BYTES];
    memset(non_asian, 0, sizeof(non_asian));
    memset(asian, 0, sizeof(asian));

    memset(g_kbo_foreign_org_count_cache, 0, sizeof(g_kbo_foreign_org_count_cache));
    memset(g_kbo_foreign_org_snapshot, 0, sizeof(g_kbo_foreign_org_snapshot));
    memset(g_kbo_foreign_org_parent_cache_team_ids, 0, sizeof(g_kbo_foreign_org_parent_cache_team_ids));
    memset(g_kbo_foreign_org_parent_cache_org_ids, 0, sizeof(g_kbo_foreign_org_parent_cache_org_ids));
    memset(g_kbo_foreign_org_team_generation_team_ids, 0, sizeof(g_kbo_foreign_org_team_generation_team_ids));
    memset(g_kbo_foreign_org_team_generations, 0, sizeof(g_kbo_foreign_org_team_generations));
    g_kbo_foreign_org_snapshot_count = 0;
    g_kbo_foreign_org_snapshot_tick = 0u;
    g_kbo_foreign_org_count_cache_generation = 1;
    g_kbo_foreign_org_snapshot_rebuild_in_progress = 0;
    g_kbo_foreign_org_snapshot_mutation_generation = 1;
    g_kbo_foreign_org_snapshot_published_generation = 0;
    g_test_player_vector_lookup_count = 0;

    *(uint32_t*)(non_asian + OOTP27_PLAYER_ID_OFFSET) = 7001u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_NATION_ID_OFFSET) = 206u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 2u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 2u;

    *(uint32_t*)(asian + OOTP27_PLAYER_ID_OFFSET) = 7002u;
    *(uint32_t*)(asian + OOTP27_PLAYER_NATION_ID_OFFSET) = 43u;
    *(uint32_t*)(asian + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 2u;
    *(uint32_t*)(asian + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 2u;
    *(int32_t*)(asian + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET) = 100000;

    g_test_player_vector_storage[0] = (uintptr_t)non_asian;
    g_test_player_vector_storage[1] = (uintptr_t)asian;
    g_test_player_vector_count = 2;
    g_test_player_vector_enabled = 1;

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(2u, &foreign_count, &asian_count, &non_asian_count);
    assert(foreign_count == 2u);
    assert(asian_count == 1u);
    assert(non_asian_count == 1u);
    assert(g_test_player_vector_lookup_count == 1);

    uint32_t generation_before = kbo_foreign_org_count_cache_generation_for_team(2u);
    *(uint32_t*)(non_asian + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(asian + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(asian + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;

    kbo_foreign_org_count_cache_note_player_assignment_change(
        2u,
        2u,
        0u,
        0u,
        0u,
        0u,
        7001u,
        0);
    kbo_foreign_org_count_cache_note_player_assignment_change(
        2u,
        2u,
        0u,
        0u,
        0u,
        0u,
        7002u,
        1);

    foreign_count = 99u;
    asian_count = 99u;
    non_asian_count = 99u;
    kbo_count_team_asian_quota_probe(2u, &foreign_count, &asian_count, &non_asian_count);
    assert(foreign_count == 0u);
    assert(asian_count == 0u);
    assert(non_asian_count == 0u);
    assert(g_test_player_vector_lookup_count == 1);
    assert(kbo_foreign_org_count_cache_generation_for_team(2u) != generation_before);

    g_test_player_vector_enabled = 0;
    g_test_player_vector_count = 0;
    memset(g_test_player_vector_storage, 0, sizeof(g_test_player_vector_storage));
    memset(g_kbo_foreign_org_count_cache, 0, sizeof(g_kbo_foreign_org_count_cache));
    memset(g_kbo_foreign_org_snapshot, 0, sizeof(g_kbo_foreign_org_snapshot));
    g_kbo_foreign_org_snapshot_count = 0;
    g_kbo_foreign_org_snapshot_tick = 0u;
    g_kbo_foreign_org_snapshot_rebuild_in_progress = 0;
    g_kbo_foreign_org_snapshot_mutation_generation = 1;
    g_kbo_foreign_org_snapshot_published_generation = 0;
    printf("test_foreign_org_snapshot_updates_on_assignment_change: PASS\n");
}

static void test_foreign_org_live_count_ignores_stale_snapshot(void)
{
    uint8_t non_asian[OOTP27_PLAYER_SCAN_BYTES];
    memset(non_asian, 0, sizeof(non_asian));

    memset(g_kbo_foreign_org_count_cache, 0, sizeof(g_kbo_foreign_org_count_cache));
    memset(g_kbo_foreign_org_snapshot, 0, sizeof(g_kbo_foreign_org_snapshot));
    memset(g_kbo_foreign_org_parent_cache_team_ids, 0, sizeof(g_kbo_foreign_org_parent_cache_team_ids));
    memset(g_kbo_foreign_org_parent_cache_org_ids, 0, sizeof(g_kbo_foreign_org_parent_cache_org_ids));
    memset(g_kbo_foreign_org_team_generation_team_ids, 0, sizeof(g_kbo_foreign_org_team_generation_team_ids));
    memset(g_kbo_foreign_org_team_generations, 0, sizeof(g_kbo_foreign_org_team_generations));
    g_kbo_foreign_org_snapshot_count = 0;
    g_kbo_foreign_org_snapshot_tick = 0u;
    g_kbo_foreign_org_count_cache_generation = 1;
    g_kbo_foreign_org_snapshot_rebuild_in_progress = 0;
    g_kbo_foreign_org_snapshot_mutation_generation = 1;
    g_kbo_foreign_org_snapshot_published_generation = 0;
    g_test_player_vector_lookup_count = 0;

    *(uint32_t*)(non_asian + OOTP27_PLAYER_ID_OFFSET) = 7101u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_NATION_ID_OFFSET) = 206u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 2u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 2u;

    g_test_player_vector_storage[0] = (uintptr_t)non_asian;
    g_test_player_vector_count = 1;
    g_test_player_vector_enabled = 1;

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    g_kbo_foreign_org_snapshot_count = 1;
    g_kbo_foreign_org_snapshot[0].team_id = 2u;
    g_kbo_foreign_org_snapshot[0].foreign_count = 99u;
    g_kbo_foreign_org_snapshot[0].asian_count = 99u;
    g_kbo_foreign_org_snapshot[0].non_asian_count = 99u;
    g_kbo_foreign_org_snapshot_tick = 123u;

    kbo_count_team_asian_quota_probe(2u, &foreign_count, &asian_count, &non_asian_count);
    assert(foreign_count == 1u);
    assert(asian_count == 0u);
    assert(non_asian_count == 1u);
    assert(g_test_player_vector_lookup_count == 1);
    assert(g_kbo_foreign_org_snapshot_published_generation == g_kbo_foreign_org_snapshot_mutation_generation);

    kbo_foreign_org_count_cache_note_observed_assignment_change(
        0u,
        0u,
        0u,
        2u,
        2u,
        0u);
    assert(g_kbo_foreign_org_snapshot_tick == 0u);

    foreign_count = 99u;
    asian_count = 99u;
    non_asian_count = 99u;
    kbo_count_team_asian_quota_probe(2u, &foreign_count, &asian_count, &non_asian_count);
    assert(foreign_count == 1u);
    assert(asian_count == 0u);
    assert(non_asian_count == 1u);
    assert(g_test_player_vector_lookup_count == 2);

    *(uint32_t*)(non_asian + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;

    kbo_foreign_org_count_cache_note_observed_assignment_change(
        2u,
        2u,
        0u,
        0u,
        0u,
        0u);
    assert(g_kbo_foreign_org_snapshot_tick == 0u);

    foreign_count = 99u;
    asian_count = 99u;
    non_asian_count = 99u;
    kbo_count_team_asian_quota_probe(2u, &foreign_count, &asian_count, &non_asian_count);
    assert(foreign_count == 0u);
    assert(asian_count == 0u);
    assert(non_asian_count == 0u);
    assert(g_test_player_vector_lookup_count == 3);

    g_test_player_vector_enabled = 0;
    g_test_player_vector_count = 0;
    memset(g_test_player_vector_storage, 0, sizeof(g_test_player_vector_storage));
    memset(g_kbo_foreign_org_count_cache, 0, sizeof(g_kbo_foreign_org_count_cache));
    memset(g_kbo_foreign_org_snapshot, 0, sizeof(g_kbo_foreign_org_snapshot));
    g_kbo_foreign_org_snapshot_count = 0;
    g_kbo_foreign_org_snapshot_tick = 0u;
    g_kbo_foreign_org_snapshot_rebuild_in_progress = 0;
    g_kbo_foreign_org_snapshot_mutation_generation = 1;
    g_kbo_foreign_org_snapshot_published_generation = 0;
    printf("test_foreign_org_live_count_ignores_stale_snapshot: PASS\n");
}

static void test_foreign_org_count_cache_reuses_generation_without_ttl(void)
{
    uint8_t non_asian[OOTP27_PLAYER_SCAN_BYTES];
    memset(non_asian, 0, sizeof(non_asian));

    memset(g_kbo_foreign_org_count_cache, 0, sizeof(g_kbo_foreign_org_count_cache));
    memset(g_kbo_foreign_org_snapshot, 0, sizeof(g_kbo_foreign_org_snapshot));
    memset(g_kbo_foreign_org_parent_cache_team_ids, 0, sizeof(g_kbo_foreign_org_parent_cache_team_ids));
    memset(g_kbo_foreign_org_parent_cache_org_ids, 0, sizeof(g_kbo_foreign_org_parent_cache_org_ids));
    memset(g_kbo_foreign_org_team_generation_team_ids, 0, sizeof(g_kbo_foreign_org_team_generation_team_ids));
    memset(g_kbo_foreign_org_team_generations, 0, sizeof(g_kbo_foreign_org_team_generations));
    g_kbo_foreign_org_snapshot_count = 0;
    g_kbo_foreign_org_snapshot_tick = 0u;
    g_kbo_foreign_org_count_cache_generation = 1;
    g_kbo_foreign_org_snapshot_rebuild_in_progress = 0;
    g_kbo_foreign_org_snapshot_mutation_generation = 1;
    g_kbo_foreign_org_snapshot_published_generation = 0;
    g_test_player_vector_lookup_count = 0;

    *(uint32_t*)(non_asian + OOTP27_PLAYER_ID_OFFSET) = 7201u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_NATION_ID_OFFSET) = 206u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 2u;
    *(uint32_t*)(non_asian + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 2u;

    g_test_player_vector_storage[0] = (uintptr_t)non_asian;
    g_test_player_vector_count = 1;
    g_test_player_vector_enabled = 1;

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(2u, &foreign_count, &asian_count, &non_asian_count);
    assert(foreign_count == 1u);
    assert(asian_count == 0u);
    assert(non_asian_count == 1u);
    assert(g_test_player_vector_lookup_count == 1);

    g_test_player_vector_enabled = 0;
    g_test_player_vector_count = 0;
    foreign_count = 99u;
    asian_count = 99u;
    non_asian_count = 99u;
    kbo_count_team_asian_quota_probe(2u, &foreign_count, &asian_count, &non_asian_count);
    assert(foreign_count == 1u);
    assert(asian_count == 0u);
    assert(non_asian_count == 1u);
    assert(g_test_player_vector_lookup_count == 1);

    kbo_foreign_org_count_cache_note_observed_assignment_change(
        2u,
        2u,
        0u,
        0u,
        0u,
        0u);
    foreign_count = 99u;
    asian_count = 99u;
    non_asian_count = 99u;
    kbo_count_team_asian_quota_probe(2u, &foreign_count, &asian_count, &non_asian_count);
    assert(foreign_count == 0u);
    assert(asian_count == 0u);
    assert(non_asian_count == 0u);
    assert(g_test_player_vector_lookup_count == 3);

    memset(g_test_player_vector_storage, 0, sizeof(g_test_player_vector_storage));
    memset(g_kbo_foreign_org_count_cache, 0, sizeof(g_kbo_foreign_org_count_cache));
    memset(g_kbo_foreign_org_snapshot, 0, sizeof(g_kbo_foreign_org_snapshot));
    memset(g_kbo_foreign_org_parent_cache_team_ids, 0, sizeof(g_kbo_foreign_org_parent_cache_team_ids));
    memset(g_kbo_foreign_org_parent_cache_org_ids, 0, sizeof(g_kbo_foreign_org_parent_cache_org_ids));
    memset(g_kbo_foreign_org_team_generation_team_ids, 0, sizeof(g_kbo_foreign_org_team_generation_team_ids));
    memset(g_kbo_foreign_org_team_generations, 0, sizeof(g_kbo_foreign_org_team_generations));
    g_kbo_foreign_org_snapshot_count = 0;
    g_kbo_foreign_org_snapshot_tick = 0u;
    g_kbo_foreign_org_count_cache_generation = 1;
    g_kbo_foreign_org_snapshot_rebuild_in_progress = 0;
    g_kbo_foreign_org_snapshot_mutation_generation = 1;
    g_kbo_foreign_org_snapshot_published_generation = 0;
    printf("test_foreign_org_count_cache_reuses_generation_without_ttl: PASS\n");
}

static void test_flag_key_from_file_name(void)
{
    char out[64];

    /* Bare key passes through unchanged. */
    out[0] = 'X';
    assert(kbo_flag_key_from_file_name("enable_foreign_waiver_ai", out, sizeof(out)));
    assert(strcmp(out, "enable_foreign_waiver_ai") == 0);

    /* .txt suffix is stripped (case-insensitive). */
    assert(kbo_flag_key_from_file_name("enable_launcher_injection.txt", out, sizeof(out)));
    assert(strcmp(out, "enable_launcher_injection") == 0);
    assert(kbo_flag_key_from_file_name("enable_launcher_injection.TXT", out, sizeof(out)));
    assert(strcmp(out, "enable_launcher_injection") == 0);
    assert(kbo_flag_key_from_file_name("enable_launcher_injection.TxT", out, sizeof(out)));
    assert(strcmp(out, "enable_launcher_injection") == 0);

    /* Path components are trimmed; both separator styles are accepted. */
    assert(kbo_flag_key_from_file_name("C:\\Users\\u\\flags\\kbo_league_id.txt", out, sizeof(out)));
    assert(strcmp(out, "kbo_league_id") == 0);
    assert(kbo_flag_key_from_file_name("/home/u/flags/kbo_league_id.txt", out, sizeof(out)));
    assert(strcmp(out, "kbo_league_id") == 0);
    assert(kbo_flag_key_from_file_name("mixed\\path/with.txt", out, sizeof(out)));
    assert(strcmp(out, "with") == 0);

    /* A non-.txt extension is preserved as part of the key. */
    assert(kbo_flag_key_from_file_name("flag.json", out, sizeof(out)));
    assert(strcmp(out, "flag.json") == 0);

    /* The .txt strip only fires when there is a stem before the suffix.
     * A bare ".txt" is preserved verbatim — never collapses to an empty key. */
    assert(kbo_flag_key_from_file_name(".txt", out, sizeof(out)));
    assert(strcmp(out, ".txt") == 0);
    assert(kbo_flag_key_from_file_name("dir/.txt", out, sizeof(out)));
    assert(strcmp(out, ".txt") == 0);

    /* Buffer must hold the key plus the NUL terminator. */
    char tight[5];
    assert(!kbo_flag_key_from_file_name("abcde", tight, sizeof(tight)));
    assert(kbo_flag_key_from_file_name("abcd", tight, sizeof(tight)));
    assert(strcmp(tight, "abcd") == 0);
    assert(!kbo_flag_key_from_file_name("abcde.txt", tight, 5u));
    assert(kbo_flag_key_from_file_name("abcd.txt", tight, sizeof(tight)));
    assert(strcmp(tight, "abcd") == 0);

    /* NULL/empty inputs and zero-sized output buffer are rejected. */
    assert(!kbo_flag_key_from_file_name(NULL, out, sizeof(out)));
    assert(!kbo_flag_key_from_file_name("", out, sizeof(out)));
    assert(!kbo_flag_key_from_file_name("name", NULL, sizeof(out)));
    assert(!kbo_flag_key_from_file_name("name", out, 0u));

    printf("test_flag_key_from_file_name: PASS\n");
}

static void test_amateur_assignment_policy(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];

    /* kbo_amateur_player_is_hitter: position_group 1 = pitcher; anything else = hitter */
    memset(player, 0, sizeof(player));
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 1u;
    assert(!kbo_amateur_player_is_hitter(player));

    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 2u;
    assert(kbo_amateur_player_is_hitter(player));

    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 10u;
    assert(kbo_amateur_player_is_hitter(player));

    /* position_group 0 (unset) is treated as hitter (not 1) */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 0u;
    assert(kbo_amateur_player_is_hitter(player));

    assert(!kbo_amateur_player_is_hitter(NULL));

    /* kbo_amateur_player_position_bucket: each position_group maps to a named bucket */
    memset(player, 0, sizeof(player));
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 1u;  assert(kbo_amateur_player_position_bucket(player) == 0); /* P  */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 2u;  assert(kbo_amateur_player_position_bucket(player) == 1); /* C  */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 3u;  assert(kbo_amateur_player_position_bucket(player) == 2); /* 1B */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 4u;  assert(kbo_amateur_player_position_bucket(player) == 3); /* 2B */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 5u;  assert(kbo_amateur_player_position_bucket(player) == 4); /* 3B */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 6u;  assert(kbo_amateur_player_position_bucket(player) == 5); /* SS */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 7u;  assert(kbo_amateur_player_position_bucket(player) == 6); /* LF */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 8u;  assert(kbo_amateur_player_position_bucket(player) == 7); /* CF */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 9u;  assert(kbo_amateur_player_position_bucket(player) == 8); /* RF */
    /* DH (10) folds into 1B bucket for amateur balancing */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 10u; assert(kbo_amateur_player_position_bucket(player) == 2);
    /* unknown group falls through to is_hitter; 0 is treated as hitter → bucket 2 */
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] = 0u;  assert(kbo_amateur_player_position_bucket(player) == 2);
    assert(kbo_amateur_player_position_bucket(NULL) == 0);

    /* kbo_amateur_position_bucket_label: covers 0-8, default falls back to "P" */
    assert(strcmp(kbo_amateur_position_bucket_label(0), "P")  == 0);
    assert(strcmp(kbo_amateur_position_bucket_label(1), "C")  == 0);
    assert(strcmp(kbo_amateur_position_bucket_label(2), "1B") == 0);
    assert(strcmp(kbo_amateur_position_bucket_label(3), "2B") == 0);
    assert(strcmp(kbo_amateur_position_bucket_label(4), "3B") == 0);
    assert(strcmp(kbo_amateur_position_bucket_label(5), "SS") == 0);
    assert(strcmp(kbo_amateur_position_bucket_label(6), "LF") == 0);
    assert(strcmp(kbo_amateur_position_bucket_label(7), "CF") == 0);
    assert(strcmp(kbo_amateur_position_bucket_label(8), "RF") == 0);
    assert(strcmp(kbo_amateur_position_bucket_label(9),  "P") == 0); /* out-of-range → default */
    assert(strcmp(kbo_amateur_position_bucket_label(-1), "P") == 0);

    /* kbo_amateur_assignment_target_max_players follows the externalized policy defaults */
    const KboAmateurPlayerQualityPolicy* amateur_policy = kbo_amateur_player_quality_policy();
    assert(kbo_amateur_assignment_target_max_players(KBO_HIGH_SCHOOL_LEAGUE_ID) == amateur_policy->high_school_assignment_target_max_players);
    assert(kbo_amateur_assignment_target_max_players(KBO_COLLEGE_LEAGUE_ID)     == amateur_policy->college_assignment_target_max_players);
    assert(kbo_amateur_assignment_target_max_players(0u)                        == amateur_policy->college_assignment_target_max_players);

    /* kbo_amateur_quality_score: sum of four i16 value fields */
    memset(player, 0, sizeof(player));
    *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET)  = 100;
    *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET)   = 200;
    *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET)  = 300;
    *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET)   = 400;
    assert(kbo_amateur_quality_score(player) == 1000);

    memset(player, 0, sizeof(player));
    assert(kbo_amateur_quality_score(player) == 0);
    assert(kbo_amateur_quality_score(NULL) == 0);

    /* kbo_amateur_assignment_target_reputation: high school — exact boundary cases */
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 2300) == 92);
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 2299) == 84);
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 1800) == 84);
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 1799) == 72);
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 1350) == 72);
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 1349) == 58);
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 950)  == 58);
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 949)  == 45);
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 600)  == 45);
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 599)  == 35);
    assert(kbo_amateur_assignment_target_reputation(KBO_HIGH_SCHOOL_LEAGUE_ID, 0)    == 35);
    /* college: subtracts 10 from each threshold, then clamps to the college reputation cap. */
    assert(kbo_amateur_assignment_target_reputation(KBO_COLLEGE_LEAGUE_ID, 2300) == 78);
    assert(kbo_amateur_assignment_target_reputation(KBO_COLLEGE_LEAGUE_ID, 1800) == 74);
    assert(kbo_amateur_assignment_target_reputation(KBO_COLLEGE_LEAGUE_ID, 1350) == 62);
    assert(kbo_amateur_assignment_target_reputation(KBO_COLLEGE_LEAGUE_ID, 950)  == 48);
    assert(kbo_amateur_assignment_target_reputation(KBO_COLLEGE_LEAGUE_ID, 600)  == 35);
    assert(kbo_amateur_assignment_target_reputation(KBO_COLLEGE_LEAGUE_ID, 0)    == 25);

    /* kbo_amateur_assignment_team_tier: high school / default thresholds (90/80/68/56/48) */
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 255) == 5);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 90)  == 5);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 89)  == 4);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 80)  == 4);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 79)  == 3);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 68)  == 3);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 67)  == 2);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 56)  == 2);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 55)  == 1);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 48)  == 1);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 47)  == 0);
    assert(kbo_amateur_assignment_team_tier(KBO_HIGH_SCHOOL_LEAGUE_ID, 0)   == 0);
    /* college: tighter thresholds (68/55/42/28/15) */
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 68) == 5);
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 67) == 4);
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 55) == 4);
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 54) == 3);
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 42) == 3);
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 41) == 2);
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 28) == 2);
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 27) == 1);
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 15) == 1);
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 14) == 0);
    assert(kbo_amateur_assignment_team_tier(KBO_COLLEGE_LEAGUE_ID, 0)  == 0);

    /* kbo_amateur_assignment_player_tier: league_id ignored; score thresholds match target_reputation */
    assert(kbo_amateur_assignment_player_tier(0u, 2300) == 5);
    assert(kbo_amateur_assignment_player_tier(0u, 2299) == 4);
    assert(kbo_amateur_assignment_player_tier(0u, 1800) == 4);
    assert(kbo_amateur_assignment_player_tier(0u, 1799) == 3);
    assert(kbo_amateur_assignment_player_tier(0u, 1350) == 3);
    assert(kbo_amateur_assignment_player_tier(0u, 1349) == 2);
    assert(kbo_amateur_assignment_player_tier(0u, 950)  == 2);
    assert(kbo_amateur_assignment_player_tier(0u, 949)  == 1);
    assert(kbo_amateur_assignment_player_tier(0u, 600)  == 1);
    assert(kbo_amateur_assignment_player_tier(0u, 599)  == 0);
    assert(kbo_amateur_assignment_player_tier(0u, 0)    == 0);

    /* kbo_amateur_assignment_tier_allowed: |player_tier - team_tier| <= 1 */
    assert(kbo_amateur_assignment_tier_allowed(3, 3));
    assert(kbo_amateur_assignment_tier_allowed(3, 2));
    assert(kbo_amateur_assignment_tier_allowed(3, 4));
    assert(!kbo_amateur_assignment_tier_allowed(3, 1));
    assert(!kbo_amateur_assignment_tier_allowed(3, 5));
    assert(!kbo_amateur_assignment_tier_allowed(-1, 0));
    assert(!kbo_amateur_assignment_tier_allowed(0, -1));
    assert(kbo_amateur_assignment_tier_allowed(0, 0));
    assert(kbo_amateur_assignment_tier_allowed(5, 5));

    /* kbo_amateur_assignment_effective_player_tier:
     * max_team_tier < 0 → pass through unchanged; otherwise cap at max_team_tier */
    assert(kbo_amateur_assignment_effective_player_tier(5, -1) == 5);
    assert(kbo_amateur_assignment_effective_player_tier(5, 3)  == 3);
    assert(kbo_amateur_assignment_effective_player_tier(2, 3)  == 2);
    assert(kbo_amateur_assignment_effective_player_tier(3, 3)  == 3);
    assert(kbo_amateur_assignment_effective_player_tier(0, 0)  == 0);

    /* kbo_amateur_player_assignment_league_id: current league id checked first */
    memset(player, 0, sizeof(player));
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = KBO_HIGH_SCHOOL_LEAGUE_ID;
    assert(kbo_amateur_player_assignment_league_id(player) == KBO_HIGH_SCHOOL_LEAGUE_ID);

    memset(player, 0, sizeof(player));
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = KBO_COLLEGE_LEAGUE_ID;
    assert(kbo_amateur_player_assignment_league_id(player) == KBO_COLLEGE_LEAGUE_ID);

    /* original league id used when current league is not an amateur league */
    memset(player, 0, sizeof(player));
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET)  = 999u;
    *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = KBO_HIGH_SCHOOL_LEAGUE_ID;
    assert(kbo_amateur_player_assignment_league_id(player) == KBO_HIGH_SCHOOL_LEAGUE_ID);

    memset(player, 0, sizeof(player));
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET)  = 999u;
    *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = 888u;
    assert(kbo_amateur_player_assignment_league_id(player) == 0u);

    assert(kbo_amateur_player_assignment_league_id(NULL) == 0u);

    /* kbo_amateur_player_assignment_team_id: current → active → original fallback chain */
    memset(player, 0, sizeof(player));
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 42u;
    assert(kbo_amateur_player_assignment_team_id(player) == 42u);

    memset(player, 0, sizeof(player));
    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 77u;
    assert(kbo_amateur_player_assignment_team_id(player) == 77u);

    memset(player, 0, sizeof(player));
    *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = 99u;
    assert(kbo_amateur_player_assignment_team_id(player) == 99u);

    memset(player, 0, sizeof(player));
    assert(kbo_amateur_player_assignment_team_id(player) == 0u);

    assert(kbo_amateur_player_assignment_team_id(NULL) == 0u);

    printf("test_amateur_assignment_policy: PASS\n");
}

static void test_amateur_reputation_balanced_deltas_prevent_inflation(void)
{
    KboAmateurReputationUpdateRow rows[50];
    memset(rows, 0, sizeof(rows));

    for (int i = 0; i < 50; ++i) {
        rows[i].league_id = KBO_COLLEGE_LEAGUE_ID;
        rows[i].team_id = (uint32_t)(i + 1);
        rows[i].old_reputation = 60u;
        rows[i].new_reputation = 60u;
        rows[i].wins = 30u;
        rows[i].losses = 20u;
        rows[i].ties = 10u;
        rows[i].score = 1200;
    }

    int32_t raw_sum = 0;
    int32_t adjustment = 0;
    int32_t final_sum = 0;
    kbo_apply_amateur_reputation_balanced_deltas(
        KBO_COLLEGE_LEAGUE_ID,
        rows,
        50,
        &raw_sum,
        &adjustment,
        &final_sum);

    assert(raw_sum == 50);
    assert(adjustment == 1);
    assert(final_sum == 0);
    assert(rows[0].new_reputation == 62u);
    assert(rows[4].new_reputation == 61u);
    assert(rows[10].new_reputation == 60u);
    assert(rows[44].new_reputation == 59u);
    assert(rows[49].new_reputation == 58u);

    printf("test_amateur_reputation_balanced_deltas_prevent_inflation: PASS\n");
}

static void test_independent_acquisition_score_policy(void)
{
    assert(kbo_independent_acquisition_market_interest_adjustment(0) == 0);
    assert(kbo_independent_acquisition_market_interest_adjustment(1) == 2500);
    assert(kbo_independent_acquisition_market_interest_adjustment(2) == 5000);
    assert(kbo_independent_acquisition_market_interest_adjustment(3) == -9000);
    assert(kbo_independent_acquisition_market_interest_adjustment(4) == -23000);
    assert(kbo_independent_acquisition_market_interest_adjustment(9) == -93000);

    int64_t team_1_pitcher = kbo_independent_acquisition_team_need_mix_adjustment(1u, 1, 0, 0);
    int64_t team_1_hitter = kbo_independent_acquisition_team_need_mix_adjustment(1u, 0, 0, 0);
    int64_t team_2_pitcher = kbo_independent_acquisition_team_need_mix_adjustment(2u, 1, 0, 0);
    int64_t team_1_foreign_pitcher = kbo_independent_acquisition_team_need_mix_adjustment(1u, 1, 1, 0);

    assert(team_1_pitcher >= 0 && team_1_pitcher < 9000);
    assert(team_1_hitter >= 0 && team_1_hitter < 9000);
    assert(team_2_pitcher >= 0 && team_2_pitcher < 9000);
    assert(team_1_foreign_pitcher >= 0 && team_1_foreign_pitcher < 9000);
    assert(team_1_pitcher == kbo_independent_acquisition_team_need_mix_adjustment(1u, 1, 0, 0));
    assert(team_1_pitcher != team_1_hitter);
    assert(team_1_pitcher != team_2_pitcher);
    assert(team_1_pitcher != team_1_foreign_pitcher);

    printf("test_independent_acquisition_score_policy: PASS\n");
}

static void test_asian_games_wildcard_age_policy(void)
{
    assert(kbo_asian_games_policy_clamp_max_wildcards(-1) == KBO_ASIAN_GAMES_MAX_WILDCARDS_MIN);
    assert(kbo_asian_games_policy_clamp_max_wildcards(99) == KBO_ASIAN_GAMES_MAX_WILDCARDS_MAX);
    assert(kbo_asian_games_policy_clamp_max_wildcards(3) == 3);

    assert(kbo_asian_games_policy_clamp_wildcard_age_min(-5) == KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MIN);
    assert(kbo_asian_games_policy_clamp_wildcard_age_min(99) == KBO_ASIAN_GAMES_WILDCARD_AGE_MIN_MAX);

    assert(!kbo_asian_games_policy_is_wildcard_age_with_min(23u, 24));
    assert(kbo_asian_games_policy_is_wildcard_age_with_min(24u, 24));
    assert(!kbo_asian_games_policy_is_wildcard_age_with_min(24u, 25));
    assert(kbo_asian_games_policy_is_wildcard_age_with_min(25u, 25));

    printf("test_asian_games_wildcard_age_policy: PASS\n");
}

int main(void)
{
    test_core_text_and_sql_helpers();
    test_ootp_display_string_decode();
    test_json_flags_parser();
    test_award_schedule_policy_parse_event_types();
    test_news_template_render();
    test_news_related_link_parse();
    test_news_strip_link_markup();
    test_date_serial();
    test_current_date_tick_consumer_retries_save_enter_until_date_ready();
    test_current_date_tick_consumer_preserves_hooks_before_save_path_ready();
    test_current_date_tick_consumer_requires_published_dates();
    test_current_date_tick_publish_rejects_non_adjacent_live_dates();
    test_current_date_tick_live_candidate_publishable_filters_transient_dates();
    test_current_date_tick_latest_date_helpers_use_published_date();
    test_current_date_boundary_context_records_source_and_epoch();
    test_current_date_tick_sync_consumers_run_by_phase();
    test_current_date_tick_publish_and_dispatch_waits_sync_consumers();
    test_current_date_tick_save_enter_does_not_advance_live_date();
    test_current_date_tick_force_resync_allows_save_enter_rebase();
    test_foreign_waiver_date_helpers();
    test_military_csv_parse();
    test_military_date_round_trip();
    test_military_seed_line_parse();
    test_military_service_team_policy_parse();
    test_team_classification_seed_parse();
    test_fa_market_independent_source_precedence();
    test_fa_market_carryover_unsigned_metadata();
    test_allstar_csv_parse();
    test_allstar_schedule_date_slots_ignore_serializer_callbacks();
    test_foreign_replacement_seed_parse();
    test_captain_seed_parse();
    test_captain_effective_season();
    test_season_phase_effective_rules();
    test_core_csv_parse();
    test_masked_pattern_matching();
    test_patch_bytes_writers();
    test_patch_bytes_jump_recognizers();
    test_flag_key_from_file_name();
    test_fa_filing_csv_parse();
    test_salary_snapshot_csv_parse();
    test_core_atomic_file_round_trip();
    test_rule_audit_ndjson_sink();
    test_nation_table_parses_all_seed_rows();
    test_military_native_loan_on_loan_predicate();
    test_military_native_loan_clear();
    test_team_roster_arrays_contains_player();
    test_policy_data_begin();
    test_foreign_waiver_read_player_i16();
    test_foreign_waiver_value_score();
    test_player_is_foreign_for_kbo_rights();
    test_asian_quota_slot_candidate_ignores_unprepared_salary();
    test_foreign_injury_slot_label();
    test_foreign_injury_status_label();
    test_foreign_injury_policy_helpers();
    test_foreign_injury_live_memory_fields();
    test_foreign_injury_episode_policy();
    test_foreign_injury_foreign_count_exclusion();
    test_foreign_org_snapshot_updates_on_assignment_change();
    test_foreign_org_live_count_ignores_stale_snapshot();
    test_foreign_org_count_cache_reuses_generation_without_ttl();
    test_amateur_assignment_policy();
    test_amateur_reputation_balanced_deltas_prevent_inflation();
    test_independent_acquisition_score_policy();
    test_asian_games_wildcard_age_policy();
    test_policy_data_end();
    printf("All tests passed.\n");
    return 0;
}

void kbo_log_runtimef_at(const char* file, int line, const char* fmt, ...)
{
    (void)file;
    (void)line;
    (void)fmt;
}

int kbo_profiler_begin(LARGE_INTEGER* out_start)
{
    if (out_start != NULL) {
        out_start->QuadPart = 0;
    }
    return 0;
}

void kbo_profiler_end(const char* name, const LARGE_INTEGER* start)
{
    (void)name;
    (void)start;
}

int kbo_profiler_is_enabled(void)
{
    return 0;
}

void kbo_profiler_record_us(const char* name, unsigned long long elapsed_us)
{
    (void)name;
    (void)elapsed_us;
}

void kbo_profiler_reset_enabled_cache(void) {}

int kbo_get_save_scoped_data_file(const char* file_name, char* out, size_t out_size)
{
    (void)file_name;
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    return 0;
}

int kbo_get_global_data_file(const char* file_name, char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    if (file_name == NULL || file_name[0] == '\0'
            || g_rule_audit_test_dir[0] == '\0'
            || out == NULL || out_size == 0u) {
        return 0;
    }
    snprintf(out, out_size, "%s\\%s", g_rule_audit_test_dir, file_name);
    return out[0] != '\0';
}

int memory_range_readable(const void* ptr, size_t size)
{
    if (ptr == NULL || size == 0u) {
        return 0;
    }

    uintptr_t start = (uintptr_t)ptr;
    uintptr_t end = start + size;
    if (start < 0x10000u || end <= start) {
        return 0;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) {
        return 0;
    }

    DWORD protect = mbi.Protect & 0xffu;
    int readable = protect == PAGE_READONLY
        || protect == PAGE_READWRITE
        || protect == PAGE_WRITECOPY
        || protect == PAGE_EXECUTE_READ
        || protect == PAGE_EXECUTE_READWRITE
        || protect == PAGE_EXECUTE_WRITECOPY;
    uintptr_t region_end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return readable && end <= region_end;
}

void* kbo_resolve_build_specific_rva_ptr(HMODULE exe, uint32_t steam_rva)
{
    (void)exe;
    (void)steam_rva;
    return NULL;
}

int kbo_foreign_injury_player_on_inactive_replacement_roster(
    uint8_t* player,
    uint32_t player_id,
    uint32_t top_team_id,
    uint32_t today_yyyymmdd)
{
    (void)player;
    (void)player_id;
    (void)top_team_id;
    (void)today_yyyymmdd;
    return 0;
}

/* ---- Test-only stubs for symbols referenced by foreign_waiver_player_eval.c
 * functions that the tests do NOT exercise. The link must resolve them, but
 * their bodies are unreachable from any test path — keeping them inert. */

int kbo_player_pointer_plausible(uintptr_t player_ptr)
{
    return g_test_player_vector_enabled
        && player_ptr != 0u
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
}

int find_kbo_global_player_vector(uintptr_t* out_vector, int32_t* out_count, uint32_t* out_offset)
{
    g_test_player_vector_lookup_count++;
    if (out_vector != NULL) { *out_vector = 0u; }
    if (out_count != NULL) { *out_count = 0; }
    if (out_offset != NULL) { *out_offset = 0u; }
    if (!g_test_player_vector_enabled) {
        return 0;
    }
    if (out_vector != NULL) { *out_vector = (uintptr_t)g_test_player_vector_storage; }
    if (out_count != NULL) { *out_count = g_test_player_vector_count; }
    return 1;
}

void kbo_ensure_foreign_replacement_player_seeds_loaded(void) {}

int kbo_foreign_replacement_player_seed_matches_loaded(uint8_t* player, uint8_t* out_slot_type)
{
    (void)player;
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    return 0;
}

int kbo_foreign_injury_player_excluded_from_foreign_count(uint32_t team_id, uint32_t player_id)
{
    return kbo_foreign_injury_player_excluded_from_foreign_count_locked(team_id, player_id);
}

int get_kbo_asian_quota_nation_ids_path(char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    return 0;
}

uint32_t kbo_read_u32_leading_number_from_foreign_policy_file(const char* file_name)
{
    (void)file_name;
    return 0u;
}

int read_kbo_localappdata_flag_file(const char* file_name)
{
    (void)file_name;
    return 0;
}

int kbo_runtime_threads_should_continue(void)
{
    return 0;
}

int kbo_runtime_sleep_should_continue(uint32_t total_ms)
{
    (void)total_ms;
    return 0;
}

int kbo_start_runtime_thread(LPTHREAD_START_ROUTINE start, LPVOID parameter, const char* label)
{
    (void)start;
    (void)parameter;
    (void)label;
    return 0;
}

void kbo_log_runtime_line_at(const char* file, int source_line, const char* line)
{
    (void)file;
    (void)source_line;
    (void)line;
}

/* ---- Globals and stubs for amateur_assignment_policy.c ---- */

KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed[KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX];
KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed_hash[KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX];
LONG g_kbo_amateur_assignment_processed_count = 0;
LONG g_kbo_amateur_assignment_processed_hash_count = 0;
KboAmateurAssignmentProcessed g_kbo_amateur_assignment_rejected_targets[KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX];
LONG g_kbo_amateur_assignment_rejected_target_count = 0;

void kbo_amateur_assignment_clear_processed_cache(void) {}

uint32_t kbo_amateur_assignment_processed_hash_key(uint32_t player_id, uint32_t team_id)
{
    (void)player_id; (void)team_id; return 0u;
}
