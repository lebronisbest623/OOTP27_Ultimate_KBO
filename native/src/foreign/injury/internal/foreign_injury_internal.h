#ifndef NATIVE_SRC_FOREIGN_INJURY_FOREIGN_INJURY_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_INJURY_FOREIGN_INJURY_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/news/live/core_live_news.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/logging/rule_audit.h"
#include "../../../core/sync/lock.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_name_cache.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_player_policy.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../replacement_seed/api/foreign_replacement_seed.h"
#include "../paths/foreign_injury_paths.h"
#ifndef KBO_FOREIGN_INJURY_SLOT_REGULAR
#define KBO_FOREIGN_INJURY_SLOT_REGULAR         1
#endif
#ifndef KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA     2
#endif
#define KBO_FOREIGN_INJURY_REPLACEMENT_MAX      256
#define KBO_FOREIGN_INJURY_STATUS_OPEN          1
#define KBO_FOREIGN_INJURY_STATUS_ACTIVE        2
#define KBO_FOREIGN_INJURY_STATUS_PENDING       3
#define KBO_FOREIGN_INJURY_STATUS_CLOSED        4
typedef struct KboForeignInjuryReplacement {
    uint32_t team_id;
    uint32_t league_id;
    uint32_t injured_player_id;
    uint32_t replacement_player_id;
    uint32_t opened_on_yyyymmdd;
    uint32_t expected_end_yyyymmdd;
    uint8_t  slot_type;
    uint8_t  status;
    uint8_t  converted;
} KboForeignInjuryReplacement;

extern KboForeignInjuryReplacement g_kbo_foreign_injury_replacements[KBO_FOREIGN_INJURY_REPLACEMENT_MAX];
extern int g_kbo_foreign_injury_replacement_count;
extern KboLock g_kbo_foreign_injury_replacement_lock;
extern char g_kbo_foreign_injury_replacement_loaded_path[MAX_PATH];
extern LONG g_kbo_foreign_injury_date_tick_thread_started;
extern LONG g_kbo_foreign_injury_sql_watch_thread_started;

int kbo_foreign_injury_replacement_enabled(void);
int kbo_foreign_injury_status_uses_slot(uint8_t status);
uint8_t kbo_foreign_injury_slot_type_for_player(uint8_t* player);
int kbo_foreign_injury_player_excluded_from_foreign_count_locked(uint32_t team_id, uint32_t player_id);
int kbo_foreign_injury_player_excluded_from_foreign_count(uint32_t team_id, uint32_t player_id);
/* Inactive roster only corroborates a known long-term injury; it is never a duration source by itself. */
int kbo_foreign_injury_inactive_roster_has_long_term_injury_basis(
    uint8_t injury_active,
    int16_t days_left,
    int min_days,
    int inactive_roster_present);
int kbo_foreign_injury_duration_meets_minimum(int16_t days_left, int min_days);
int kbo_foreign_injury_duration_text_meets_minimum(
    const char* text,
    int min_days,
    int* out_days);
int kbo_foreign_injury_expected_end_reached(
    uint32_t today_yyyymmdd,
    uint32_t expected_end_yyyymmdd);
int kbo_foreign_injury_expected_end_pending(
    uint32_t today_yyyymmdd,
    uint32_t expected_end_yyyymmdd);
uint32_t kbo_foreign_injury_expected_end_from_duration(
    uint32_t anchor_yyyymmdd,
    int duration_days);
int kbo_foreign_injury_sql_evidence_date_allowed(
    uint32_t scan_date_yyyymmdd,
    uint32_t evidence_date_yyyymmdd,
    int duration_days,
    int allow_backdated);
uint32_t kbo_foreign_injury_slot_opened_on_from_sql_evidence(
    uint32_t scan_date_yyyymmdd,
    uint32_t evidence_date_yyyymmdd,
    int allow_backdated);
int kbo_foreign_injury_replacement_phase_allows_signing(uint8_t effective_phase);
int kbo_foreign_injury_replacement_phase_allows_close(uint8_t effective_phase);
int kbo_foreign_injury_replacement_in_season_window(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    const char* source,
    const char* context);
int kbo_foreign_injury_replacement_close_decision_allowed(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    const char* source,
    const char* context);
int kbo_foreign_injury_active_record_has_roster_basis(
    uint8_t status,
    uint32_t replacement_player_id,
    int inactive_roster_present);
