#include "../internal/foreign_roster_audit_internal.h"
#include "../../../team/add_player_guard/team_add_player_guard_ai_roster.h"
#include "../../../team/independent_acquisition/independent_acquisition_ai.h"
#include "../../retention_guard/foreign_retention_guard.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../../../fa_declaration/fa_declaration.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../../core/dates/constants/kbo_date_constants.h"

enum {
    KBO_FOREIGN_ROSTER_DAILY_FA_REPAIR_CURRENT_MIN_WALL_MS = 30000u,
    KBO_FOREIGN_ROSTER_DAILY_FA_REPAIR_PREVIOUS_MIN_WALL_MS = 120000u
};

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

typedef struct KboForeignRosterDailyState {
    uint32_t last_date_critical_date;
    uint32_t last_audit_date;
    uint32_t last_fa_repair_current_season;
    uint32_t last_fa_repair_previous_season;
    DWORD last_fa_repair_current_tick;
    DWORD last_fa_repair_previous_tick;
    char last_audit_save_path[MAX_PATH];
} KboForeignRosterDailyState;

static KboForeignRosterDailyState g_kbo_foreign_roster_daily_sync_state;
static KboForeignRosterDailyState g_kbo_foreign_roster_daily_background_state;

static int kbo_foreign_roster_daily_refresh_save_scope(
    KboForeignRosterDailyState* state,
    const char* source)
{
    if (state == NULL) {
        return 1;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }
    if (state->last_audit_save_path[0] == '\0'
            || strcmp(state->last_audit_save_path, save_path) != 0) {
        snprintf(state->last_audit_save_path, sizeof(state->last_audit_save_path), "%s", save_path);
        state->last_audit_date = kbo_foreign_roster_daily_load_last_audit_date(
            source != NULL ? source : "foreign_roster_daily_save_scope");
        state->last_date_critical_date = 0u;
        state->last_fa_repair_current_season = 0u;
        state->last_fa_repair_previous_season = 0u;
        state->last_fa_repair_current_tick = 0u;
        state->last_fa_repair_previous_tick = 0u;
    }
    return 1;
}

static int kbo_foreign_roster_daily_process_date_critical(
    KboForeignRosterDailyState* state,
    uint32_t today,
    const char* source)
{
    if (state == NULL || today == 0u) {
        return 1;
    }
    if (!kbo_foreign_roster_daily_refresh_save_scope(
            state,
            source != NULL ? source : "foreign_roster_daily_sync")) {
        return 0;
    }
    if (state->last_date_critical_date != 0u && today <= state->last_date_critical_date) {
        return 1;
    }
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "foreign_roster_daily_sync")) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_foreign_roster_daily_tick);

    if (kbo_foreign_roster_daily_abort_if_save("before_independent_acquisition", today)) {
        KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.sync.date_critical.save_abort.before_independent_acquisition");
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_foreign_roster_daily_independent_acquisition);
    kbo_run_independent_team_acquisition_ai_for_date(
        today,
        source != NULL ? source : "foreign_roster_daily_sync");
    KBO_PROFILE_END(profile_foreign_roster_daily_independent_acquisition, "foreign_roster.daily.sync.independent_acquisition");

    if (kbo_foreign_roster_daily_abort_if_save("before_rights_sync", today)) {
        KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.sync.date_critical.save_abort.before_rights_sync");
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_foreign_roster_daily_rights_sync);
    kbo_sync_active_foreign_waiver_rights_to_memory(
        source != NULL ? source : "foreign_roster_daily_sync",
        today);
    KBO_PROFILE_END(profile_foreign_roster_daily_rights_sync, "foreign_roster.daily.sync.rights_sync");

    state->last_date_critical_date = today;
    KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.sync.date_critical");
    return !kbo_runtime_save_in_progress();
}

