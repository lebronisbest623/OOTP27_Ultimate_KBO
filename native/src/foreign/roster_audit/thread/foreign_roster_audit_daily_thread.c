#include "../internal/foreign_roster_audit_internal.h"
#include "../../../team/add_player_guard/team_add_player_guard_ai_roster.h"
#include "../../../team/independent_acquisition/independent_acquisition_ai.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../retention_guard/foreign_retention_guard.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../../../fa_declaration/fa_declaration.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/core_flags/json/json_bool_parser.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"

enum {
    KBO_FOREIGN_ROSTER_DAILY_FA_REPAIR_CURRENT_MIN_WALL_MS = 30000u,
    KBO_FOREIGN_ROSTER_DAILY_FA_REPAIR_PREVIOUS_MIN_WALL_MS = 120000u
};

#define KBO_FOREIGN_ROSTER_DAILY_AUDIT_STATE_FILE "kbo_daily_audit_state.json"

static int kbo_foreign_roster_daily_audit_state_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_FOREIGN_ROSTER_DAILY_AUDIT_STATE_FILE,
        out,
        out_size);
}

static uint32_t kbo_foreign_roster_daily_load_last_audit_date(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_foreign_roster_daily_audit_state_path(path, sizeof(path))) {
        return 0u;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0u;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > KBO_FLAGS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return 0u;
    }

    char* json = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (json == NULL) {
        CloseHandle(file);
        return 0u;
    }

    DWORD read = 0u;
    int ok = ReadFile(file, json, size, &read, NULL) && read == size;
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, json);
        return 0u;
    }

    int value = 0;
    uint32_t result = 0u;
    if (kbo_find_int_value_in_json(json, read, "last_audit_date", &value)
            && value >= 19820101
            && value <= 22001231) {
        result = (uint32_t)value;
    } else {
        kbo_log_runtimef(
            "foreign roster daily audit state ignored source=%s reason=invalid_last_audit_date path=%s",
            source != NULL ? source : "",
            path);
    }
    HeapFree(GetProcessHeap(), 0, json);
    return result;
}