int kbo_foreign_injury_return_state_allows_close(
    uint8_t injury_active,
    int16_t days_left,
    uint8_t loan_active,
    int active_roster_present,
    int inactive_roster_present,
    int roster_hold_flags_present,
    int close_decision_allowed);
int kbo_foreign_injury_open_news_allowed(
    uint32_t scan_date,
    uint32_t live_date,
    uint32_t opened_on,
    int process_existing_replacements,
    int captured_live_date);
int kbo_foreign_injury_player_on_inactive_replacement_roster(
    uint8_t* player,
    uint32_t player_id,
    uint32_t top_team_id,
    uint32_t today_yyyymmdd);
int kbo_foreign_injury_recent_sql_has_long_term_injury_date(
    uint32_t player_id,
    int min_days,
    int* out_days,
    uint32_t* out_evidence_date);
int kbo_foreign_injury_recent_sql_has_long_term_injury_date_on_date(
    uint32_t player_id,
    int min_days,
    uint32_t game_date_yyyymmdd,
    int* out_days,
    uint32_t* out_evidence_date);
const char* kbo_foreign_injury_slot_label(uint8_t slot_type);
const char* kbo_foreign_injury_status_label(uint8_t status);
void kbo_lock_foreign_injury_replacements(void);
void kbo_unlock_foreign_injury_replacements(void);
int kbo_load_foreign_injury_replacements_locked(const char* path);
int kbo_parse_foreign_injury_replacement_seed_line(
    const char* line,
    uint32_t today,
    KboForeignInjuryReplacement* out);
int kbo_parse_foreign_injury_replacement_key_seed_line(
    const char* line,
    uint32_t today,
    KboForeignInjuryReplacement* out);
int kbo_parse_foreign_injury_replacement_key_seed_fields(
    char fields[][96],
    int field_count,
    uint32_t today,
    KboForeignInjuryReplacement* out);
int kbo_import_foreign_injury_replacement_seed_file_locked(
    const char* path,
    uint32_t today,
    const char* source);
int kbo_persist_foreign_injury_replacements_locked(void);
void kbo_ensure_foreign_injury_replacements_loaded(void);
int kbo_foreign_injury_replacements_loaded_for_current_save(void);
int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed);
int kbo_foreign_injury_replacement_player_reserved_locked(
    uint32_t replacement_player_id,
    const KboForeignInjuryReplacement* owner_rec);
int kbo_team_has_foreign_injury_slot_locked(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id);
int kbo_team_has_foreign_injury_slot(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id);
int kbo_foreign_injury_record_has_minimum_injury_basis(const KboForeignInjuryReplacement* rec);
int kbo_foreign_injury_record_has_minimum_injury_basis_on_date(
    const KboForeignInjuryReplacement* rec,
    uint32_t today);
int kbo_team_has_foreign_injury_slot_for_candidate_locked(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id);
int kbo_team_has_foreign_injury_slot_for_candidate(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id);
int kbo_team_has_foreign_injury_slot_for_candidate_any(
    uint32_t team_id,
    int allow_asian_slot,
    uint32_t candidate_player_id,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id);
int kbo_attach_foreign_injury_replacement_after_signing(
    uint32_t team_id,
    uint8_t* replacement,
    uint8_t slot_type,
    uint32_t injured_player_id,
    const char* source);
void kbo_count_foreign_injury_replacements_for_team(
    uint32_t team_id,
    int* out_open,
    int* out_pending,
    int* out_closed);
void kbo_emit_foreign_injury_replacement_news(
    const KboForeignInjuryReplacement* rec,
    int days_left,
    const char* phase);
void kbo_emit_foreign_injury_replacement_news_on_date(
    const KboForeignInjuryReplacement* rec,
    int days_left,
    const char* phase,
    uint32_t event_date);
void kbo_foreign_injury_replacement_scan_captured_date(const char* source, uint32_t today);
void kbo_foreign_injury_replacement_scan_discovery_for_date(const char* source, uint32_t today);
void kbo_foreign_injury_replacement_scan_sql_settled_for_date(const char* source, uint32_t today);
void kbo_foreign_injury_sql_cache_invalidate_all(const char* source);
void start_kbo_foreign_injury_date_tick_thread(void);
void start_kbo_foreign_injury_sql_watch_thread(void);

#endif
