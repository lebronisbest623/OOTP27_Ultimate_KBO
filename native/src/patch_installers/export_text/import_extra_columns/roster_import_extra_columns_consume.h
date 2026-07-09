#ifndef KBOFIX_SRC_PATCH_INSTALLERS_EXPORT_TEXT_IMPORT_EXTRA_COLUMNS_ROSTER_IMPORT_EXTRA_COLUMNS_CONSUME_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_EXPORT_TEXT_IMPORT_EXTRA_COLUMNS_ROSTER_IMPORT_EXTRA_COLUMNS_CONSUME_H_

#include <stdint.h>

#define KBO_ROSTER_IMPORT_CHADWICK_ID_BYTES 80

typedef struct KboRosterImportExtraValues {
    int32_t military_active;
    int32_t military_exempt;
    int32_t military_days_left;
    uint32_t military_return_yyyymmdd;
    uint32_t service_team_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint32_t service_time_days;
    int32_t has_injury_proneness;
    uint32_t injury_proneness_overall;
    uint32_t injury_proneness_back;
    uint32_t injury_proneness_leg;
    uint32_t injury_proneness_arm;
    int32_t has_chadwick_id;
    char chadwick_id[KBO_ROSTER_IMPORT_CHADWICK_ID_BYTES];
    int32_t has_popularity;
    uint32_t local_popularity;
    uint32_t national_popularity;
} KboRosterImportExtraValues;

void kbo_roster_import_apply_extra_values(
    uint8_t* player,
    const KboRosterImportExtraValues* values);

void kbo_roster_import_extra_columns_consume(uint8_t* player, void* row, int32_t* column_index);

void kbo_roster_import_extra_columns_consume_tail(uint8_t* player, void* row);

#endif