static void kbo_foreign_roster_daily_persist_last_audit_date(uint32_t today, const char* source)
{
    if (today == 0u) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_foreign_roster_daily_audit_state_path(path, sizeof(path))) {
        return;
    }

    char json[128] = {0};
    int len = snprintf(
        json,
        sizeof(json),
        "{\r\n  \"last_audit_date\": %u\r\n}\r\n",
        today);
    if (len <= 0 || (size_t)len >= sizeof(json)) {
        return;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "foreign roster daily audit state persist skipped source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            today,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    DWORD written = 0u;
    DWORD write_len = (DWORD)len;
    if (!WriteFile(file, json, write_len, &written, NULL) || written != write_len) {
        kbo_log_runtimef(
            "foreign roster daily audit state persist failed source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            today,
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}

static uint32_t kbo_foreign_roster_daily_first_catchup_date(uint32_t last_audit_date, uint32_t today)
{
    if (today == 0u) {
        return 0u;
    }
    if (last_audit_date != 0u && last_audit_date < today) {
        uint32_t next_date = kbo_add_days_yyyymmdd(last_audit_date, 1u);
        if (next_date != 0u && next_date <= today) {
            return next_date;
        }
    }
    return today;
}

static int kbo_foreign_roster_daily_abort_if_save(const char* stage, uint32_t today)
{
    if (!kbo_runtime_save_in_progress()) {
        return 0;
    }

    kbo_log_runtimef(
        "foreign roster daily audit deferred reason=save_in_progress stage=%s today=%u",
        stage != NULL ? stage : "",
        today);
    return 1;
}

DWORD WINAPI kbo_foreign_roster_daily_audit_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("foreign roster daily audit thread started");

    uint32_t last_audit_date = 0u;
    uint32_t last_fa_repair_current_season = 0u;
    uint32_t last_fa_repair_previous_season = 0u;
    DWORD last_fa_repair_current_tick = 0u;
    DWORD last_fa_repair_previous_tick = 0u;
    char last_audit_save_path[MAX_PATH] = {0};

    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "foreign_roster_daily_audit",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->foreign_roster_daily_audit_sleep_ms)) {
            break;
        }
        if (!kbo_runtime_pause_for_save_if_needed("foreign_roster_daily_audit")) {
            break;
        }

        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
        uint32_t today = work.date;
        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
            break;
        }

        if (last_audit_save_path[0] == '\0' || strcmp(last_audit_save_path, save_path) != 0) {
            snprintf(last_audit_save_path, sizeof(last_audit_save_path), "%s", save_path);
            last_audit_date = kbo_foreign_roster_daily_load_last_audit_date(
                "foreign_roster_daily_save_scope");
        }
        if (today == 0u || today == last_audit_date) {
            kbo_current_date_tick_consumer_mark_processed(&consumer);
            continue;
        }
        if (!kbo_runtime_pause_for_save_if_needed("foreign_roster_daily_date_change")) {
            break;
        }

        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_tick);

        int aborted_tick = 0;
        uint32_t catchup_date = kbo_foreign_roster_daily_first_catchup_date(last_audit_date, today);
        while (catchup_date != 0u && catchup_date <= today) {
            if (kbo_foreign_roster_daily_abort_if_save("date_sensitive_loop", catchup_date)) {
                aborted_tick = 1;
                break;
            }

            KBO_PROFILE_BEGIN(profile_foreign_roster_daily_independent_acquisition);
            kbo_run_independent_team_acquisition_ai_for_date(
                catchup_date,
                "foreign_roster_daily_date_change");
            KBO_PROFILE_END(profile_foreign_roster_daily_independent_acquisition, "foreign_roster.daily.independent_acquisition");
            if (kbo_foreign_roster_daily_abort_if_save("after_independent_acquisition", catchup_date)) {
                aborted_tick = 1;
                break;
            }

            if (catchup_date == today) {
                break;
            }
            uint32_t next_date = kbo_add_days_yyyymmdd(catchup_date, 1u);
            if (next_date == 0u || next_date <= catchup_date) {
                break;
            }
            catchup_date = next_date;
        }
        if (aborted_tick) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.date_sensitive_loop");
            goto defer_current_work;
        }

        if (kbo_foreign_roster_daily_abort_if_save("before_rights_sync", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.before_rights_sync");
            goto defer_current_work;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_rights_sync);
        kbo_sync_active_foreign_waiver_rights_to_memory(
            "foreign_roster_daily_date_change",
            today);
        KBO_PROFILE_END(profile_foreign_roster_daily_rights_sync, "foreign_roster.daily.rights_sync");
        if (kbo_foreign_roster_daily_abort_if_save("after_rights_sync", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_rights_sync");
            goto defer_current_work;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_ai_callup);
        kbo_run_foreign_ai_roster_daily_callup("foreign_roster_daily_date_change");
        KBO_PROFILE_END(profile_foreign_roster_daily_ai_callup, "foreign_roster.daily.ai_roster_callup");
        if (kbo_foreign_roster_daily_abort_if_save("after_ai_roster_callup", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_ai_roster_callup");
            goto defer_current_work;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_retention);
        kbo_foreign_retention_guard_repair("foreign_roster_daily_date_change");
        KBO_PROFILE_END(profile_foreign_roster_daily_retention, "foreign_roster.daily.retention_guard");
        if (kbo_foreign_roster_daily_abort_if_save("after_retention_guard", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_retention_guard");
            goto defer_current_work;
        }
        uint32_t season = today / 10000u;
        DWORD now = GetTickCount();
        int run_current_fa_repair = season != last_fa_repair_current_season
            || last_fa_repair_current_tick == 0u
            || now - last_fa_repair_current_tick >= KBO_FOREIGN_ROSTER_DAILY_FA_REPAIR_CURRENT_MIN_WALL_MS;
        if (run_current_fa_repair) {
            KBO_PROFILE_BEGIN(profile_foreign_roster_daily_fa_repair_current);
            kbo_fa_declaration_repair_retained_contracts_for_season(
                season,
                "foreign_roster_daily_date_change");
            KBO_PROFILE_END(profile_foreign_roster_daily_fa_repair_current, "foreign_roster.daily.fa_repair_current");
            last_fa_repair_current_season = season;
            last_fa_repair_current_tick = now;
        } else {
            KBO_PROFILE_BEGIN(profile_foreign_roster_daily_fa_repair_current_cached);
            KBO_PROFILE_END(profile_foreign_roster_daily_fa_repair_current_cached, "foreign_roster.daily.fa_repair_current_cached");
        }
        if (kbo_foreign_roster_daily_abort_if_save("after_fa_repair_current", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_fa_repair_current");
            goto defer_current_work;
        }
        if (season > 1982u) {
            uint32_t previous_season = season - 1u;
            int run_previous_fa_repair = previous_season != last_fa_repair_previous_season
                || last_fa_repair_previous_tick == 0u
                || now - last_fa_repair_previous_tick >= KBO_FOREIGN_ROSTER_DAILY_FA_REPAIR_PREVIOUS_MIN_WALL_MS;
            if (run_previous_fa_repair) {
                KBO_PROFILE_BEGIN(profile_foreign_roster_daily_fa_repair_previous);
                kbo_fa_declaration_repair_retained_contracts_for_season(
                    previous_season,
                    "foreign_roster_daily_date_change_previous_season");
                KBO_PROFILE_END(profile_foreign_roster_daily_fa_repair_previous, "foreign_roster.daily.fa_repair_previous");
                last_fa_repair_previous_season = previous_season;
                last_fa_repair_previous_tick = now;
            } else {
                KBO_PROFILE_BEGIN(profile_foreign_roster_daily_fa_repair_previous_cached);
                KBO_PROFILE_END(profile_foreign_roster_daily_fa_repair_previous_cached, "foreign_roster.daily.fa_repair_previous_cached");
            }
        }
        if (kbo_foreign_roster_daily_abort_if_save("after_fa_repair_previous", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_fa_repair_previous");
            goto defer_current_work;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_audit);
        audit_foreign_roster_state("foreign_roster_daily_date_change", 1);
        KBO_PROFILE_END(profile_foreign_roster_daily_audit, "foreign_roster.daily.audit");
        KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.tick");
        last_audit_date = today;
        kbo_foreign_roster_daily_persist_last_audit_date(
            today,
            "foreign_roster_daily_date_change");
        kbo_current_date_tick_consumer_mark_processed(&consumer);
        continue;

defer_current_work:
        break;
        }
    }

    InterlockedExchange(&g_kbo_foreign_roster_daily_audit_started, 0);
    kbo_log_runtime_line("foreign roster daily audit thread stopped");
    return 0;
}

void start_kbo_foreign_roster_daily_audit_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_foreign_roster_daily_audit_started, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(kbo_foreign_roster_daily_audit_thread, NULL, "foreign roster daily audit")) {
        InterlockedExchange(&g_kbo_foreign_roster_daily_audit_started, 0);
    }
}
