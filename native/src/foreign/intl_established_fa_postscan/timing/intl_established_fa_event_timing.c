#include "../internal/intl_established_fa_postscan_internal.h"

int kbo_intl_established_fa_postscan_batch_matches_event_date(
    const KboIntlEstablishedFaPostscanState* batch,
    uint32_t event_yyyymmdd)
{
    if (batch == NULL
            || batch->scheduled_date == 0u
            || event_yyyymmdd == 0u
            || batch->expected_count <= 0) {
        return 0;
    }

    return batch->scheduled_date == event_yyyymmdd;
}

int kbo_intl_established_fa_event_is_stale(
    uint32_t event_yyyymmdd,
    uint32_t today_yyyymmdd)
{
    return event_yyyymmdd != 0u
        && today_yyyymmdd != 0u
        && event_yyyymmdd < today_yyyymmdd;
}
