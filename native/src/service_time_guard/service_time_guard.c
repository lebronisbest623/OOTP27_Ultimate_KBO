#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../bootstrap/abi/ootp_offsets.h"
#include "../core/core_flags/api/flags_api.h"
#include "../core/dates/tick/current_date_tick_capture.h"
#include "../core/logging/core_log.h"
#include "../military_service/players/team_policy/military_service_team_policy.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/classification/team_classification.h"
#include "../team/lookup/team_lookup.h"
#include "service_time_guard.h"

#define KBO_SERVICE_TIME_GUARD_PLAYER_COUNT_MAX_PLAUSIBLE 200000
#define KBO_SERVICE_TIME_GUARD_SAVE_CHECK_INTERVAL 128
#define KBO_SERVICE_TIME_GUARD_PULSE_MS 5000u
#define KBO_SERVICE_TIME_GUARD_LOG_BURST 160
#define KBO_SERVICE_TIME_GUARD_CONSUMER_FLAGS \
    (KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER \
        | KBO_CURRENT_DATE_TICK_CONSUMER_COALESCE_TO_LATEST)

typedef struct KboServiceTimeGuardRecord {
    uint32_t player_id;
    uint32_t protected_team_id;
    uint32_t baseline_service_time;
    uint32_t last_seen_date;
    int active;
} KboServiceTimeGuardRecord;

typedef struct KboServiceTimeGuardScanResult {
    int scanned;
    int protected_players;
    int baselines_registered;
    int service_time_restored;
    int aborted_for_save;
    int vector_unavailable;
    uint32_t vector_offset;
} KboServiceTimeGuardScanResult;

static KboServiceTimeGuardRecord g_service_time_guard_records[OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS];
static volatile LONG g_service_time_guard_record_count = 0;
static volatile LONG g_service_time_guard_thread_started = 0;
static volatile LONG g_service_time_guard_log_count = 0;

uint32_t kbo_service_time_guard_player_days(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(
            player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET,
            sizeof(uint16_t))) {
        return 0u;
    }
    return (uint32_t)*(uint16_t*)(player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET);
}

int kbo_service_time_guard_set_player_days(uint8_t* player, uint32_t service_days)
{
    if (player == NULL || !memory_range_readable(
            player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET,
            sizeof(uint16_t))) {
        return 0;
    }

    if (service_days > UINT16_MAX) {
        service_days = UINT16_MAX;
    }
    *(uint16_t*)(player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET) = (uint16_t)service_days;
    return 1;
}

static int kbo_service_time_guard_team_matches_csv_id(uint32_t team_id, const char* csv_id)
{
    if (team_id == 0u || csv_id == NULL || csv_id[0] == '\0') {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_csv_id_any_league(csv_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) == team_id;
}

int kbo_service_time_guard_team_excluded(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }
    if (kbo_team_id_is_military_service_team(team_id)
            || kbo_service_time_guard_team_matches_csv_id(team_id, "SANG")
            || kbo_service_time_guard_team_matches_csv_id(team_id, "KPB")) {
        return 1;
    }
    return kbo_team_classification_independent_kind_for_team(team_id)
        == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES;
}

static uint32_t kbo_service_time_guard_protected_team_for_player(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0u;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    if (kbo_service_time_guard_team_excluded(current_team_id)) {
        return current_team_id;
    }
    if (kbo_service_time_guard_team_excluded(loan_team_id)) {
        return loan_team_id;
    }
    return 0u;
}

static KboServiceTimeGuardRecord* kbo_service_time_guard_find_record(uint32_t player_id)
{
    if (player_id == 0u) {
        return NULL;
    }

    LONG count = InterlockedCompareExchange(&g_service_time_guard_record_count, 0, 0);
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    for (LONG i = 0; i < count; i++) {
        if (g_service_time_guard_records[i].player_id == player_id) {
            return &g_service_time_guard_records[i];
        }
    }
    return NULL;
}

