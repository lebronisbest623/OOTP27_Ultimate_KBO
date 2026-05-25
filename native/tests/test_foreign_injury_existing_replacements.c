#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/foreign/injury/scanner/foreign_injury_scanner_internal.h"
#include "../src/foreign/injury/scanner/lifecycle/team_cache/foreign_injury_existing_replacements_team_cache.h"
#include "../src/foreign/injury/scanner/lifecycle/logs/foreign_injury_existing_replacements_logs.h"

KboForeignInjuryReplacement g_kbo_foreign_injury_replacements[KBO_FOREIGN_INJURY_REPLACEMENT_MAX] = {{0}};
int g_kbo_foreign_injury_replacement_count = 0;
KboLock g_kbo_foreign_injury_replacement_lock = KBO_LOCK_INIT;
char g_kbo_foreign_injury_replacement_loaded_path[MAX_PATH] = {0};
LONG g_kbo_foreign_injury_date_tick_thread_started = 0;

static uint8_t g_test_player[OOTP27_PLAYER_SCAN_BYTES];
static int g_cached_team_lookup_calls = 0;
static int g_find_player_calls = 0;
static int g_read_live_calls = 0;
static int g_repair_check_calls = 0;
static int g_reserved_check_calls = 0;
static int g_persist_calls = 0;

static void test_reset_state(void)
{
    memset(g_kbo_foreign_injury_replacements, 0, sizeof(g_kbo_foreign_injury_replacements));
    memset(g_test_player, 0, sizeof(g_test_player));
    g_kbo_foreign_injury_replacement_count = 0;
    g_cached_team_lookup_calls = 0;
    g_find_player_calls = 0;
    g_read_live_calls = 0;
    g_repair_check_calls = 0;
    g_reserved_check_calls = 0;
    g_persist_calls = 0;
}

static void test_seed_closed_record(uint8_t close_choice)
{
    g_kbo_foreign_injury_replacement_count = 1;
    g_kbo_foreign_injury_replacements[0].team_id = 8u;
    g_kbo_foreign_injury_replacements[0].league_id = 100u;
    g_kbo_foreign_injury_replacements[0].injured_player_id = 5372u;
    g_kbo_foreign_injury_replacements[0].replacement_player_id = 5306u;
    g_kbo_foreign_injury_replacements[0].opened_on_yyyymmdd = 20260301u;
    g_kbo_foreign_injury_replacements[0].expected_end_yyyymmdd = 20270310u;
    g_kbo_foreign_injury_replacements[0].closed_on_yyyymmdd = 20261012u;
    g_kbo_foreign_injury_replacements[0].status = KBO_FOREIGN_INJURY_STATUS_CLOSED;
    g_kbo_foreign_injury_replacements[0].converted = 0u;
    g_kbo_foreign_injury_replacements[0].close_choice = close_choice;
}

static void test_offseason_reset_closed_records_skip_repair_scan(void)
{
    int active_count = -1;
    int closed_count = -1;
    test_reset_state();
    test_seed_closed_record(KBO_FOREIGN_INJURY_CLOSE_OFFSEASON_RESET);

    kbo_foreign_injury_process_existing_replacements(
        20261012u,
        "test_offseason_reset",
        &active_count,
        &closed_count);

    assert(active_count == 0);
    assert(closed_count == 0);
    assert(g_kbo_foreign_injury_replacements[0].status == KBO_FOREIGN_INJURY_STATUS_CLOSED);
    assert(g_kbo_foreign_injury_replacements[0].close_choice == KBO_FOREIGN_INJURY_CLOSE_OFFSEASON_RESET);
    assert(g_cached_team_lookup_calls == 0);
    assert(g_find_player_calls == 0);
    assert(g_read_live_calls == 0);
    assert(g_reserved_check_calls == 0);
    assert(g_repair_check_calls == 0);
    assert(g_persist_calls == 0);
    printf("test_offseason_reset_closed_records_skip_repair_scan: PASS\n");
}

static void test_other_closed_records_still_check_repair_path(void)
{
    int active_count = -1;
    int closed_count = -1;
    test_reset_state();
    test_seed_closed_record(KBO_FOREIGN_INJURY_CLOSE_KEEP_INJURED);

    kbo_foreign_injury_process_existing_replacements(
        20261012u,
        "test_closed_keep_injured",
        &active_count,
        &closed_count);

    assert(active_count == 0);
    assert(closed_count == 0);
    assert(g_cached_team_lookup_calls == 1);
    assert(g_find_player_calls == 1);
    assert(g_read_live_calls == 1);
    assert(g_reserved_check_calls == 1);
    assert(g_repair_check_calls == 1);
    assert(g_persist_calls == 0);
    printf("test_other_closed_records_still_check_repair_path: PASS\n");
}

