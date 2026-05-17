#include "military_service_date.h"
#include <stdio.h>
#include <string.h>
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/logging/core_log.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"

/* Military service current-date helpers. */

uint32_t kbo_current_date_serial(void)
{
    uint32_t yyyymmdd = 0u;
    if (!kbo_current_date_tick_latest_published_date(&yyyymmdd)) {
        return 0;
    }
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    return kbo_date_serial(year, month, day);
}