static int kbo_foreign_roster_daily_process_background_date(
    KboForeignRosterDailyState* state,
    uint32_t today,
    const char* source)
{
    if (state == NULL || today == 0u) {
        return 1;
    }
    if (!kbo_foreign_roster_daily_refresh_save_scope(
            state,
            source != NULL ? source : "foreign_roster_daily_background")) {
        return 0;
    }
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "foreign_roster_daily_background")) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_foreign_roster_daily_tick);

    if (kbo_foreign_roster_daily_abort_if_save("before_dirty_ai_callup", today)) {
        KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.background.save_abort.before_dirty_ai_callup");
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_foreign_roster_daily_ai_callup);
    kbo_consume_foreign_ai_roster_daily_callup_dirty(
        source != NULL ? source : "foreign_roster_daily_background");
    KBO_PROFILE_END(profile_foreign_roster_daily_ai_callup, "foreign_roster.daily.background.dirty_ai_roster_callup");

    if (state->last_audit_date != 0u && today <= state->last_audit_date) {
        KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.background.cached");
        return 1;
    }

    if (kbo_foreign_roster_daily_abort_if_save("after_ai_roster_callup", today)) {
        KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.background.save_abort.after_ai_roster_callup");
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_foreign_roster_daily_retention);
    kbo_foreign_retention_guard_repair(source != NULL ? source : "foreign_roster_daily_background");
    KBO_PROFILE_END(profile_foreign_roster_daily_retention, "foreign_roster.daily.background.retention_guard");

    uint32_t season = today / 10000u;
    DWORD now = GetTickCount();
    int run_current_fa_repair = season != state->last_fa_repair_current_season
        || state->last_fa_repair_current_tick == 0u
        || now - state->last_fa_repair_current_tick >= KBO_FOREIGN_ROSTER_DAILY_FA_REPAIR_CURRENT_MIN_WALL_MS;
    if (run_current_fa_repair) {
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_fa_repair_current);
        kbo_fa_declaration_repair_retained_contracts_for_season(
            season,
            source != NULL ? source : "foreign_roster_daily_background");
        KBO_PROFILE_END(profile_foreign_roster_daily_fa_repair_current, "foreign_roster.daily.background.fa_repair_current");
        state->last_fa_repair_current_season = season;
        state->last_fa_repair_current_tick = now;
    }

    if (season > KBO_SEASON_YEAR_MIN) {
        uint32_t previous_season = season - 1u;
        int run_previous_fa_repair = previous_season != state->last_fa_repair_previous_season
            || state->last_fa_repair_previous_tick == 0u
            || now - state->last_fa_repair_previous_tick >= KBO_FOREIGN_ROSTER_DAILY_FA_REPAIR_PREVIOUS_MIN_WALL_MS;
        if (run_previous_fa_repair) {
            KBO_PROFILE_BEGIN(profile_foreign_roster_daily_fa_repair_previous);
            kbo_fa_declaration_repair_retained_contracts_for_season(
                previous_season,
                "foreign_roster_daily_background_previous_season");
            KBO_PROFILE_END(profile_foreign_roster_daily_fa_repair_previous, "foreign_roster.daily.background.fa_repair_previous");
            state->last_fa_repair_previous_season = previous_season;
            state->last_fa_repair_previous_tick = now;
        }
    }

    if (kbo_foreign_roster_daily_abort_if_save("before_audit", today)) {
        KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.background.save_abort.before_audit");
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_foreign_roster_daily_audit);
    audit_foreign_roster_state(source != NULL ? source : "foreign_roster_daily_background", 1);
    KBO_PROFILE_END(profile_foreign_roster_daily_audit, "foreign_roster.daily.background.audit");

    state->last_audit_date = today;
    kbo_foreign_roster_daily_persist_last_audit_date(
        today,
        source != NULL ? source : "foreign_roster_daily_background");
    KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.background.audit_repair");
    return 1;
}

static int kbo_foreign_roster_daily_sync_consumer(
    uint32_t date,
    uint32_t site_rva,
    void* context)
{
    (void)context;
    const char* source = site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA
        ? "foreign_roster_daily_sync_save_enter"
        : "foreign_roster_daily_sync_post_advance";
    if (site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA) {
        kbo_mark_foreign_ai_roster_daily_callup_dirty("foreign_roster_daily_save_enter");
    }
    return kbo_foreign_roster_daily_process_date_critical(
        &g_kbo_foreign_roster_daily_sync_state,
        date,
        source);
}

DWORD WINAPI kbo_foreign_roster_daily_audit_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("foreign roster daily audit thread started");

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

        KboCurrentDateTickWork latest_work = {0};
        int saw_work = 0;
        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
            latest_work = work;
            saw_work = 1;
            kbo_current_date_tick_consumer_mark_processed(&consumer);
        }
        if (saw_work) {
            kbo_foreign_roster_daily_process_background_date(
                &g_kbo_foreign_roster_daily_background_state,
                latest_work.date,
                "foreign_roster_daily_background_latest");
        } else {
            uint32_t latest_date = 0u;
            if (kbo_current_date_tick_latest_published_date(&latest_date)) {
                kbo_foreign_roster_daily_process_background_date(
                    &g_kbo_foreign_roster_daily_background_state,
                    latest_date,
                    "foreign_roster_daily_background_idle");
            }
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
    kbo_current_date_tick_register_sync_consumer(
        "foreign_roster_daily_audit",
        kbo_foreign_roster_daily_sync_consumer,
        NULL);
    kbo_mark_foreign_ai_roster_daily_callup_dirty("foreign_roster_daily_thread_start");

    if (!kbo_start_runtime_thread(kbo_foreign_roster_daily_audit_thread, NULL, "foreign roster daily audit")) {
        InterlockedExchange(&g_kbo_foreign_roster_daily_audit_started, 0);
    }
}
