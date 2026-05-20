#ifndef ALLSTAR_LEAGUE_CONTEXT_H
#define ALLSTAR_LEAGUE_CONTEXT_H

#include <stdint.h>
#include <windows.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../build_verify/build_verify.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../core/dates/core_text_date.h"
#include "../../runtime_memory/runtime_memory.h"

typedef struct KboAllstarTeamRow {
    uint16_t year;
    char team_id[16];
    char team_name[96];
    char current_city[64];
    uint8_t side;
} KboAllstarTeamRow;

typedef struct KboAllstarLayout {
    uint32_t rules_flag_offset;
    uint32_t game_flag_offset;
    uint32_t auto_schedule_offset;
    uint32_t team_a_offset;
    uint32_t team_b_offset;
    uint32_t subleague_array_offset;
    uint32_t subleague_count_offset;
    uint32_t league_id_primary_offset;
    uint32_t league_id_fallback_offset;
} KboAllstarLayout;

extern KboAllstarTeamRow g_allstar_team_rows[OOTP27_KBO_MAX_ALLSTAR_TEAM_ROWS];
extern int g_allstar_team_row_count;
extern LONG g_allstar_team_rules_loaded;
extern LONG g_allstar_candidate_seed_log_count;
extern LONG g_allstar_voting_prepare_log_count;
extern LONG g_allstar_team_name_log_count;
extern volatile LONG g_allstar_native_event_generation_in_progress;
extern volatile LONG g_allstar_native_event_generation_done;
extern volatile uintptr_t g_allstar_schedule_import_league_ptr;
extern volatile uintptr_t g_allstar_make_events_ptr;

typedef void (__fastcall *OotpMakeAllstarGameEventsFn)(uintptr_t league_ptr, uint8_t force_create);
typedef void (__fastcall *OotpVectorPushBack)(void* vector, void* value);
typedef void (__fastcall *OotpAllstarTeamSetupFn)(uintptr_t league_ptr);
typedef void (__fastcall *OotpAllstarCandidateRebuildFn)(uintptr_t league_ptr, uint8_t force_rebuild);

KboAllstarLayout kbo_get_allstar_layout(void);
void load_allstar_team_rules_once(void);
int copy_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size);
int team_has_ootp_string_text(uint8_t* team, const char* expected);
int is_kbo_historical_league_context(uintptr_t league_ptr);

/* context_enabled.c */
int kbo_allstar_league_context_enabled(uintptr_t league_ptr);
int kbo_allstar_league_uses_kbo_schedule_file(uintptr_t league_ptr);
int kbo_allstar_raw_kbo_league_context_enabled(uintptr_t league_ptr);
int kbo_allstar_team_matches_league(uint8_t* team, uint32_t primary_league_id, uint32_t fallback_league_id);

/* league_lookup.c */
uintptr_t kbo_scan_league_vec(uintptr_t global, uint32_t vec_off, uint32_t league_id, const KboAllstarLayout* layout);
uintptr_t kbo_find_allstar_league_ptr(uint32_t league_id);

/* team_seed_context.c */
uint8_t kbo_allstar_seed_side_for_team_strings(uint8_t* team, uint32_t league_year);
int kbo_allstar_team_matches_league_ids(uint8_t* team, uint32_t primary_league_id, uint32_t fallback_league_id);
int kbo_allstar_league_has_seeded_division_split(uintptr_t league_ptr);

/* memory_plausibility.c */
uint32_t kbo_allstar_read_u32(uint8_t* base, uint32_t offset);
int kbo_allstar_memory_executable(const void* address);
int kbo_allstar_league_vtable_plausible(uintptr_t league_ptr);
int kbo_allstar_league_core_plausible(uintptr_t league_ptr);

#endif