static KboServiceTimeGuardRecord* kbo_service_time_guard_get_or_add_record(uint32_t player_id)
{
    KboServiceTimeGuardRecord* existing = kbo_service_time_guard_find_record(player_id);
    if (existing != NULL) {
        return existing;
    }

    LONG slot = InterlockedIncrement(&g_service_time_guard_record_count) - 1;
    if (slot < 0 || slot >= OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        InterlockedDecrement(&g_service_time_guard_record_count);
        return NULL;
    }

    KboServiceTimeGuardRecord* record = &g_service_time_guard_records[slot];
    memset(record, 0, sizeof(*record));
    record->player_id = player_id;
    return record;
}

int kbo_service_time_guard_apply_player(
    uint8_t* player,
    uint32_t date_yyyymmdd,
    const char* source,
    KboServiceTimeGuardApplyResult* out_result)
{
    KboServiceTimeGuardApplyResult result = {0};
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        if (out_result != NULL) { *out_result = result; }
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        if (out_result != NULL) { *out_result = result; }
        return 0;
    }

    uint32_t protected_team_id = kbo_service_time_guard_protected_team_for_player(player);
    KboServiceTimeGuardRecord* record = kbo_service_time_guard_find_record(player_id);
    if (protected_team_id == 0u) {
        if (record != NULL) {
            record->active = 0;
            record->protected_team_id = 0u;
        }
        if (out_result != NULL) { *out_result = result; }
        return 0;
    }

    result.protected_player = 1;
    result.protected_team_id = protected_team_id;
    result.service_time_before = kbo_service_time_guard_player_days(player);

    record = kbo_service_time_guard_get_or_add_record(player_id);
    if (record == NULL) {
        if (out_result != NULL) { *out_result = result; }
        return 0;
    }

    if (!record->active || record->protected_team_id != protected_team_id) {
        record->active = 1;
        record->protected_team_id = protected_team_id;
        record->baseline_service_time = result.service_time_before;
        record->last_seen_date = date_yyyymmdd;
        result.baseline_registered = 1;
        result.baseline_service_time = record->baseline_service_time;
        if (out_result != NULL) { *out_result = result; }
        (void)source;
        return 0;
    }

    if (result.service_time_before < record->baseline_service_time) {
        record->baseline_service_time = result.service_time_before;
    }
    record->last_seen_date = date_yyyymmdd;
    result.baseline_service_time = record->baseline_service_time;

    if (result.service_time_before > record->baseline_service_time
            && kbo_service_time_guard_set_player_days(player, record->baseline_service_time)) {
        result.service_time_restored = 1;
    }
    if (out_result != NULL) { *out_result = result; }
    return result.service_time_restored;
}

