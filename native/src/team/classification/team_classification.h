#ifndef KBOFIX_SRC_TEAM_CLASSIFICATION_H_
#define KBOFIX_SRC_TEAM_CLASSIFICATION_H_

#include <stdint.h>

#include "parse/team_classification_seed_parse.h"

typedef struct KboIndependentFuturesTeamLeague {
    uint32_t team_id;
    uint32_t league_id;
    char team_csv_id[16];
    char display_name[96];
} KboIndependentFuturesTeamLeague;

int kbo_collect_independent_futures_team_leagues(
    KboIndependentFuturesTeamLeague* out,
    int max_count,
    int* out_seed_rows,
    int* out_unresolved_rows);
int kbo_team_classification_seed_source_available(void);
int kbo_team_classification_independent_kind_for_team(uint32_t team_id);
int kbo_team_classification_league_has_independent_futures_team(uint32_t league_id);

#endif
