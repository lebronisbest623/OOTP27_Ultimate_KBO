#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/core/dates/tick/current_date_tick_capture.h"
#include "../src/service_time_guard/service_time_guard.h"
#include "../src/team/classification/parse/team_classification_seed_parse.h"

static uint8_t g_player[OOTP27_PLAYER_SCAN_BYTES];
static uint8_t g_sang_team[OOTP27_KBO_TEAM_READABLE_BYTES];
static uint8_t g_kpb_team[OOTP27_KBO_TEAM_READABLE_BYTES];
static uint32_t g_military_policy_team_id = 0u;
static uint32_t g_independent_futures_team_id = 0u;
static uint32_t g_independent_league_team_id = 0u;

int memory_range_readable(const void* address, SIZE_T size)
{
    if (address == NULL || size == 0u) {
        return 0;
    }

    uintptr_t start = (uintptr_t)address;
    uintptr_t end = start + (uintptr_t)size;
    if (end < start) {
        return 0;
    }

    const struct {
        uintptr_t start;
        uintptr_t end;
    } ranges[] = {
        { (uintptr_t)g_player, (uintptr_t)g_player + sizeof(g_player) },
        { (uintptr_t)g_sang_team, (uintptr_t)g_sang_team + sizeof(g_sang_team) },
        { (uintptr_t)g_kpb_team, (uintptr_t)g_kpb_team + sizeof(g_kpb_team) }
    };

    for (size_t i = 0; i < sizeof(ranges) / sizeof(ranges[0]); i++) {
        if (start >= ranges[i].start && end <= ranges[i].end) {
            return 1;
        }
    }
    return 0;
}

int kbo_player_pointer_plausible(uintptr_t player_ptr)
{
    return player_ptr == (uintptr_t)g_player;
}

uint8_t* find_kbo_team_by_csv_id_any_league(const char* csv_id, int allow_inactive)
{
    (void)allow_inactive;
    if (csv_id != NULL && _stricmp(csv_id, "SANG") == 0) {
        return g_sang_team;
    }
    if (csv_id != NULL && _stricmp(csv_id, "KPB") == 0) {
        return g_kpb_team;
    }
    return NULL;
}

int kbo_team_id_is_military_service_team(uint32_t team_id)
{
    return team_id != 0u && team_id == g_military_policy_team_id;
}

int kbo_team_classification_independent_kind_for_team(uint32_t team_id)
{
    if (team_id != 0u && team_id == g_independent_futures_team_id) {
        return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES;
    }
    if (team_id != 0u && team_id == g_independent_league_team_id) {
        return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE;
    }
    return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE;
}

int kbo_fix_enabled(void) { return 1; }
int kbo_runtime_save_in_progress(void) { return 0; }
int kbo_runtime_pause_for_save_if_needed(const char* source) { (void)source; return 1; }
int kbo_runtime_threads_should_continue(void) { return 0; }
int kbo_runtime_sleep_should_continue(uint32_t total_ms) { (void)total_ms; return 0; }
int kbo_start_runtime_thread(LPTHREAD_START_ROUTINE start, LPVOID parameter, const char* label)
{
    (void)start;
    (void)parameter;
    (void)label;
    return 0;
}
int find_kbo_global_player_vector(uintptr_t* out_vector, int32_t* out_count, uint32_t* out_offset)
{
    if (out_vector != NULL) { *out_vector = 0u; }
    if (out_count != NULL) { *out_count = 0; }
    if (out_offset != NULL) { *out_offset = 0u; }
    return 0;
}
void kbo_current_date_tick_consumer_init(
    KboCurrentDateTickConsumer* consumer,
    const char* label,
    uint32_t flags)
{
    (void)consumer;
    (void)label;
    (void)flags;
}
int kbo_current_date_tick_consumer_next(
    KboCurrentDateTickConsumer* consumer,
    KboCurrentDateTickWork* out_work)
{
    (void)consumer;
    (void)out_work;
    return 0;
}
void kbo_current_date_tick_consumer_mark_processed(KboCurrentDateTickConsumer* consumer)
{
    (void)consumer;
}
void kbo_log_runtime_line_at(const char* file, int line, const char* message)
{
    (void)file;
    (void)line;
    (void)message;
}
void kbo_log_runtimef_at(const char* file, int line, const char* format, ...)
{
    (void)file;
    (void)line;
    (void)format;
}

