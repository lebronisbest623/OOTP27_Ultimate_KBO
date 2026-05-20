#include "patch_installers_allstar_common.h"
#include <stdio.h>
#include <string.h>
#include "../../common/patch_host.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../patch_helpers/patch_helpers.h"

HMODULE kbo_allstar_get_host_exe(const char* label)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtimef("GetModuleHandleA(NULL) failed for %s", label);
        return NULL;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping %s host=%s", label, host);
        return NULL;
    }

    if (!kbo_fix_enabled()) {
        kbo_log_runtimef("%s skipped: opt-in disabled", label);
        return NULL;
    }

    return exe;
}

int kbo_patch_static_pattern(
    const char* label,
    const uint8_t* pattern,
    size_t pattern_size,
    size_t patch_offset,
    const uint8_t* expected,
    const uint8_t* patch,
    size_t patch_size)
{
    uint8_t* base = find_ootp_executable_pattern(pattern, pattern_size);
    if (base == NULL) {
        return 0;
    }
    return patch_static_bytes(label, base + patch_offset, expected, patch, patch_size);
}
