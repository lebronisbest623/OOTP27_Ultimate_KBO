#include "../entrypoint_internal.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"
#include "../../core/logging/event/log_event.h"

void start_kbo_full_runtime_marker_wait_thread(HINSTANCE instance)
{
    if (InterlockedCompareExchange(&g_kbo_full_runtime_marker_wait_started, 1, 0) != 0) {
        kbo_log_runtime_line("KBO full runtime marker guard thread already started");
        return;
    }

    if (!kbo_start_runtime_thread(kbo_full_runtime_marker_wait_thread, instance, "full runtime marker wait")) {
        InterlockedExchange(&g_kbo_full_runtime_marker_wait_started, 0);
    }
}

DWORD WINAPI patch_thread(LPVOID parameter)
{
    kbo_log_runtime_line("KBOFix loaded");
    kbo_log_runtime_line("KBOFix build includes scoped all-star single-division prep/roster/team setup gates");

    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_EXPERIMENTAL_RUNTIME_HOOKS_FILE)) {
        kbo_log_runtime_line("KBOFix: legacy enable_experimental_runtime_hooks=false ignored; use enable_kbo_diagnostic_minimal_runtime for no-patch diagnostics");
    }

    int diagnostic_minimal_runtime = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_DIAGNOSTIC_MINIMAL_RUNTIME_FILE);
    if (diagnostic_minimal_runtime) {
        kbo_log_runtime_line("KBO diagnostic minimal runtime enabled: F2 hub and runtime patches disabled");
    }

    if (!verify_ootp_build()) {
        kbo_log_runtime_line("KBOFix: build verification failed; no patches installed");
        return 0;
    }

    if (diagnostic_minimal_runtime) {
        kbo_log_runtime_line("KBO diagnostic minimal runtime: build verified, no runtime patches installed");
        return 0;
    }

    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_AWARD_SCHEDULE_CREATE_EVENT_HOOK_FILE)) {
        install_kbo_award_schedule_create_event_patch();
    } else {
        kbo_log_runtime_line("KBO award schedule create-event hook disabled: disable_kbo_award_schedule_create_event_hook is true");
    }

    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_PLAYER_HOVER_MANAGER_PROBE_FILE)) {
        install_kbo_player_hover_manager_probe_patch();
    } else {
        kbo_log_runtime_line("KBO player hover manager probe disabled: disable_kbo_player_hover_manager_probe is true");
    }
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_PLAYER_TOOLTIP_TEXT_APPEND_PROBE_FILE)) {
        install_kbo_player_tooltip_text_append_probe_patch();
    } else {
        kbo_log_runtime_line("KBO player tooltip text append probe disabled: disable_kbo_player_tooltip_text_append_probe is true");
    }
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_PLAYER_TOOLTIP_STRING_FORMAT_PROBE_FILE)) {
        install_kbo_player_tooltip_string_format_probe_patch();
    } else {
        kbo_log_runtime_line("KBO player tooltip string format probe disabled: disable_kbo_player_tooltip_string_format_probe is true");
    }
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_PLAYER_TOOLTIP_RATING_COMMON_PROBE_FILE)) {
        install_kbo_player_tooltip_rating_common_probe_patch();
    } else {
        kbo_log_runtime_line("KBO player tooltip rating common probe disabled: disable_kbo_player_tooltip_rating_common_probe is true");
    }
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_PLAYER_TOOLTIP_RATING_PANEL_CTOR_PROBE_FILE)) {
        install_kbo_player_tooltip_rating_panel_ctor_probe_patch();
    } else {
        kbo_log_runtime_line("KBO player tooltip rating panel ctor probe disabled: disable_kbo_player_tooltip_rating_panel_ctor_probe is true");
    }
    install_kbo_early_foreign_policy_hooks_once("presave_bootstrap");
    int date_tick_hooks = 0;
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_CURRENT_DATE_TICK_CAPTURE_HOOK_FILE)) {
        date_tick_hooks = install_kbo_current_date_tick_capture_hook();
    } else {
        kbo_log_runtime_line("KBO current date tick capture hook disabled: disable_kbo_current_date_tick_capture_hook is true");
    }
    if (kbo_current_date_tick_watchpoint_enabled()) {
        kbo_log_runtimef(
            "KBO current date tick watchpoint primary requested capture_hooks=%d",
            date_tick_hooks);
        start_kbo_current_date_tick_watchpoint_thread();
    } else {
        kbo_log_runtime_line("KBO current date tick watchpoint disabled: disable_kbo_current_date_tick_watchpoint is true");
    }
    install_kbo_early_no_minor_contract_hooks_once("presave_bootstrap");
    int foreign_ai_roster_management =
        read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_MANAGEMENT_FILE);
    int foreign_ai_controller = kbo_foreign_ai_controller_enabled();
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_AI_FA_STATUS_CANDIDATE_INSERT_HOOK_FILE)
            && (foreign_ai_roster_management
                || foreign_ai_controller
                || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_AI_FA_STATUS_CANDIDATE_INSERT_HOOK_FILE))) {
        kbo_log_runtime_line("KBO presave foreign AI FA candidate hook install requested");
        install_kbo_ai_fa_status_candidate_insert_patch();
    }
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FOREIGN_AI_OFFER_CANDIDATE_PRIORITY_HOOK_FILE)
            && (foreign_ai_roster_management
                || foreign_ai_controller
                || kbo_offer_candidate_replacement_dispatcher_needs_hook()
                || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_CANDIDATE_PRIORITY_HOOK_FILE))) {
        kbo_log_runtime_line("KBO presave foreign AI offer candidate priority hook install requested");
        install_kbo_foreign_ai_offer_candidate_priority_patch();
    }
    if (kbo_foreign_ai_offer_attach_hook_required(
            foreign_ai_roster_management,
            foreign_ai_controller,
            read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_RESEARCH_HOOKS_FILE),
            read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_ATTACH_PROBE_FILE))) {
        kbo_log_runtime_line("KBO presave foreign AI offer attach hook install requested");
        install_kbo_foreign_ai_offer_attach_probe_patch();
    }
    if (foreign_ai_roster_management) {
        install_kbo_military_team_add_guard_patch();
        install_kbo_ai_roster_select_trace_patch();
        install_kbo_ai_roster_primary_apply_flow_trace_patch();
        install_kbo_ai_roster_apply_selection_trace_patch();
        start_kbo_foreign_daily_maintenance_thread();
    } else {
        kbo_log_runtime_line("KBO foreign AI roster management skipped: enable_foreign_ai_roster_management is false");
    }
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_AMATEUR_ASSIGNMENT_REROUTE_FILE)) {
        install_kbo_amateur_assignment_batch_probe_patch();
    } else {
        kbo_log_runtime_line("KBO early amateur assignment batch probe skipped: disable_amateur_assignment_reroute is true");
    }
    start_kbo_military_seed_bootstrap_thread();

    kbo_log_runtime_line("KBO F2 hub starting before runtime marker guard");
    start_kbo_hotkey_window_thread((HINSTANCE)parameter);
    kbo_log_runtime_line("KBO CBT/custom event startup deferred until full runtime marker");
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_CBT_DRAFT_ORDER_PENALTY_HOOK_FILE)) {
        kbo_log_runtime_line("KBO CBT draft order penalty hook install requested");
        install_kbo_cbt_draft_order_penalty_patch();
    } else {
        kbo_log_runtime_line("KBO CBT draft order penalty hook skipped: disable_kbo_cbt_draft_order_penalty_hook is true");
    }
    start_kbo_fa_salary_snapshot_thread();
    start_kbo_domestic_fa_market_investigation_thread();
    start_kbo_captain_preseason_selection_thread();

    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_SINGLE_DIVISION_ALLSTAR_RUNTIME_PATCHES_FILE)) {
        kbo_log_runtime_line("KBO all-star presave bootstrap install started");
        install_single_division_allstar_patch();
        install_allstar_team_setup_single_division_patch();
        install_allstar_candidate_team_split_patch();
        install_allstar_candidate_player_push_filter_patch();
        install_allstar_candidate_team_roster_push_filter_patch();
        install_allstar_candidate_ranked_player_push_filter_patch();
        if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_SINGLE_DIVISION_ALLSTAR_VOTING_HOOK_FILE)) {
            install_allstar_voting_begin_prepare_patch();
        } else {
            kbo_log_runtime_line("KBO all-star voting begin prepare hook disabled: kbo_flags.json enable_single_division_allstar_voting_hook is false");
        }
        if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_SINGLE_DIVISION_ALLSTAR_EVENTS_FILE)) {
            install_allstar_events_prepare_patch();
        } else {
            kbo_log_runtime_line("KBO all-star events prepare hook disabled: kbo_flags.json enable_single_division_allstar_events is false");
        }
        if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_SINGLE_DIVISION_ALLSTAR_SETTINGS_PATCH_FILE)) {
            install_allstar_settings_ui_patch();
        } else {
            kbo_log_runtime_line("KBO all-star settings UI patch disabled: kbo_flags.json enable_single_division_allstar_settings_patch is false");
        }
        kbo_log_runtime_line("KBO all-star presave bootstrap hooks installed");

        load_allstar_team_rules_once();
        kbo_log_runtime_line("KBO all-star presave direct league mutations deferred until OOTP invokes scoped hooks");
    } else {
        kbo_log_runtime_line("KBO single-division all-star runtime patches disabled: kbo_flags.json enable_single_division_allstar_runtime_patches is false");
        if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_SINGLE_DIVISION_ALLSTAR_SETTINGS_PATCH_FILE)) {
            install_allstar_settings_ui_patch();
        } else {
            kbo_log_runtime_line("KBO all-star settings UI patch disabled: kbo_flags.json enable_single_division_allstar_settings_patch is false");
        }
    }

    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_RUNTIME_ROSTER_MARKER_GUARD_FILE)) {
        kbo_log_runtime_line("KBO runtime marker guard disabled by flag");
        install_kbo_full_runtime_after_roster_marker((HINSTANCE)parameter);
        return 0;
    }

    start_kbo_full_runtime_marker_wait_thread((HINSTANCE)parameter);
    return 0;
}

