#include "../fa_compensation_due.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"

int kbo_process_due_fa_compensation_protected_lists(const char* source)
{
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        return 0;
    }
    return kbo_process_due_fa_compensation_protected_lists_for_date(today, source);
}
