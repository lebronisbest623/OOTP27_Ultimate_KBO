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

int main(void)
{
    test_better_new_candidate_clears_retained_best();
    test_equal_new_candidate_clears_retained_best();
    test_worse_new_candidate_does_not_clear_retained_best();
    printf("All foreign retention score gate tests passed.\n");
    return 0;
}
