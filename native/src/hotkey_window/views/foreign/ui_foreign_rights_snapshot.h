#ifndef KBOFIX_SRC_HOTKEY_WINDOW_UI_FOREIGN_RIGHTS_SNAPSHOT_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_UI_FOREIGN_RIGHTS_SNAPSHOT_H_

#include <stdint.h>

#define KBO_FOREIGN_RIGHTS_UI_MAX_ROWS 500

typedef struct KboForeignRightsUiSnapshotRow {
    uint32_t player_id;
    uint32_t current_team_id;
    uint32_t nation_id;
    uint32_t retained_on;
    uint32_t expires_on;
    uint16_t age;
    uint8_t restricted;
    uint8_t secondary_restricted;
    uint8_t dfa;
    uint8_t loan_active;
    uint8_t injury_active;
    uint8_t has_active_right;
    uint8_t retain_requested;
    uint8_t skip_chosen;
    char player_name[96];
    char team_abbrev[16];
    char uniform_number[8];
    char position_label[24];
} KboForeignRightsUiSnapshotRow;

typedef struct KboForeignRightsUiSnapshot {
    uint32_t selected_team_id;
    uint32_t today;
    uint32_t window_start;
    uint32_t window_end;
    uint32_t top_player_id;
    uint32_t top_current_team_id;
    int window_open;
    int count;
    KboForeignRightsUiSnapshotRow rows[KBO_FOREIGN_RIGHTS_UI_MAX_ROWS];
} KboForeignRightsUiSnapshot;

int kbo_foreign_rights_ui_snapshot_get(
    uint32_t selected_team_id,
    KboForeignRightsUiSnapshot* out_snapshot,
    int* out_updating);
void kbo_foreign_rights_ui_snapshot_invalidate(void);

#endif
