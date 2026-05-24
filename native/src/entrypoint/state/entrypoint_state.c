#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include "../entrypoint.h"
#include "../../allstar/flags/allstar_flags.h"
#include "../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../allstar/team_patch/allstar_team_patch.h"
#include "../../build_verify/build_verify.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../custom_events/asian_games/player_eval/asian_games_player_eval.h"
#include "../../custom_events/runtime/monitor/custom_event_monitor.h"
#include "../../fa_salary_snapshot/threads/salary_snapshot_phase_events.h"
#include "../../fa_salary_snapshot/threads/salary_snapshot_thread.h"
#include "../../foreign/waiver_core/api/foreign_waiver_core.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/injury/api/foreign_injury.h"
#include "../../foreign/intl_established_fa_postscan/api/intl_established_fa_postscan.h"
#include "../../foreign/signability/state/submit_offer_probe_state.h"
#include "../../hotkey_window/hotkey_window.h"
#include "../../military_service/military_service.h"
#include "../../military_service/calendar/military_service_date.h"
#include "../../military_service/players/team_policy/military_service_team_policy.h"
#include "../../patch_installers/arbitration/no_withdraw/arbitration_no_withdraw_patch.h"
#include "../../patch_installers/no_minor_contracts/core/api/no_minor_patch.h"
#include "../../patch_installers/allstar/candidate/patch_installers_allstar_candidate.h"
#include "../../patch_installers/allstar/events/patch_installers_allstar_events.h"
#include "../../patch_installers/allstar/settings/patch_installers_allstar_settings.h"
#include "../../patch_installers/allstar/static/patch_installers_allstar_static.h"
#include "../../patch_installers/foreign/ai_fa/fallback/patch_installers_foreign_ai_fa_fallback.h"
#include "../../patch_installers/foreign/ai_fa/patch_installers_foreign_ai_fa_status.h"
#include "../../patch_installers/foreign/roster_limits/callup/patch_installers_foreign_callup_limits.h"
#include "../../patch_installers/foreign/roster_limits/counts/patch_installers_foreign_counts.h"
#include "../../patch_installers/foreign/signability/entry/patch_installers_foreign_signability_entry.h"
#include "../../patch_installers/foreign/signability/signing/patch_installers_foreign_signing_branch.h"
#include "../../patch_installers/foreign/signability/submit_offer/patch_installers_foreign_submit_offer_probe.h"
#include "../../patch_installers/foreign/roster_limits/trade/patch_installers_foreign_trade_check.h"
#include "../../patch_installers/foreign/intl_established_fa/patch_installers_intl_established_fa.h"
#include "../../patch_installers/military/patch_installers_military.h"
#include "../../patch_installers/season_phase/patch_installers_season_phase_probe.h"
#include "../../season_phase_monitor/season_phase_monitor.h"

#include "../entrypoint_internal.h"
HANDLE g_kbo_process_instance_mutex = NULL;
volatile LONG g_kbo_sangmu_fa_hooks_install_started = 0;
volatile LONG g_kbo_full_runtime_install_started = 0;
volatile LONG g_kbo_full_runtime_marker_wait_started = 0;
volatile LONG g_kbo_runtime_date_stable_ready = 0;

typedef struct KboSangmuFaHookInstallRequest {
    int enable_signability;
    int enable_offer;
} KboSangmuFaHookInstallRequest;








