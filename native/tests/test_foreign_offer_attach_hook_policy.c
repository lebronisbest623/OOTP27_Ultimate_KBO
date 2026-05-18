#include <assert.h>
#include <stdio.h>

#include "../src/foreign/signability/foreign_policy/wrappers/offer_attach/install_policy/foreign_ai_offer_attach_hook_policy.h"

static void test_enhanced_foreign_ai_installs_offer_attach_hook(void)
{
    assert(kbo_foreign_ai_offer_attach_hook_required(1, 0, 0, 0));
    assert(kbo_foreign_ai_offer_attach_hook_required(0, 1, 0, 0));
}

static void test_diagnostics_still_install_offer_attach_hook(void)
{
    assert(kbo_foreign_ai_offer_attach_hook_required(0, 0, 1, 0));
    assert(kbo_foreign_ai_offer_attach_hook_required(0, 0, 0, 1));
}

static void test_offer_attach_hook_not_required_without_feature_or_diagnostics(void)
{
    assert(!kbo_foreign_ai_offer_attach_hook_required(0, 0, 0, 0));
}

int main(void)
{
    test_enhanced_foreign_ai_installs_offer_attach_hook();
    test_diagnostics_still_install_offer_attach_hook();
    test_offer_attach_hook_not_required_without_feature_or_diagnostics();
    printf("All foreign offer attach hook policy tests passed.\n");
    return 0;
}
