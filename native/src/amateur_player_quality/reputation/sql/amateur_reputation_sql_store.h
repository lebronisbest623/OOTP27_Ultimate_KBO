#ifndef KBOFIX_SRC_AMATEUR_PLAYER_QUALITY_REPUTATION_SQL_AMATEUR_REPUTATION_SQL_STORE_H_
#define KBOFIX_SRC_AMATEUR_PLAYER_QUALITY_REPUTATION_SQL_AMATEUR_REPUTATION_SQL_STORE_H_

#include <stdint.h>

#include "../../api/amateur_player_quality.h"

typedef struct KboAmateurReputationHistoryRow {
    uint32_t year;
    uint32_t league_id;
    uint32_t team_id;
    uint32_t old_reputation;
    int32_t delta;
    uint32_t new_reputation;
} KboAmateurReputationHistoryRow;

int kbo_amateur_reputation_sql_history_has_year(uint32_t league_id, uint32_t year);
int kbo_amateur_reputation_sql_history_load(
    KboAmateurReputationHistoryRow* rows,
    int capacity,
    int* out_count);
int kbo_amateur_reputation_sql_history_append(
    uint32_t league_id,
    const KboAmateurReputationUpdateRow* rows,
    int row_count,
    const char* source,
    uint32_t year);

#endif
