#ifndef KBOFIX_SRC_HOTKEY_WINDOW_UI_SECONDARY_DRAFT_DRAFT_SNAPSHOT_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_UI_SECONDARY_DRAFT_DRAFT_SNAPSHOT_H_

#include <stdint.h>

#include "../../../custom_events/secondary_draft/secondary_draft.h"

typedef struct KboSecondaryDraftDraftUiSnapshot {
    uint32_t season;
    uint32_t team_id;
    uint32_t today;
    int has_window;
    int draft_open;
    KboSecondaryDraftWindow window;
    int pool_count;
    KboSecondaryDraftCandidateRow rows[KBO_SECONDARY_DRAFT_UI_MAX_CANDIDATES];
} KboSecondaryDraftDraftUiSnapshot;

int kbo_secondary_draft_ui_draft_snapshot_get(
    uint32_t season,
    uint32_t team_id,
    KboSecondaryDraftDraftUiSnapshot* out_snapshot,
    int* out_updating);
void kbo_secondary_draft_ui_draft_snapshot_invalidate(void);

#endif
