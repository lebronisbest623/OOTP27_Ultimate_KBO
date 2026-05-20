#ifndef KBOFIX_SRC_CAPTAIN_SEED_DISPLAY_CAPTAIN_SEED_DISPLAY_RESOLVE_H_
#define KBOFIX_SRC_CAPTAIN_SEED_DISPLAY_CAPTAIN_SEED_DISPLAY_RESOLVE_H_

#include "../../internal/captain_selection_internal.h"

typedef struct KboCaptainDisplayCache {
    uint32_t season;
    uint32_t league_id;
    uint32_t team_id;
    uint32_t player_id;
    int found;
    char loaded_key[MAX_PATH * 6];
    char player_name[128];
    char source[24];
} KboCaptainDisplayCache;

void kbo_captain_display_loaded_key(uint32_t season, char* out, size_t out_size);
int kbo_captain_compute_display_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    const char* loaded_key,
    KboCaptainDisplayCache* out);

#endif
