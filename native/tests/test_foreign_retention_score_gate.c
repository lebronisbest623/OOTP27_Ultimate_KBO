#include <assert.h>
#include <stdio.h>

#include "../src/foreign/quota/candidates/retention_score/foreign_quota_retention_score_gate.h"

static void test_better_new_candidate_clears_retained_best(void)
{
    assert(kbo_retention_candidate_score_clears_best(146335, 134925));
    printf("test_better_new_candidate_clears_retained_best: PASS\n");
}

static void test_equal_new_candidate_clears_retained_best(void)
{
    assert(kbo_retention_candidate_score_clears_best(134925, 134925));
    printf("test_equal_new_candidate_clears_retained_best: PASS\n");
}

static void test_worse_new_candidate_does_not_clear_retained_best(void)
{
    assert(!kbo_retention_candidate_score_clears_best(134924, 134925));
    printf("test_worse_new_candidate_does_not_clear_retained_best: PASS\n");
}

static void test_retention_does_not_consume_open_slot(void)
{
    assert(!kbo_retention_candidate_consumes_last_effective_slot(2u, 3u));
    printf("test_retention_does_not_consume_open_slot: PASS\n");
}

static void test_retention_consumes_last_slot_at_limit(void)
{
    assert(kbo_retention_candidate_consumes_last_effective_slot(3u, 3u));
    printf("test_retention_consumes_last_slot_at_limit: PASS\n");
}

static void test_retention_consumes_last_slot_over_limit(void)
{
    assert(kbo_retention_candidate_consumes_last_effective_slot(4u, 3u));
    printf("test_retention_consumes_last_slot_over_limit: PASS\n");
}

static void test_retention_zero_limit_never_blocks(void)
{
    assert(!kbo_retention_candidate_consumes_last_effective_slot(1u, 0u));
    printf("test_retention_zero_limit_never_blocks: PASS\n");
}

static void test_retention_slot_reservation_stays_active_inside_window(void)
{
    assert(kbo_retention_candidate_slot_reservation_active(20261013u, 20261212u, 60u));
    printf("test_retention_slot_reservation_stays_active_inside_window: PASS\n");
}

static void test_retention_slot_reservation_expires_after_window(void)
{
    assert(!kbo_retention_candidate_slot_reservation_active(20261013u, 20261213u, 60u));
    printf("test_retention_slot_reservation_expires_after_window: PASS\n");
}

static void test_retention_slot_reservation_keeps_invalid_dates_safe(void)
{
    assert(kbo_retention_candidate_slot_reservation_active(0u, 20261213u, 60u));
    assert(kbo_retention_candidate_slot_reservation_active(20261013u, 0u, 60u));
    printf("test_retention_slot_reservation_keeps_invalid_dates_safe: PASS\n");
}

static void test_open_market_candidate_keeps_open_slot(void)
{
    assert(!kbo_retention_open_market_candidate_replacement_allowed(2u, 3u, 1, 0));
    printf("test_open_market_candidate_keeps_open_slot: PASS\n");
}

static void test_open_market_candidate_can_be_replaced_for_last_slot(void)
{
    assert(kbo_retention_open_market_candidate_replacement_allowed(3u, 3u, 1, 0));
    printf("test_open_market_candidate_can_be_replaced_for_last_slot: PASS\n");
}

static void test_open_market_candidate_clearing_score_is_not_replaced(void)
{
    assert(!kbo_retention_open_market_candidate_replacement_allowed(3u, 3u, 1, 1));
    printf("test_open_market_candidate_clearing_score_is_not_replaced: PASS\n");
}

static void test_expired_retention_reserve_does_not_replace_candidate(void)
{
    assert(!kbo_retention_open_market_candidate_replacement_allowed(3u, 3u, 0, 0));
    printf("test_expired_retention_reserve_does_not_replace_candidate: PASS\n");
}

int main(void)
{
    test_better_new_candidate_clears_retained_best();
    test_equal_new_candidate_clears_retained_best();
    test_worse_new_candidate_does_not_clear_retained_best();
    test_retention_does_not_consume_open_slot();
    test_retention_consumes_last_slot_at_limit();
    test_retention_consumes_last_slot_over_limit();
    test_retention_zero_limit_never_blocks();
    test_retention_slot_reservation_stays_active_inside_window();
    test_retention_slot_reservation_expires_after_window();
    test_retention_slot_reservation_keeps_invalid_dates_safe();
    test_open_market_candidate_keeps_open_slot();
    test_open_market_candidate_can_be_replaced_for_last_slot();
    test_open_market_candidate_clearing_score_is_not_replaced();
    test_expired_retention_reserve_does_not_replace_candidate();
    printf("All foreign retention score gate tests passed.\n");
    return 0;
}
