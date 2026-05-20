#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../core/core_flags/api/flags_api.h"
#include "foreign_ai_controller.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

int kbo_foreign_ai_controller_enabled(void)
{
    enum { KBO_FOREIGN_AI_CONTROLLER_FLAG_CACHE_MS = 500u };
    static volatile LONG s_cached_tick = 0;
    static volatile LONG s_cached_enabled = 0;

    DWORD now = GetTickCount();
    LONG cached_tick = InterlockedCompareExchange(&s_cached_tick, 0, 0);
    if (cached_tick != 0
            && (DWORD)(now - (DWORD)cached_tick) <= KBO_FOREIGN_AI_CONTROLLER_FLAG_CACHE_MS) {
        return InterlockedCompareExchange(&s_cached_enabled, 0, 0) != 0;
    }

    int enabled = kbo_fix_enabled()
        && read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_CONTROLLER_FILE)
        && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_FOREIGN_AI_CONTROLLER_FILE);
    InterlockedExchange(&s_cached_enabled, enabled ? 1 : 0);
    InterlockedExchange(&s_cached_tick, (LONG)now);
    return enabled;
}