static void seed_team(uint8_t* team, uint32_t team_id)
{
    memset(team, 0, OOTP27_KBO_TEAM_READABLE_BYTES);
    *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) = team_id;
}

static void seed_player(uint32_t player_id, uint32_t current_team_id, uint32_t loan_team_id, uint32_t service_days)
{
    memset(g_player, 0, sizeof(g_player));
    *(uint32_t*)(g_player + OOTP27_PLAYER_ID_OFFSET) = player_id;
    *(uint32_t*)(g_player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = current_team_id;
    *(uint32_t*)(g_player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = loan_team_id;
    *(uint16_t*)(g_player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET) = (uint16_t)service_days;
}

static void test_military_policy_team_service_time_is_frozen(void)
{
    kbo_service_time_guard_reset_for_tests();
    g_military_policy_team_id = 10u;
    seed_player(1001u, 10u, 0u, 1145u);

    KboServiceTimeGuardApplyResult result = {0};
    kbo_service_time_guard_apply_player(g_player, 20260401u, "test", &result);
    assert(result.protected_player);
    assert(result.baseline_registered);
    assert(kbo_service_time_guard_player_days(g_player) == 1145u);

    *(uint16_t*)(g_player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET) = 1146u;
    memset(&result, 0, sizeof(result));
    assert(kbo_service_time_guard_apply_player(g_player, 20260402u, "test", &result));
    assert(result.service_time_restored);
    assert(kbo_service_time_guard_player_days(g_player) == 1145u);

    printf("test_military_policy_team_service_time_is_frozen: PASS\n");
}

static void test_kpb_fallback_team_service_time_is_frozen(void)
{
    kbo_service_time_guard_reset_for_tests();
    g_military_policy_team_id = 0u;
    seed_player(1002u, 0u, 70u, 500u);

    KboServiceTimeGuardApplyResult result = {0};
    kbo_service_time_guard_apply_player(g_player, 20260401u, "test", &result);
    assert(result.protected_player);
    assert(result.baseline_registered);

    *(uint16_t*)(g_player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET) = 501u;
    assert(kbo_service_time_guard_apply_player(g_player, 20260402u, "test", &result));
    assert(kbo_service_time_guard_player_days(g_player) == 500u);

    printf("test_kpb_fallback_team_service_time_is_frozen: PASS\n");
}

static void test_independent_futures_team_service_time_is_frozen(void)
{
    kbo_service_time_guard_reset_for_tests();
    g_independent_futures_team_id = 28u;
    seed_player(1003u, 28u, 0u, 210u);

    KboServiceTimeGuardApplyResult result = {0};
    kbo_service_time_guard_apply_player(g_player, 20260401u, "test", &result);
    assert(result.protected_player);
    assert(result.baseline_registered);

    *(uint16_t*)(g_player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET) = 220u;
    assert(kbo_service_time_guard_apply_player(g_player, 20260402u, "test", &result));
    assert(kbo_service_time_guard_player_days(g_player) == 210u);

    printf("test_independent_futures_team_service_time_is_frozen: PASS\n");
}

static void test_true_independent_league_team_is_not_frozen(void)
{
    kbo_service_time_guard_reset_for_tests();
    g_independent_league_team_id = 91u;
    seed_player(1004u, 91u, 0u, 100u);

    KboServiceTimeGuardApplyResult result = {0};
    assert(!kbo_service_time_guard_apply_player(g_player, 20260401u, "test", &result));
    assert(!result.protected_player);

    printf("test_true_independent_league_team_is_not_frozen: PASS\n");
}

int main(void)
{
    seed_team(g_sang_team, 60u);
    seed_team(g_kpb_team, 70u);

    test_military_policy_team_service_time_is_frozen();
    test_kpb_fallback_team_service_time_is_frozen();
    test_independent_futures_team_service_time_is_frozen();
    test_true_independent_league_team_is_not_frozen();
    printf("Service-time guard tests passed.\n");
    return 0;
}
