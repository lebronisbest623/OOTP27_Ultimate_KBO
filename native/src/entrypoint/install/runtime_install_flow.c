#include "../entrypoint_internal.h"
#include "../../core/season/opening_day_storyline_guard.h"
#include "../../patch_installers/amateur_assignment/patch_installers_amateur_assignment.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

static volatile LONG g_kbo_early_foreign_policy_hooks_install_started = 0;

void install_kbo_early_foreign_policy_hooks_once(const char* source)
{
    if (!kbo_custom_foreign_policy_enabled()) {
        kbo_log_runtimef(
            "KBO early foreign policy hooks skipped source=%s reason=custom_policy_disabled",
            source != NULL ? source : "");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_early_foreign_policy_hooks_install_started, 1, 0) != 0) {
        kbo_log_runtimef(
            "KBO early foreign policy hooks skipped source=%s reason=already_started",
            source != NULL ? source : "");
        return;
    }

    kbo_log_runtimef("KBO early foreign policy hooks installing source=%s", source != NULL ? source : "");

    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_PLAYER_TEAM_SIGNABILITY_PATCH_FILE)) {
        install_kbo_player_team_signability_patch();
    } else {
        kbo_log_runtime_line("KBO early player/team signability patch skipped: enable_kbo_player_team_signability_patch is false");
    }

    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_OFFER_ELIGIBILITY_PATCH_FILE)) {
        install_kbo_player_offer_eligibility_patch();
    } else {
        kbo_log_runtime_line("KBO early player offer eligibility patch skipped: enable_kbo_offer_eligibility_patch is false");
    }

    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_SUBMIT_OFFER_PROBE_PATCH_FILE)) {
        install_kbo_fa_submit_offer_probe_patch();
    } else {
        kbo_log_runtime_line("KBO early submit-offer probe patch skipped: disable_kbo_submit_offer_probe_patch is true");
    }

    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FOREIGN_SIGNING_BRANCH_PATCH_FILE)) {
        install_kbo_fa_signing_branch_patch();
    } else {
        kbo_log_runtime_line("KBO early foreign signing branch patch skipped: disable_kbo_foreign_signing_branch_patch is true");
    }

    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_TRADE_CHECK_PATCH_FILE)
            && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FOREIGN_TRADE_CHECK_PATCH_FILE)) {
        install_kbo_trade_check_foreign_policy_patch();
    } else {
        kbo_log_runtime_line("KBO early trade foreign policy patch skipped: flag disabled");
    }

    kbo_log_runtimef("KBO early foreign policy hooks installed source=%s", source != NULL ? source : "");
}

