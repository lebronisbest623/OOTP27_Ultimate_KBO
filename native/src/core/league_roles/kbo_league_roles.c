#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "kbo_league_roles.h"

#include "../policy/core_policy.h"

static INIT_ONCE g_kbo_league_roles_once = INIT_ONCE_STATIC_INIT;
static KboLeagueRoles g_kbo_league_roles;

static uint32_t kbo_league_role_u32(const char* key, uint32_t fallback)
{
    int32_t value = kbo_read_clamped_policy_int(
        KBO_LEAGUE_ROLES_FILE,
        key,
        (int32_t)fallback,
        1,
        1000000);
    return value > 0 ? (uint32_t)value : fallback;
}

static BOOL CALLBACK kbo_league_roles_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    g_kbo_league_roles.main_league_id = kbo_league_role_u32("main_league_id", KBO_DEFAULT_MAIN_LEAGUE_ID);
    g_kbo_league_roles.independent_league_id = kbo_league_role_u32("independent_league_id", KBO_DEFAULT_INDEPENDENT_LEAGUE_ID);
    g_kbo_league_roles.college_league_id = kbo_league_role_u32("college_league_id", KBO_DEFAULT_COLLEGE_LEAGUE_ID);
    g_kbo_league_roles.high_school_league_id = kbo_league_role_u32("high_school_league_id", KBO_DEFAULT_HIGH_SCHOOL_LEAGUE_ID);
    return TRUE;
}

const KboLeagueRoles* kbo_league_roles(void)
{
    InitOnceExecuteOnce(&g_kbo_league_roles_once, kbo_league_roles_init_once, NULL, NULL);
    return &g_kbo_league_roles;
}

uint32_t kbo_league_role_main_league_id(void)
{
    return kbo_league_roles()->main_league_id;
}

uint32_t kbo_league_role_independent_league_id(void)
{
    return kbo_league_roles()->independent_league_id;
}

uint32_t kbo_league_role_college_league_id(void)
{
    return kbo_league_roles()->college_league_id;
}

uint32_t kbo_league_role_high_school_league_id(void)
{
    return kbo_league_roles()->high_school_league_id;
}

int kbo_league_role_is_amateur(uint32_t league_id)
{
    const KboLeagueRoles* roles = kbo_league_roles();
    return league_id != 0u
        && (league_id == roles->college_league_id || league_id == roles->high_school_league_id);
}
