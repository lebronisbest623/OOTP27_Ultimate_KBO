#include "../internal/foreign_roster_audit_internal.h"
#include "../../../custom_events/runtime/monitor/custom_event_monitor.h"
#include "../../../team/add_player_guard/team_add_player_guard_ai_roster.h"
#include "../../../team/independent_acquisition/independent_acquisition_ai.h"
#include "../../injury/api/foreign_injury.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../retention_guard/foreign_retention_guard.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../../../fa_declaration/fa_declaration.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"

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

DWORD WINAPI kbo_foreign_roster_daily_audit_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("foreign roster daily audit thread started");

    uint32_t last_audit_date = 0u;
    uint32_t last_custom_event_scheduled_date = 0u;
    uint32_t last_custom_event_scanned_date = 0u;
    uint32_t last_custom_event_fa_comp_date = 0u;
    uint32_t last_fa_repair_current_season = 0u;
    uint32_t last_fa_repair_previous_season = 0u;
    DWORD last_fa_repair_current_tick = 0u;
    DWORD last_fa_repair_previous_tick = 0u;
    char last_audit_save_path[MAX_PATH] = {0};
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->foreign_roster_daily_audit_sleep_ms)) {
            break;
        }
        if (!kbo_runtime_pause_for_save_if_needed("foreign_roster_daily_audit")) {
            break;
        }

        uint32_t today = 0u;
        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_yyyymmdd(&today)
                || !kbo_get_current_save_path(save_path, sizeof(save_path))) {
            continue;
        }

        if (last_audit_save_path[0] == '\0' || strcmp(last_audit_save_path, save_path) != 0) {
            snprintf(last_audit_save_path, sizeof(last_audit_save_path), "%s", save_path);
            last_audit_date = 0u;
        }
        if (today == 0u || today == last_audit_date) {
            continue;
        }
        if (!kbo_runtime_pause_for_save_if_needed("foreign_roster_daily_date_change")) {
            break;
        }

        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_tick);
        if (kbo_foreign_roster_daily_abort_if_save("before_rights_sync", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.before_rights_sync");
            continue;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_rights_sync);
        kbo_sync_active_foreign_waiver_rights_to_memory(
            "foreign_roster_daily_date_change",
            today);
        KBO_PROFILE_END(profile_foreign_roster_daily_rights_sync, "foreign_roster.daily.rights_sync");
        if (kbo_foreign_roster_daily_abort_if_save("after_rights_sync", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_rights_sync");
            continue;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_custom_events);
        kbo_custom_event_monitor_tick(
            &last_custom_event_scheduled_date,
            &last_custom_event_scanned_date,
            &last_custom_event_fa_comp_date,
            "foreign_roster_daily_date_change");
        KBO_PROFILE_END(profile_foreign_roster_daily_custom_events, "foreign_roster.daily.custom_events");
        if (kbo_foreign_roster_daily_abort_if_save("after_custom_events", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_custom_events");
            continue;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_ai_callup);
        kbo_run_foreign_ai_roster_daily_callup("foreign_roster_daily_date_change");
        KBO_PROFILE_END(profile_foreign_roster_daily_ai_callup, "foreign_roster.daily.ai_roster_callup");
        if (kbo_foreign_roster_daily_abort_if_save("after_ai_roster_callup", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_ai_roster_callup");
            continue;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_independent_acquisition);
        kbo_run_independent_team_acquisition_ai("foreign_roster_daily_date_change");
        KBO_PROFILE_END(profile_foreign_roster_daily_independent_acquisition, "foreign_roster.daily.independent_acquisition");
        if (kbo_foreign_roster_daily_abort_if_save("after_independent_acquisition", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_independent_acquisition");
            continue;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_injury);
        uint32_t injury_scan_date = today;
        if (last_audit_date != 0u && last_audit_date < today) {
            uint32_t next_date = kbo_add_days_yyyymmdd(last_audit_date, 1u);
            if (next_date != 0u && next_date <= today) {
                injury_scan_date = next_date;
            }
        }
        while (injury_scan_date != 0u && injury_scan_date <= today) {
            if (injury_scan_date == today) {
                kbo_foreign_injury_replacement_scan_for_date(
                    "foreign_roster_daily_date_change",
                    injury_scan_date);
            } else {
                kbo_foreign_injury_replacement_scan_discovery_for_date(
                    "foreign_roster_daily_date_change",
                    injury_scan_date);
            }
            if (injury_scan_date == today) {
                break;
            }
            uint32_t next_date = kbo_add_days_yyyymmdd(injury_scan_date, 1u);
            if (next_date == 0u || next_date <= injury_scan_date) {
                break;
            }
            injury_scan_date = next_date;
        }
        KBO_PROFILE_END(profile_foreign_roster_daily_injury, "foreign_roster.daily.injury_replacement");
        if (kbo_foreign_roster_daily_abort_if_save("after_injury_replacement", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_injury_replacement");
            continue;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_retention);
        kbo_foreign_retention_guard_repair("foreign_roster_daily_date_change");
        KBO_PROFILE_END(profile_foreign_roster_daily_retention, "foreign_roster.daily.retention_guard");
        if (kbo_foreign_roster_daily_abort_if_save("after_retention_guard", today)) {
            KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.save_abort.after_retention_guard");
            continue;
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
            continue;
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
            continue;
        }
        KBO_PROFILE_BEGIN(profile_foreign_roster_daily_audit);
        audit_foreign_roster_state("foreign_roster_daily_date_change", 1);
        KBO_PROFILE_END(profile_foreign_roster_daily_audit, "foreign_roster.daily.audit");
        KBO_PROFILE_END(profile_foreign_roster_daily_tick, "foreign_roster.daily.tick");
        last_audit_date = today;
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