int main(void)
{
    test_offseason_reset_closed_records_skip_repair_scan();
    test_other_closed_records_still_check_repair_path();
    printf("Foreign injury existing replacement tests passed.\n");
    return 0;
}

int memory_range_readable(const void* address, SIZE_T size)
{
    (void)size;
    return address != NULL;
}

void kbo_lock_foreign_injury_replacements(void) {}
void kbo_unlock_foreign_injury_replacements(void) {}

uint8_t* kbo_foreign_injury_cached_team_lookup(
    uint32_t team_id,
    KboForeignInjuryTeamLookupCacheEntry* cache,
    int* cache_count,
    int cache_capacity)
{
    (void)team_id;
    (void)cache;
    (void)cache_count;
    (void)cache_capacity;
    g_cached_team_lookup_calls++;
    return NULL;
}

uint8_t* kbo_find_player_by_id(uint32_t player_id, uint32_t* out_current_team_id, uint32_t* out_current_league_id)
{
    (void)player_id;
    if (out_current_team_id != NULL) { *out_current_team_id = 0u; }
    if (out_current_league_id != NULL) { *out_current_league_id = 0u; }
    g_find_player_calls++;
    return g_test_player;
}

int kbo_foreign_injury_read_live_memory(uint8_t* player, KboForeignInjuryLiveMemory* out)
{
    (void)player;
    g_read_live_calls++;
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    return 1;
}

int kbo_foreign_injury_runtime_injury_present(uint8_t* player)
{
    (void)player;
    return 0;
}

int kbo_foreign_injury_roster_hold_flags_present(uint8_t* player)
{
    (void)player;
    return 0;
}

int kbo_foreign_injury_return_state_allows_close(
    uint8_t injury_active,
    int16_t days_left,
    uint8_t loan_active,
    int active_roster_present,
    int inactive_roster_present,
    int roster_hold_flags_present,
    int close_decision_allowed)
{
    (void)injury_active;
    (void)days_left;
    (void)loan_active;
    (void)active_roster_present;
    (void)inactive_roster_present;
    (void)roster_hold_flags_present;
    (void)close_decision_allowed;
    return 0;
}

int kbo_foreign_injury_replacement_player_reserved_locked(
    uint32_t replacement_player_id,
    const KboForeignInjuryReplacement* owner_rec)
{
    (void)replacement_player_id;
    (void)owner_rec;
    g_reserved_check_calls++;
    return 0;
}

int kbo_foreign_injury_closed_record_can_repair_on_date(
    const KboForeignInjuryReplacement* rec,
    const KboForeignInjuryLiveMemory* live,
    uint32_t today_yyyymmdd,
    int inactive_roster_present,
    int roster_hold_flags_present)
{
    (void)rec;
    (void)live;
    (void)today_yyyymmdd;
    (void)inactive_roster_present;
    (void)roster_hold_flags_present;
    g_repair_check_calls++;
    return 0;
}

int kbo_foreign_injury_team_inactive_roster_contains_player(uint8_t* team, uint32_t player_id)
{
    (void)team;
    (void)player_id;
    return 0;
}

int kbo_foreign_injury_team_active_roster_contains_player(uint8_t* team, uint32_t player_id)
{
    (void)team;
    (void)player_id;
    return 0;
}

int kbo_foreign_injury_status_uses_slot(uint8_t status)
{
    return status == KBO_FOREIGN_INJURY_STATUS_OPEN
        || status == KBO_FOREIGN_INJURY_STATUS_ACTIVE;
}

int kbo_foreign_injury_expected_end_pending(uint32_t today_yyyymmdd, uint32_t expected_end_yyyymmdd)
{
    return today_yyyymmdd != 0u
        && expected_end_yyyymmdd != 0u
        && today_yyyymmdd < expected_end_yyyymmdd;
}

int kbo_foreign_injury_expected_end_reached(uint32_t today_yyyymmdd, uint32_t expected_end_yyyymmdd)
{
    return today_yyyymmdd != 0u
        && expected_end_yyyymmdd != 0u
        && today_yyyymmdd >= expected_end_yyyymmdd;
}

