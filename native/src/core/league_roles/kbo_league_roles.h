#ifndef KBOFIX_SRC_CORE_LEAGUE_ROLES_KBO_LEAGUE_ROLES_H_
#define KBOFIX_SRC_CORE_LEAGUE_ROLES_KBO_LEAGUE_ROLES_H_

#include <stdint.h>

#define KBO_LEAGUE_ROLES_FILE "league_roles.json"

typedef struct KboLeagueRoles {
    uint32_t main_league_id;
    uint32_t independent_league_id;
    uint32_t college_league_id;
    uint32_t high_school_league_id;
} KboLeagueRoles;

const KboLeagueRoles* kbo_league_roles(void);
uint32_t kbo_league_role_main_league_id(void);
uint32_t kbo_league_role_independent_league_id(void);
uint32_t kbo_league_role_college_league_id(void);
uint32_t kbo_league_role_high_school_league_id(void);
int kbo_league_role_is_amateur(uint32_t league_id);

#endif