void install_kbo_full_runtime_after_roster_marker(HINSTANCE instance)
{
    if (InterlockedCompareExchange(&g_kbo_full_runtime_install_started, 1, 0) != 0) {
        kbo_log_runtime_line("KBO full runtime install skipped: already started");
        return;
    }
    InterlockedExchange(&g_kbo_runtime_date_stable_ready, 1);

    start_kbo_hotkey_window_thread(instance);
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
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_SINGLE_DIVISION_ALLSTAR_RUNTIME_PATCHES_FILE)
            && read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_SINGLE_DIVISION_ALLSTAR_EVENTS_FILE)) {
        kbo_log_runtime_line("KBO all-star flag repair enabled after roster marker");
        start_kbo_allstar_force_retry_thread();
    } else {
        kbo_log_runtime_line("KBO all-star flag repair skipped: single-division all-star runtime/events flag is false");
    }
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_AWARD_SCHEDULE_CREATE_EVENT_HOOK_FILE)) {
        install_kbo_award_schedule_create_event_patch();
    } else {
        kbo_log_runtime_line("KBO award schedule create-event hook disabled: disable_kbo_award_schedule_create_event_hook is true");
    }

    install_kbo_military_service_entry_patch();
    install_kbo_military_status_update_patch();
    kbo_load_military_service_team_policy_override_once();
    int enable_sangmu_fa_block_default = !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_SANGMU_FA_BLOCK_CORE_FILE);
    int enable_sangmu_signability_only = enable_sangmu_fa_block_default
        ||
        read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_SANGMU_SIGNABILITY_ONLY_FILE);
    int enable_sangmu_offer_only = enable_sangmu_fa_block_default
        || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_SANGMU_OFFER_ONLY_FILE);
    int enable_sangmu_fa_block_core = enable_sangmu_signability_only || enable_sangmu_offer_only;
    int enable_fa_compensation_core = !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FA_COMPENSATION_FILE);
    int enable_amateur_assignment_core =
        !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_AMATEUR_ASSIGNMENT_REROUTE_FILE);
    if (enable_sangmu_fa_block_core) {
        kbo_log_runtimef(
            "KBO Sangmu FA block enabled: signability=%d offer=%d",
            enable_sangmu_signability_only,
            enable_sangmu_offer_only);
    } else {
        kbo_log_runtime_line("KBO Sangmu FA block diagnostic disabled: kbo_flags.json enable_kbo_sangmu_signability_only / enable_kbo_sangmu_offer_only are false");
    }
    if (enable_sangmu_fa_block_core || enable_fa_compensation_core || enable_amateur_assignment_core) {
        if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_MILITARY_TEAM_ADD_GUARD_PATCH_FILE)) {
            install_kbo_military_team_add_guard_patch();
        } else {
            kbo_log_runtime_line("KBO military team-add guard patch disabled: kbo_flags.json disable_kbo_military_team_add_guard_patch is true");
        }
    }
    if (enable_amateur_assignment_core) {
        install_kbo_amateur_assignment_batch_probe_patch();
    }
    start_kbo_delayed_sangmu_fa_hooks_install_thread(
        enable_sangmu_signability_only || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_PLAYER_TEAM_SIGNABILITY_PATCH_FILE),
        enable_sangmu_offer_only || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_OFFER_ELIGIBILITY_PATCH_FILE));
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_AI_FA_FALLBACK_PATCH_FILE)) {
        install_kbo_ai_fa_signability_random_fallback_patch();
    } else {
        kbo_log_runtime_line("KBO AI FA signability fallback patch disabled: kbo_flags.json enable_kbo_ai_fa_fallback_patch is false");
    }
    int enable_submit_offer_probe = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_SUBMIT_OFFER_PROBE_PATCH_FILE)
        || (kbo_custom_foreign_policy_enabled()
            && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_SUBMIT_OFFER_PROBE_PATCH_FILE));
    if (enable_submit_offer_probe) {
        install_kbo_fa_submit_offer_probe_patch();
    } else {
        kbo_log_runtime_line("KBO FA submit-offer probe patch disabled: kbo_flags.json disable_kbo_submit_offer_probe_patch is true");
    }
    int enable_fa_signing_branch = enable_sangmu_fa_block_core
        || enable_fa_compensation_core
        || (kbo_custom_foreign_policy_enabled()
            && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FOREIGN_SIGNING_BRANCH_PATCH_FILE));
    if (enable_fa_signing_branch) {
        install_kbo_fa_signing_branch_patch();
    } else {
        kbo_log_runtime_line("KBO FA signing branch hook disabled: Sangmu FA block is disabled, FA compensation is disabled, and custom foreign policy hook is disabled");
    }
    if (kbo_custom_foreign_policy_enabled()
            && read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_TRADE_CHECK_PATCH_FILE)
            && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FOREIGN_TRADE_CHECK_PATCH_FILE)) {
        install_kbo_trade_check_foreign_policy_patch();
    } else {
        kbo_log_runtime_line("KBO trade foreign policy patch disabled: custom foreign policy is disabled, enable_kbo_foreign_trade_check_patch is false, or disable_kbo_foreign_trade_check_patch is true");
    }
    int explicit_enable_ai_fa_status_candidate_insert_hook =
        read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_AI_FA_STATUS_CANDIDATE_INSERT_HOOK_FILE);
    int disable_ai_fa_status_candidate_insert_hook =
        read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_AI_FA_STATUS_CANDIDATE_INSERT_HOOK_FILE);
    int auto_enable_ai_fa_status_candidate_insert_hook_for_foreign_ai =
        read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_MANAGEMENT_FILE);
    int auto_enable_ai_fa_status_candidate_insert_hook_for_foreign_controller =
        kbo_foreign_ai_controller_enabled();
    int auto_enable_ai_fa_status_candidate_insert_hook =
        kbo_foreign_injury_replacement_enabled()
        || auto_enable_ai_fa_status_candidate_insert_hook_for_foreign_ai
        || auto_enable_ai_fa_status_candidate_insert_hook_for_foreign_controller;
    int enable_ai_fa_status_candidate_insert_hook =
        !disable_ai_fa_status_candidate_insert_hook
        && (explicit_enable_ai_fa_status_candidate_insert_hook
            || auto_enable_ai_fa_status_candidate_insert_hook);
    kbo_log_runtimef(
        "KBO FA market diagnostic flags: enable_ai_fa_status_candidate_insert_hook=%d explicit=%d auto_foreign_injury=%d auto_foreign_ai=%d auto_foreign_controller=%d disable=%d",
        enable_ai_fa_status_candidate_insert_hook,
        explicit_enable_ai_fa_status_candidate_insert_hook,
        kbo_foreign_injury_replacement_enabled(),
        auto_enable_ai_fa_status_candidate_insert_hook_for_foreign_ai,
        auto_enable_ai_fa_status_candidate_insert_hook_for_foreign_controller,
        disable_ai_fa_status_candidate_insert_hook);
    if (enable_ai_fa_status_candidate_insert_hook) {
        install_kbo_ai_fa_status_candidate_insert_patch();
    } else {
        kbo_log_runtime_line("KBO AI FA status candidate insert hook disabled: no explicit enable, no foreign injury/foreign AI/controller auto-enable, or disable flag is set");
    }
    int explicit_enable_foreign_ai_offer_candidate_priority_hook =
        read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_CANDIDATE_PRIORITY_HOOK_FILE);
    int disable_foreign_ai_offer_candidate_priority_hook =
        read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FOREIGN_AI_OFFER_CANDIDATE_PRIORITY_HOOK_FILE);
    int auto_enable_foreign_ai_offer_candidate_priority_hook_for_foreign_ai =
        read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_MANAGEMENT_FILE);
    int auto_enable_foreign_ai_offer_candidate_priority_hook_for_foreign_controller =
        kbo_foreign_ai_controller_enabled();
    int enable_foreign_ai_offer_candidate_priority_hook =
        !disable_foreign_ai_offer_candidate_priority_hook
        && (explicit_enable_foreign_ai_offer_candidate_priority_hook
            || auto_enable_foreign_ai_offer_candidate_priority_hook_for_foreign_ai
            || auto_enable_foreign_ai_offer_candidate_priority_hook_for_foreign_controller);
    kbo_log_runtimef(
        "KBO foreign AI offer candidate priority flags: enable=%d explicit=%d auto_foreign_ai=%d auto_foreign_controller=%d disable=%d",
        enable_foreign_ai_offer_candidate_priority_hook,
        explicit_enable_foreign_ai_offer_candidate_priority_hook,
        auto_enable_foreign_ai_offer_candidate_priority_hook_for_foreign_ai,
        auto_enable_foreign_ai_offer_candidate_priority_hook_for_foreign_controller,
        disable_foreign_ai_offer_candidate_priority_hook);
    if (enable_foreign_ai_offer_candidate_priority_hook) {
        install_kbo_foreign_ai_offer_candidate_priority_patch();
    } else {
        kbo_log_runtime_line("KBO foreign AI offer candidate priority hook disabled: no explicit/foreign-AI/controller auto-enable or disable flag is set");
    }
    if (kbo_foreign_ai_offer_attach_hook_required(
            auto_enable_foreign_ai_offer_candidate_priority_hook_for_foreign_ai,
            auto_enable_foreign_ai_offer_candidate_priority_hook_for_foreign_controller,
            read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_RESEARCH_HOOKS_FILE),
            read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_ATTACH_PROBE_FILE))) {
        kbo_log_runtime_line("KBO foreign AI offer attach hook enabled");
        install_kbo_foreign_ai_offer_attach_probe_patch();
    } else {
        kbo_log_runtime_line("KBO foreign AI offer attach hook disabled: no foreign AI/controller/research/probe flag is enabled");
    }
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_SEASON_PHASE_CAPTURE_HOOKS_FILE)) {
        install_kbo_season_phase_capture_hooks();
    } else {
        kbo_log_runtime_line("KBO season phase capture hooks disabled: disable_kbo_season_phase_capture_hooks is true");
    }
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
    start_kbo_foreign_injury_date_tick_thread();
    if (kbo_no_minor_contract_patch_enabled()) {
        if (!kbo_opening_day_storyline_guard_active("no_minor_contract_patch_install", NULL, NULL)) {
            install_kbo_no_minor_contract_patch_once("runtime_install");
        } else {
            kbo_log_runtime_line("KBO no-minor-contract patch deferred during opening-day stock-news guard");
            start_kbo_delayed_no_minor_contract_patch_install_thread();
        }
    }
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_SALARY_ARBITRATION_NO_WITHDRAW_PATCH_FILE)) {
        install_kbo_salary_arbitration_no_withdraw_patch();
    } else {
        kbo_log_runtime_line("KBO salary arbitration no-withdraw patch disabled: kbo_flags.json disable_kbo_salary_arbitration_no_withdraw_patch is true");
    }
    install_kbo_intl_established_fa_multiplier_patch();
    install_kbo_intl_established_fa_generation_filter_patch();
    start_kbo_intl_established_fa_postscan_thread();
    kbo_log_runtime_line("KBO international established FA player probe patch retired: using postscan diagnostics after OOTP registration");
    install_kbo_foreign_count_patches();
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_CALLUP_FOREIGN_LIMIT_PATCH_FILE)) {
        install_kbo_callup_foreign_limit_branch_patches();
    } else {
        kbo_log_runtime_line("KBO callup foreign limit branch patches disabled: custom foreign policy is org-level");
    }
    start_kbo_foreign_waiver_scanner_thread();
    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FA_SALARY_OPENING_DAY_SNAPSHOT_FILE)) {
        kbo_log_runtime_line("KBO FA salary opening-day phase hook retired: using league-memory/news snapshot thread only");
        start_kbo_fa_salary_snapshot_thread();
    } else {
        kbo_log_runtime_line("KBO FA salary opening-day snapshot thread disabled: kbo_flags.json disable_kbo_fa_salary_opening_day_snapshot is true");
    }
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_SEASON_PHASE_MONITOR_FILE)) {
        start_kbo_season_phase_monitor();
        kbo_log_runtime_line("KBO season phase read-only monitor enabled; write capture hooks are managed separately");
    } else {
        kbo_log_runtime_line("KBO season phase read-only monitor disabled: kbo_flags.json enable_kbo_season_phase_monitor is false");
    }
    start_kbo_award_schedule_probe_thread();
    start_kbo_military_seed_bootstrap_thread();
    start_kbo_military_days_tick_thread();
    start_kbo_cbt_event_scheduler_thread();
    start_kbo_custom_event_monitor();
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_CBT_SERVICE_TIME_PROBE_FILE)) {
        kbo_cbt_service_time_probe_once();
    } else {
        kbo_log_runtime_line("KBO CBT service-time memory probe disabled: kbo_flags.json enable_kbo_cbt_service_time_probe is false");
    }
    kbo_log_runtime_line("KBO full runtime install complete after roster marker");
}