int kbo_foreign_injury_replacement_close_decision_allowed(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    const char* source,
    const char* context)
{
    (void)league_id;
    (void)today_yyyymmdd;
    (void)source;
    (void)context;
    return 0;
}

int kbo_foreign_injury_record_has_minimum_injury_basis_on_date(
    const KboForeignInjuryReplacement* rec,
    uint32_t today)
{
    (void)rec;
    (void)today;
    return 0;
}

int kbo_foreign_injury_live_memory_has_record_continuation_basis(
    const KboForeignInjuryReplacement* rec,
    const KboForeignInjuryLiveMemory* live,
    uint32_t today_yyyymmdd)
{
    (void)rec;
    (void)live;
    (void)today_yyyymmdd;
    return 0;
}

int kbo_foreign_injury_replacement_unavailable_by_long_injury(
    const KboForeignInjuryReplacement* rec,
    uint32_t today)
{
    (void)rec;
    (void)today;
    return 0;
}

int kbo_foreign_injury_replacement_player_attached_to_record(
    const KboForeignInjuryReplacement* rec,
    uint8_t* replacement)
{
    (void)rec;
    (void)replacement;
    return 0;
}

int kbo_foreign_injury_replacement_player_can_restore_to_record(
    const KboForeignInjuryReplacement* rec,
    uint8_t* replacement)
{
    (void)rec;
    (void)replacement;
    return 0;
}

int kbo_foreign_injury_restore_active_replacement_player(
    const KboForeignInjuryReplacement* rec,
    const char* source)
{
    (void)rec;
    (void)source;
    return 1;
}

int kbo_foreign_injury_release_replacement_player(uint32_t team_id, uint32_t player_id, const char* source)
{
    (void)team_id;
    (void)player_id;
    (void)source;
    return 1;
}

int kbo_foreign_injury_release_injured_player(uint32_t team_id, uint32_t player_id, const char* source)
{
    (void)team_id;
    (void)player_id;
    (void)source;
    return 1;
}

uint32_t kbo_foreign_injury_resolve_replacement_for_record(const KboForeignInjuryReplacement* rec)
{
    (void)rec;
    return 0u;
}

int kbo_foreign_injury_injured_player_returned_to_org_roster(
    const KboForeignInjuryReplacement* rec,
    uint8_t* injured)
{
    (void)rec;
    (void)injured;
    return 0;
}

int kbo_foreign_injury_choose_returning_player(
    const KboForeignInjuryReplacement* rec,
    uint8_t* injured,
    uint8_t* replacement,
    KboForeignInjuryReplacementDecision* out)
{
    (void)rec;
    (void)injured;
    (void)replacement;
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->choice = KBO_FOREIGN_INJURY_DECISION_KEEP_INJURED;
    }
    return 1;
}

void kbo_foreign_injury_log_existing_replacement_wait(
    KboForeignInjuryExistingWaitLogKind kind,
    const KboForeignInjuryExistingWaitLogContext* context)
{
    (void)kind;
    (void)context;
}

void kbo_foreign_injury_emit_active_replacement_news_batch(
    const KboForeignInjuryReplacement* active_news,
    int active_count,
    uint32_t today,
    const char* source)
{
    (void)active_news;
    (void)active_count;
    (void)today;
    (void)source;
}

void kbo_foreign_injury_emit_closed_news_batch(
    const KboForeignInjuryClosedNews* closed_news,
    int closed_count,
    uint32_t today,
    const char* source)
{
    (void)closed_news;
    (void)closed_count;
    (void)today;
    (void)source;
}

int kbo_persist_foreign_injury_replacements_locked(void)
{
    g_persist_calls++;
    return 1;
}

void kbo_log_runtimef_at(const char* file, int line, const char* fmt, ...)
{
    (void)file;
    (void)line;
    (void)fmt;
}

void kbo_log_fields_init(KboLogFields* fields)
{
    if (fields != NULL) {
        memset(fields, 0, sizeof(*fields));
    }
}

void kbo_log_field_u32(KboLogFields* fields, const char* name, uint32_t value)
{
    (void)fields;
    (void)name;
    (void)value;
}

void kbo_rule_audit_emit_fields(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    const KboLogFields* fields)
{
    (void)rule;
    (void)decision;
    (void)reason;
    (void)source;
    (void)fields;
}
