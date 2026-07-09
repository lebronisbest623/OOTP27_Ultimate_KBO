#ifndef KBOFIX_SRC_FOREIGN_INJURY_FOREIGN_INJURY_H_
#define KBOFIX_SRC_FOREIGN_INJURY_FOREIGN_INJURY_H_

#include <stdint.h>

#define KBO_FOREIGN_INJURY_REPLACEMENT_MAX      256
#ifndef KBO_FOREIGN_INJURY_SLOT_REGULAR
#define KBO_FOREIGN_INJURY_SLOT_REGULAR         1
#endif
#ifndef KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA     2
#endif
#define KBO_FOREIGN_INJURY_STATUS_OPEN          1
#define KBO_FOREIGN_INJURY_STATUS_ACTIVE        2
#define KBO_FOREIGN_INJURY_STATUS_PENDING       3
#define KBO_FOREIGN_INJURY_STATUS_CLOSED        4
#define KBO_FOREIGN_INJURY_CLOSE_KEEP_INJURED      1u
#define KBO_FOREIGN_INJURY_CLOSE_KEEP_REPLACEMENT  2u
#define KBO_FOREIGN_INJURY_CLOSE_OFFSEASON_RESET   3u

typedef struct KboForeignInjuryReplacement {
    uint32_t team_id;
    uint32_t league_id;
    uint32_t injured_player_id;
    uint32_t replacement_player_id;
    uint32_t opened_on_yyyymmdd;
    uint32_t expected_end_yyyymmdd;
    uint32_t injury_id;
    uint32_t closed_on_yyyymmdd;
    uint8_t  slot_type;
    uint8_t  status;
    uint8_t  converted;
    uint8_t  close_choice;
} KboForeignInjuryReplacement;

#ifndef KBO_FOREIGN_INJURY_LIVE_MEMORY_DEFINED
#define KBO_FOREIGN_INJURY_LIVE_MEMORY_DEFINED
typedef struct KboForeignInjuryLiveMemory {
    uint8_t active;
    uint8_t pending_diagnosis;
    uint8_t day_to_day;
    uint8_t career_ending;
    uint32_t injury_id;
    int32_t active_count;
    int32_t days_left;
    int32_t total_days;
    uintptr_t injury_object;
} KboForeignInjuryLiveMemory;
#endif

#ifndef KBO_FOREIGN_INJURY_EXCLUSION_SNAPSHOT_DEFINED
#define KBO_FOREIGN_INJURY_EXCLUSION_SNAPSHOT_DEFINED
/* Pre-resolved view of the open/active replacement records for foreign-count
 * exclusion checks inside per-player scan loops. Built once per scan so the
 * loop does not take the record lock or resolve team orgs per player. */
typedef struct KboForeignInjuryExclusionSnapshot {
    int count;
    uint32_t org_team_ids[KBO_FOREIGN_INJURY_REPLACEMENT_MAX];
    KboForeignInjuryReplacement records[KBO_FOREIGN_INJURY_REPLACEMENT_MAX];
} KboForeignInjuryExclusionSnapshot;
#endif

void kbo_foreign_injury_build_exclusion_snapshot(KboForeignInjuryExclusionSnapshot* out);
int kbo_foreign_injury_player_excluded_from_foreign_count_snapshot(
    const KboForeignInjuryExclusionSnapshot* snapshot,
    uint32_t team_id,
    uint32_t player_id);

extern KboForeignInjuryReplacement g_kbo_foreign_injury_replacements[KBO_FOREIGN_INJURY_REPLACEMENT_MAX];
extern int g_kbo_foreign_injury_replacement_count;

uint64_t kbo_foreign_injury_replacement_fingerprint(void);
const char* kbo_foreign_injury_slot_label(uint8_t slot_type);
const char* kbo_foreign_injury_status_label(uint8_t status);
int kbo_foreign_injury_status_uses_slot(uint8_t status);
uint8_t kbo_foreign_injury_slot_type_for_player(uint8_t* player);
int kbo_foreign_injury_duration_meets_minimum(int16_t days_left, int min_days);
int kbo_foreign_injury_read_live_memory(uint8_t* player, KboForeignInjuryLiveMemory* out);
int kbo_foreign_injury_live_memory_has_long_term_basis(
    const KboForeignInjuryLiveMemory* live,
    int min_days);
int kbo_foreign_injury_runtime_injury_present_from_memory(uint8_t* player);
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
int kbo_foreign_injury_reset_open_replacements_for_offseason(
    uint32_t close_date_yyyymmdd,
    const char* source);
int kbo_foreign_injury_active_record_has_roster_basis(
    uint8_t status,
    uint32_t replacement_player_id,
    int inactive_roster_present);
int kbo_foreign_injury_live_memory_matches_record_episode(
    const KboForeignInjuryReplacement* rec,
    const KboForeignInjuryLiveMemory* live);
int kbo_foreign_injury_live_memory_has_record_continuation_basis(
    const KboForeignInjuryReplacement* rec,
    const KboForeignInjuryLiveMemory* live,
    uint32_t today_yyyymmdd);
int kbo_foreign_injury_closed_record_can_repair_on_date(
    const KboForeignInjuryReplacement* rec,
    const KboForeignInjuryLiveMemory* live,
    uint32_t today_yyyymmdd,
    int inactive_roster_present,
    int roster_hold_flags_present);
int kbo_foreign_injury_replacement_player_attached_to_record(
    const KboForeignInjuryReplacement* rec,
    uint8_t* replacement);
int kbo_foreign_injury_replacement_player_can_restore_to_record(
    const KboForeignInjuryReplacement* rec,
    uint8_t* replacement);
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
int kbo_foreign_injury_player_excluded_from_foreign_count_locked(uint32_t team_id, uint32_t player_id);
int kbo_foreign_injury_player_excluded_from_foreign_count(uint32_t team_id, uint32_t player_id);
void kbo_lock_foreign_injury_replacements(void);
void kbo_unlock_foreign_injury_replacements(void);
void kbo_lock_foreign_injury_replacements_shared(void);
void kbo_unlock_foreign_injury_replacements_shared(void);

int kbo_foreign_injury_replacement_enabled(void);
void kbo_ensure_foreign_injury_replacements_loaded(void);
int kbo_foreign_injury_replacements_loaded_for_current_save(void);
int kbo_team_has_foreign_injury_slot_for_candidate(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id);
int kbo_team_has_foreign_injury_slot_for_candidate_type_any(
    uint32_t team_id,
    int allow_asian_slot,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id);
int kbo_team_has_foreign_injury_slot_for_candidate_any(
    uint32_t team_id,
    int allow_asian_slot,
    uint32_t candidate_player_id,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id);
int kbo_foreign_injury_replacement_signing_exception_available(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_effective_count,
    uint32_t* out_effective_limit);
int kbo_attach_foreign_injury_replacement_after_signing(
    uint32_t team_id,
    uint8_t* replacement,
    uint8_t slot_type,
    uint32_t injured_player_id,
    const char* source);
int kbo_foreign_injury_replacement_callup_exception_available(
    uintptr_t team_ptr,
    uint8_t* candidate,
    int32_t ootp_limit,
    int check_type,
    uint32_t* out_effective_after,
    uint8_t* out_slot_type);
void kbo_count_foreign_injury_replacements_for_team(
    uint32_t team_id,
    int* out_open,
    int* out_pending,
    int* out_closed);
void kbo_foreign_injury_replacement_scan_captured_date(const char* source, uint32_t today);
void kbo_foreign_injury_replacement_scan_discovery_for_date(const char* source, uint32_t today);
void start_kbo_foreign_injury_date_tick_thread(void);

#endif
