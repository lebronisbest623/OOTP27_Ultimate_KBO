#ifndef KBOFIX_SRC_CAPTAIN_INTERNAL_CAPTAIN_SELECTION_INTERNAL_H_
#define KBOFIX_SRC_CAPTAIN_INTERNAL_CAPTAIN_SELECTION_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/season/phase/season_phase.h"
#include "../../core/sync/spin_lock.h"
#include "../../core/teams/core_team_collect.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"
#include "../../team/names/team_string.h"
#include "../api/captain_selection.h"
#include "../season/captain_season.h"
#include "../seed/parse/captain_seed_parse.h"

#define KBO_CAPTAIN_MAX_TEAMS 64

typedef struct KboCaptainSelectionRow {
    uint32_t date;
    uint32_t season;
    uint32_t league_id;
    uint32_t team_id;
    char team_name[128];
    uint32_t player_id;
    char player_name[128];
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t current_league_id;
    uint16_t age;
    int32_t salary;
    int32_t value_score;
    int32_t score;
    int32_t same_team_seasons;
    int32_t seed_priority;
    int16_t overall_value;
    int16_t talent_value;
    int16_t ratings_value;
    int16_t career_value;
    uint8_t domestic;
    uint8_t dfa;
    uint8_t restricted;
    uint8_t injured;
    uint8_t seeded;
    char seed_source[24];
    char reason[96];
} KboCaptainSelectionRow;

extern volatile LONG g_kbo_captain_preseason_thread_started;
extern volatile LONG g_kbo_captain_last_attempted_season;
extern KboCaptainSeed g_kbo_captain_seeds[KBO_CAPTAIN_SEED_MAX];
extern int g_kbo_captain_seed_count;
extern KboSpinLock g_kbo_captain_seed_lock;
extern volatile LONG g_kbo_captain_seed_loaded;
extern char g_kbo_captain_seed_loaded_key[MAX_PATH * 6];

int kbo_captain_selection_csv_path(uint32_t season, char* out, size_t out_size);
int kbo_captain_selection_csv_exists(uint32_t season);
int kbo_captain_load_selection_csv(
    uint32_t season,
    KboCaptainSelectionRow* rows,
    int max_rows);
int kbo_captain_write_selection_csv(
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source,
    char* out_path,
    size_t out_path_size);
int kbo_captain_select_for_preseason(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    KboCaptainSelectionRow* rows,
    int max_rows,
    int* out_selected_count);
DWORD WINAPI kbo_captain_preseason_selection_thread(LPVOID parameter);
void kbo_lock_captain_seeds(void);
void kbo_unlock_captain_seeds(void);
int kbo_get_save_captain_seed_path(char* out, size_t out_size);
int kbo_get_global_captain_seed_path(char* out, size_t out_size);
int kbo_import_captain_seed_file_locked(const char* path, const char* source);
void kbo_ensure_captain_seeds_loaded(void);
int kbo_captain_seed_available_for_season(uint32_t season, uint32_t league_id);
int kbo_find_captain_seed_for_player(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint8_t* team,
    uint8_t* player,
    KboCaptainSeed* out_seed);
int kbo_captain_seed_matches_player(const KboCaptainSeed* seed, uint8_t* player);
int kbo_find_best_captain_seed_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint8_t* team,
    KboCaptainSeed* out_seed);
int kbo_emit_captain_initial_selection_news(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source);
int kbo_captain_initial_selection_news_exists(uint32_t season, uint32_t league_id);
int kbo_emit_captain_replacement_news(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const KboCaptainSelectionRow* old_row,
    const KboCaptainSelectionRow* new_row,
    const char* source);
int kbo_captain_current_yyyymmdd(uint32_t* out_date);
int kbo_captain_calendar_preseason_start_active(
    uint32_t date,
    uint32_t league_season,
    uint8_t phase,
    int calendar_preseason);
void kbo_captain_log_phase_observed(
    const char* source,
    uint32_t date,
    uint32_t league_id,
    uintptr_t league_ptr,
    uint32_t league_season,
    uint32_t effective_season,
    uint8_t phase,
    int csv_exists,
    int calendar_recovery,
    int calendar_preseason);
int kbo_captain_find_row_index_by_team(
    const KboCaptainSelectionRow* rows,
    int row_count,
    uint32_t team_id);
int kbo_captain_existing_row_still_with_team(const KboCaptainSelectionRow* row);
int kbo_captain_current_rows_need_inseason_repair(
    uint32_t league_id,
    const KboCaptainSelectionRow* current_rows,
    int current_count,
    uint32_t* out_team_ids,
    int* out_team_count,
    int* out_missing_count,
    int* out_departed_count);
int kbo_captain_write_initial_selection(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const char* source);
int kbo_captain_write_missing_selection_csv(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint8_t phase,
    const char* source);
int kbo_captain_run_seed_startup_without_league_ptr(
    uint32_t date,
    uint32_t league_id,
    const char* source);
int kbo_run_captain_inseason_repair_once(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const char* source);
int kbo_run_captain_selection_maintenance_for_date(uint32_t date, const char* source);
int kbo_run_captain_selection_maintenance_once(const char* source);

#endif