static DWORD WINAPI kbo_hot_reinject_ai_roster_management_thread(LPVOID parameter)
{
    (void)parameter;

    kbo_log_runtime_line("KBO hot reinject runtime refresh requested");
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_EXPERIMENTAL_RUNTIME_HOOKS_FILE)) {
        kbo_log_runtime_line("KBO hot reinject: legacy enable_experimental_runtime_hooks=false ignored; continuing runtime refresh");
    }
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_DIAGNOSTIC_MINIMAL_RUNTIME_FILE)) {
        kbo_log_runtime_line("KBO hot reinject runtime refresh skipped: diagnostic minimal runtime is enabled");
        return 0;
    }

    if (!verify_ootp_build()) {
        kbo_log_runtime_line("KBO hot reinject runtime refresh skipped: build verification failed");
        return 0;
    }

    InterlockedExchange(&g_kbo_runtime_date_stable_ready, 1);
    kbo_log_runtime_line("KBO hot reinject runtime date stable ready set");

    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_AWARD_SCHEDULE_CREATE_EVENT_HOOK_FILE)) {
        install_kbo_award_schedule_create_event_patch();
    } else {
        kbo_log_runtime_line("KBO hot reinject award schedule create-event hook disabled: disable_kbo_award_schedule_create_event_hook is true");
    }

    if (kbo_current_date_tick_watchpoint_enabled()) {
        kbo_log_runtime_line("KBO hot reinject current date tick watchpoint requested");
        start_kbo_current_date_tick_watchpoint_thread();
    } else {
        kbo_log_runtime_line("KBO hot reinject current date tick watchpoint disabled: disable_kbo_current_date_tick_watchpoint is true");
    }

    kbo_log_runtime_line("KBO hot reinject current-date consumers requested");
    start_kbo_foreign_injury_date_tick_thread();
    start_kbo_foreign_waiver_scanner_thread();
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FA_SALARY_OPENING_DAY_SNAPSHOT_FILE)) {
        start_kbo_fa_salary_snapshot_thread();
    }
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_SEASON_PHASE_MONITOR_FILE)) {
        start_kbo_season_phase_monitor();
    }
    start_kbo_award_schedule_probe_thread();
    start_kbo_amateur_reputation_update_thread();
    start_kbo_military_seed_bootstrap_thread();
    start_kbo_military_days_tick_thread();
    start_kbo_cbt_event_scheduler_thread();
    start_kbo_custom_event_monitor();
    start_kbo_domestic_fa_market_investigation_thread();
    start_kbo_captain_preseason_selection_thread();

    int foreign_ai_roster_management = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_MANAGEMENT_FILE);
    int foreign_ai_controller = kbo_foreign_ai_controller_enabled();
    int hot_roster_flow_trace = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_HOT_REINJECT_ROSTER_FLOW_TRACE_FILE);
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_AI_FA_STATUS_CANDIDATE_INSERT_HOOK_FILE)
            && (foreign_ai_roster_management
                || foreign_ai_controller
                || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_AI_FA_STATUS_CANDIDATE_INSERT_HOOK_FILE))) {
        install_kbo_ai_fa_status_candidate_insert_patch();
    }
    if ((foreign_ai_roster_management
            || foreign_ai_controller
            || kbo_offer_candidate_replacement_dispatcher_needs_hook()
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_CANDIDATE_PRIORITY_HOOK_FILE))
            && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FOREIGN_AI_OFFER_CANDIDATE_PRIORITY_HOOK_FILE)) {
        install_kbo_foreign_ai_offer_candidate_priority_patch();
    }
    if (kbo_foreign_ai_offer_attach_hook_required(
            foreign_ai_roster_management,
            foreign_ai_controller,
            read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_RESEARCH_HOOKS_FILE),
            read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_ATTACH_PROBE_FILE))) {
        install_kbo_foreign_ai_offer_attach_probe_patch();
    }
    if (foreign_ai_roster_management || hot_roster_flow_trace) {
        install_kbo_military_team_add_guard_patch();
        install_kbo_ai_roster_select_trace_patch();
        install_kbo_ai_roster_primary_apply_flow_trace_patch();
        install_kbo_ai_roster_apply_selection_trace_patch();
        start_kbo_foreign_daily_maintenance_thread();
    }
    kbo_log_runtime_line("KBO hot reinject runtime refresh finished");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);

        char mutex_name[96] = {0};
        snprintf(
            mutex_name,
            sizeof(mutex_name),
            "Local\\OOTP_KBO_FIX_%lu",
            (unsigned long)GetCurrentProcessId());
        g_kbo_process_instance_mutex = CreateMutexA(NULL, TRUE, mutex_name);
        if (g_kbo_process_instance_mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(g_kbo_process_instance_mutex);
            g_kbo_process_instance_mutex = NULL;
            kbo_log_runtime_line("KBO duplicate DLL attach detected; hot reinject runtime refresh requested");
            kbo_start_runtime_thread(
                kbo_hot_reinject_ai_roster_management_thread,
                instance,
                "hot reinject runtime refresh");
            return TRUE;
        }

        kbo_start_runtime_thread(patch_thread, instance, "patch install");
    } else if (reason == DLL_PROCESS_DETACH) {
        if (reserved == NULL) {
            kbo_shutdown_runtime_threads(10000u);
            kbo_log_event_shutdown();
        } else {
            kbo_request_runtime_threads_stop();
        }
        if (g_kbo_process_instance_mutex != NULL) {
            CloseHandle(g_kbo_process_instance_mutex);
            g_kbo_process_instance_mutex = NULL;
        }
    }

    return TRUE;
}

