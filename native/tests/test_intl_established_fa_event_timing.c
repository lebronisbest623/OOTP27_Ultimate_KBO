#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/foreign/intl_established_fa_postscan/internal/intl_established_fa_postscan_internal.h"

static void test_batch_must_match_event_date(void)
{
    KboIntlEstablishedFaPostscanState batch = {
        .expected_count = 12,
        .scheduled_date = 20261031u
    };

    assert(kbo_intl_established_fa_postscan_batch_matches_event_date(&batch, 20261031u));
    assert(!kbo_intl_established_fa_postscan_batch_matches_event_date(&batch, 20261101u));
    assert(!kbo_intl_established_fa_postscan_batch_matches_event_date(&batch, 20261030u));
    assert(!kbo_intl_established_fa_postscan_batch_matches_event_date(NULL, 20261031u));
    assert(!kbo_intl_established_fa_postscan_batch_matches_event_date(&batch, 0u));

    batch.expected_count = 0;
    assert(!kbo_intl_established_fa_postscan_batch_matches_event_date(&batch, 20261031u));

    batch.expected_count = 12;
    batch.scheduled_date = 0u;
    assert(!kbo_intl_established_fa_postscan_batch_matches_event_date(&batch, 20261031u));

    printf("test_batch_must_match_event_date: PASS\n");
}

static void test_stale_event_requires_today_after_event(void)
{
    assert(kbo_intl_established_fa_event_is_stale(20261031u, 20261101u));
    assert(!kbo_intl_established_fa_event_is_stale(20261031u, 20261031u));
    assert(!kbo_intl_established_fa_event_is_stale(20261031u, 20261030u));
    assert(!kbo_intl_established_fa_event_is_stale(0u, 20261101u));
    assert(!kbo_intl_established_fa_event_is_stale(20261031u, 0u));

    printf("test_stale_event_requires_today_after_event: PASS\n");
}

int main(void)
{
    test_batch_must_match_event_date();
    test_stale_event_requires_today_after_event();
    printf("All international established FA event timing tests passed.\n");
    return 0;
}
