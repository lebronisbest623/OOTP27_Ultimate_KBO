#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "foreign_waiver_policy.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"

int kbo_foreign_waiver_ai_enabled(void)
{
    static LONG cached = -1;
    LONG value = cached;
    if (value != -1) {
        return value == 1;
    }
    value = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_WAIVER_AI_FILE) ? 1 : 0;
    InterlockedCompareExchange(&cached, value, -1);
    return cached == 1;
}
int kbo_custom_foreign_policy_enabled(void)
{
    static LONG cached_disabled = -1;
    LONG disabled = cached_disabled;
    if (disabled == -1) {
        disabled = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_CUSTOM_FOREIGN_POLICY_FILE) ? 1 : 0;
        InterlockedCompareExchange(&cached_disabled, disabled, -1);
        disabled = cached_disabled;
    }
    return kbo_fix_enabled() && disabled != 1;
}