static int kbo_service_time_guard_scan_players(
    uint32_t date_yyyymmdd,
    const char* source,
    KboServiceTimeGuardScanResult* out_result)
{
    KboServiceTimeGuardScanResult result = {0};
    if (out_result != NULL) {
        *out_result = result;
    }
    if (!kbo_fix_enabled()) {
        return 0;
    }
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "service_time_guard")) {
        return 0;
    }
    if (kbo_runtime_save_in_progress()) {
        result.aborted_for_save = 1;
        if (out_result != NULL) { *out_result = result; }
        return 0;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    uint32_t vector_offset = 0u;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)
            || player_vector == 0u
            || player_count <= 0
            || player_count > KBO_SERVICE_TIME_GUARD_PLAYER_COUNT_MAX_PLAUSIBLE) {
        result.vector_unavailable = 1;
        if (out_result != NULL) { *out_result = result; }
        return 0;
    }
    result.vector_offset = vector_offset;

    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        result.vector_unavailable = 1;
        if (out_result != NULL) { *out_result = result; }
        return 0;
    }

    uintptr_t* player_snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (player_snapshot == NULL) {
        if (out_result != NULL) { *out_result = result; }
        return 0;
    }

    SIZE_T bytes_read = 0u;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            player_snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        HeapFree(GetProcessHeap(), 0, player_snapshot);
        result.vector_unavailable = 1;
        if (out_result != NULL) { *out_result = result; }
        return 0;
    }

    for (int32_t i = 0; i < player_count; i++) {
        if ((i % KBO_SERVICE_TIME_GUARD_SAVE_CHECK_INTERVAL) == 0
                && kbo_runtime_save_in_progress()) {
            result.aborted_for_save = 1;
            break;
        }

        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        KboServiceTimeGuardApplyResult apply_result = {0};
        kbo_service_time_guard_apply_player(
            (uint8_t*)player_ptr,
            date_yyyymmdd,
            source,
            &apply_result);
        result.scanned++;
        if (apply_result.protected_player) {
            result.protected_players++;
        }
        if (apply_result.baseline_registered) {
            result.baselines_registered++;
        }
        if (apply_result.service_time_restored) {
            result.service_time_restored++;
            LONG restore_log = InterlockedIncrement(&g_service_time_guard_log_count);
            if (restore_log <= KBO_SERVICE_TIME_GUARD_LOG_BURST) {
                kbo_log_runtimef(
                    "KBO service-time guard restored source=%s player=%u team=%u before=%u baseline=%u date=%u",
                    source != NULL ? source : "",
                    *(uint32_t*)((uint8_t*)player_ptr + OOTP27_PLAYER_ID_OFFSET),
                    apply_result.protected_team_id,
                    apply_result.service_time_before,
                    apply_result.baseline_service_time,
                    date_yyyymmdd);
            }
        }
    }

    HeapFree(GetProcessHeap(), 0, player_snapshot);
    if (out_result != NULL) {
        *out_result = result;
    }
    return result.service_time_restored;
}

static int kbo_service_time_guard_process_work(
    KboCurrentDateTickConsumer* consumer,
    const KboCurrentDateTickWork* work)
{
    if (consumer == NULL || work == NULL) {
        return 1;
    }

    const char* source = work->site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA
        ? "service_time_guard_save_enter"
        : "service_time_guard_post_advance";
    KboServiceTimeGuardScanResult result = {0};
    kbo_service_time_guard_scan_players(work->date, source, &result);
    if (result.aborted_for_save) {
        return 0;
    }

    LONG log_index = InterlockedIncrement(&g_service_time_guard_log_count);
    if (result.service_time_restored > 0
            || result.baselines_registered > 0
            || result.vector_unavailable
            || log_index <= 20) {
        kbo_log_runtimef(
            "KBO service-time guard tick source=%s date=%u scanned=%d protected=%d baselines=%d restored=%d vector_unavailable=%d vector_off=0x%x",
            source,
            work->date,
            result.scanned,
            result.protected_players,
            result.baselines_registered,
            result.service_time_restored,
            result.vector_unavailable,
            result.vector_offset);
    }

    kbo_current_date_tick_consumer_mark_processed(consumer);
    return 1;
}

DWORD WINAPI kbo_service_time_guard_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO service-time guard thread started");

    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "service_time_guard",
        KBO_SERVICE_TIME_GUARD_CONSUMER_FLAGS);

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(KBO_SERVICE_TIME_GUARD_PULSE_MS)) {
            break;
        }

        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
            if (!kbo_service_time_guard_process_work(&consumer, &work)) {
                break;
            }
        }
    }

    InterlockedExchange(&g_service_time_guard_thread_started, 0);
    kbo_log_runtime_line("KBO service-time guard thread stopped");
    return 0;
}

void start_kbo_service_time_guard_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_service_time_guard_thread_started, 1, 0) != 0) {
        return;
    }
    if (!kbo_start_runtime_thread(kbo_service_time_guard_thread, NULL, "service-time guard")) {
        InterlockedExchange(&g_service_time_guard_thread_started, 0);
    }
}

void kbo_service_time_guard_reset_for_tests(void)
{
    memset(g_service_time_guard_records, 0, sizeof(g_service_time_guard_records));
    InterlockedExchange(&g_service_time_guard_record_count, 0);
    InterlockedExchange(&g_service_time_guard_log_count, 0);
}
