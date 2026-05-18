#include "foreign_ai_offer_attach_hook_policy.h"

int kbo_foreign_ai_offer_attach_hook_required(
    int foreign_ai_roster_management,
    int foreign_ai_controller,
    int research_hooks,
    int explicit_probe)
{
    return foreign_ai_roster_management
        || foreign_ai_controller
        || research_hooks
        || explicit_probe;
}
