#ifndef NATIVE_SRC_ENTRYPOINT_ENTRYPOINT_C_INTERNAL_H
#define NATIVE_SRC_ENTRYPOINT_ENTRYPOINT_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include "entrypoint.h"
#include "../amateur_player_quality/api/amateur_player_quality.h"
#include "../allstar/flags/allstar_flags.h"
#include "../allstar/allstar_league_context/allstar_league_context.h"
#include "../allstar/team_patch/allstar_team_patch.h"
#include "../build_verify/build_verify.h"
#include "../captain/api/captain_selection.h"
#include "../core/core_flags/api/flags_api.h"
#include "../core/dates/tick/current_date_tick_capture.h"
#include "../core/dates/tick/current_date_tick_watchpoint.h"
#include "../core/runtime_tuning/runtime_tuning_policy.h"
#include "../core/logging/core_log.h"
#include "../core/files/save_paths/core_save_paths.h"
#include "../core/product/ootp_product.h"
#include "../custom_events/asian_games/player_eval/asian_games_player_eval.h"
#include "../custom_events/diagnostics/cbt_service_time/cbt_service_time_probe.h"
#include "../custom_events/runtime/monitor/custom_event_monitor.h"
#include "../competitive_balance_tax/events/cbt_events.h"
#include "../fa_market_investigation/api/fa_market_investigation.h"
#include "../fa_salary_snapshot/threads/salary_snapshot_phase_events.h"
#include "../fa_salary_snapshot/threads/salary_snapshot_thread.h"
#include "../offer_candidate/replacement/offer_candidate_replacement_dispatcher.h"
#include "../foreign/waiver_core/api/foreign_waiver_core.h"
#include "../foreign/common/policy/foreign_waiver_policy.h"
#include "../foreign/controller/foreign_ai_controller.h"
#include "../foreign/injury/api/foreign_injury.h"
#include "../foreign/intl_established_fa_postscan/api/intl_established_fa_postscan.h"
#include "../foreign/daily_maintenance/foreign_daily_maintenance.h"
#include "../foreign/roster_audit/api/foreign_roster_audit.h"
#include "../foreign/signability/foreign_policy/wrappers/offer_attach/install_policy/foreign_ai_offer_attach_hook_policy.h"
#include "../foreign/signability/state/submit_offer_probe_state.h"
#include "../hotkey_window/hotkey_window.h"
#include "../military_service/military_service.h"
#include "../military_service/calendar/military_service_date.h"
#include "../military_service/players/team_policy/military_service_team_policy.h"
#include "../patch_installers/arbitration/no_withdraw/arbitration_no_withdraw_patch.h"
#include "../patch_installers/no_minor_contracts/core/api/no_minor_patch.h"
#include "../patch_installers/allstar/candidate/patch_installers_allstar_candidate.h"
#include "../patch_installers/allstar/events/patch_installers_allstar_events.h"
#include "../patch_installers/allstar/settings/patch_installers_allstar_settings.h"
#include "../patch_installers/allstar/static/patch_installers_allstar_static.h"
#include "../patch_installers/foreign/ai_fa/patch_installers_foreign_ai_fa_fallback.h"
#include "../patch_installers/foreign/ai_fa/patch_installers_foreign_ai_fa_status.h"
#include "../patch_installers/foreign/roster_limits/callup/patch_installers_foreign_callup_limits.h"
#include "../patch_installers/foreign/roster_limits/counts/patch_installers_foreign_counts.h"
#include "../patch_installers/foreign/signability/entry/patch_installers_foreign_signability_entry.h"
#include "../patch_installers/foreign/signability/signing/patch_installers_foreign_signing_branch.h"
#include "../patch_installers/foreign/signability/submit_offer/patch_installers_foreign_submit_offer_probe.h"
#include "../patch_installers/foreign/roster_limits/trade/patch_installers_foreign_trade_check.h"
#include "../patch_installers/foreign/intl_established_fa/patch_installers_intl_established_fa.h"
#include "../patch_installers/amateur_assignment/patch_installers_amateur_assignment.h"
#include "../patch_installers/awards/patch_installers_award_schedule.h"
#include "../patch_installers/competitive_balance_tax/patch_installers_cbt_draft_order.h"
#include "../patch_installers/current_date/patch_installers_current_date_tick.h"
#include "../patch_installers/export_text/import_extra_columns/roster_import_extra_columns.h"
#include "../patch_installers/export_text/roster_export_extra_columns.h"
#include "../patch_installers/hotkey_window/patch_installers_player_hover.h"
#include "../patch_installers/military/patch_installers_military.h"
#include "../patch_installers/season_phase/patch_installers_season_phase_probe.h"
#include "../awards/schedule/award_schedule_probe.h"
#include "../season_phase_monitor/season_phase_monitor.h"
#include "../service_time_guard/service_time_guard.h"
typedef struct KboSangmuFaHookInstallRequest {
    int enable_signability;
    int enable_offer;
} KboSangmuFaHookInstallRequest;

extern HANDLE g_kbo_process_instance_mutex;
extern volatile LONG g_kbo_sangmu_fa_hooks_install_started;
extern volatile LONG g_kbo_full_runtime_install_started;
extern volatile LONG g_kbo_full_runtime_marker_wait_started;
extern volatile LONG g_kbo_runtime_date_stable_ready;

DWORD WINAPI kbo_delayed_sangmu_fa_hooks_install_thread(LPVOID parameter);
void start_kbo_delayed_sangmu_fa_hooks_install_thread(
    int enable_signability,
    int enable_offer);
int kbo_current_save_has_required_roster_marker(const char* source, int log_detail);
void install_kbo_full_runtime_after_roster_marker(HINSTANCE instance);
DWORD WINAPI kbo_full_runtime_marker_wait_thread(LPVOID parameter);
void start_kbo_full_runtime_marker_wait_thread(HINSTANCE instance);
void install_kbo_early_foreign_policy_hooks_once(const char* source);
void install_kbo_early_no_minor_contract_hooks_once(const char* source);
int kbo_no_minor_contract_patch_enabled(void);
int install_kbo_no_minor_contract_patch_once(const char* source);
void start_kbo_delayed_no_minor_contract_patch_install_thread(void);
DWORD WINAPI patch_thread(LPVOID parameter);
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved);

#endif
