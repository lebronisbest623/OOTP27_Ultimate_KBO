/* All-Star settings UI wrapper. */

#include "allstar_flags.h"

#include <windows.h>

#include "../allstar_league_context/allstar_league_context.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"

__declspec(noinline) void ootp_kbo_enable_allstar_setting(uintptr_t league_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (kbo_allstar_league_context_enabled(league_ptr)) {
        enable_kbo_allstar_flags(league_ptr, "allstar_settings_ui");
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "allstar.enable_setting");
    }

    if (enable_kbo_allstar_raw_flags_if_kbo_context(league_ptr, "allstar_settings_ui_raw")) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "allstar.enable_setting");
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (enable_kbo_allstar_flags_for_core_league(league_ptr, league_id, "allstar_settings_ui_core_fallback")) {
        kbo_log_runtimef(
            "KBO all-star settings UI wrote real league flags by core fallback league_id=%u league=%p",
            league_id,
            (void*)league_ptr);
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "allstar.enable_setting");
    }

    kbo_log_runtimef(
        "KBO all-star settings UI could not write real league flags league_id=%u league=%p",
        league_id,
        (void*)league_ptr);
    KBO_HOOK_PROFILE_END(profile_hook, "allstar.enable_setting");
}
