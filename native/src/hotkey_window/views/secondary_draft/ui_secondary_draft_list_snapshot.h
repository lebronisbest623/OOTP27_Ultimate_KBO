#ifndef KBOFIX_SRC_HOTKEY_WINDOW_UI_SECONDARY_DRAFT_LIST_SNAPSHOT_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_UI_SECONDARY_DRAFT_LIST_SNAPSHOT_H_

#include <stdint.h>

#include "../../../custom_events/secondary_draft/secondary_draft.h"

typedef struct KboSecondaryDraftListUiSnapshot {
    uint32_t season;
    uint32_t team_id;
    uint32_t today;
    int has_window;
    int window_open;
    int submitted;
    int saved_count;
    int count;
    KboSecondaryDraftWindow window;
    KboSecondaryDraftCandidateRow rows[KBO_SECONDARY_DRAFT_UI_MAX_CANDIDATES];
} KboSecondaryDraftListUiSnapshot;

int kbo_secondary_draft_ui_list_snapshot_get(
    uint32_t season,
    uint32_t team_id,
    KboSecondaryDraftListUiSnapshot* out_snapshot,
    int* out_updating);
void kbo_secondary_draft_ui_list_snapshot_invalidate(void);

#endif
