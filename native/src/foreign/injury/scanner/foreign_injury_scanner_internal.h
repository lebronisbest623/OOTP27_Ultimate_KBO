#ifndef KBOFIX_FOREIGN_INJURY_SCANNER_INTERNAL_H_
#define KBOFIX_FOREIGN_INJURY_SCANNER_INTERNAL_H_

#include "../internal/foreign_injury_internal.h"

#define KBO_FOREIGN_INJURY_DECISION_KEEP_INJURED      1u
#define KBO_FOREIGN_INJURY_DECISION_KEEP_REPLACEMENT  2u

typedef struct KboForeignInjuryReplacementDecision {
    uint8_t choice;
    int32_t injured_score;
    int32_t replacement_score;
    int32_t score_margin;
    int32_t required_margin;
    char reason[64];
} KboForeignInjuryReplacementDecision;

typedef struct KboForeignInjuryClosedNews {
    KboForeignInjuryReplacement rec;
    KboForeignInjuryReplacementDecision decision;
    char phase[32];
} KboForeignInjuryClosedNews;

typedef struct KboForeignInjuryPendingDiagnosis {
    uint32_t player_id;
    uint32_t team_id;
    uint32_t league_id;
    uint32_t first_seen_date;
    uint32_t last_seen_date;
    uint8_t slot_type;
    uint8_t active;
} KboForeignInjuryPendingDiagnosis;

int kbo_foreign_injury_player_matches_team(uint8_t* player, uint32_t team_id);
int kbo_foreign_injury_candidate_matches_slot(uint8_t* player, uint8_t slot_type);
int kbo_foreign_injury_player_has_baseball_position(uint8_t* player);
int kbo_foreign_injury_team_active_roster_contains_player(uint8_t* team, uint32_t player_id);
int kbo_foreign_injury_team_inactive_roster_contains_player(uint8_t* team, uint32_t player_id);
int kbo_foreign_injury_team_known_roster_contains_player(uint8_t* team, uint32_t player_id);
int kbo_foreign_injury_player_on_inactive_replacement_roster(
    uint8_t* player,
    uint32_t player_id,
    uint32_t top_team_id,
    uint32_t today_yyyymmdd);
int kbo_foreign_injury_resolve_player_team_assignment(
    uint8_t* player,
    uint32_t player_id,
    uint32_t configured_league_id,
    uint32_t* out_team_id,
    uint32_t* out_league_id);
int kbo_foreign_injury_injured_player_returned_to_org_roster(
    const KboForeignInjuryReplacement* rec,
    uint8_t* injured);
uint32_t kbo_foreign_injury_resolve_replacement_for_record(
    const KboForeignInjuryReplacement* rec,
    const KboForeignInjuryReplacement* snapshot,
    int snapshot_count);
int kbo_foreign_injury_choose_returning_player(
    const KboForeignInjuryReplacement* rec,
    uint8_t* injured,
    uint8_t* replacement,
    KboForeignInjuryReplacementDecision* out);
const char* kbo_foreign_injury_decision_choice_label(uint8_t choice);
void kbo_foreign_injury_emit_closed_news_batch(
    const KboForeignInjuryClosedNews* closed_news,
    int closed_count,
    uint32_t today,
    const char* source);
int kbo_foreign_injury_restore_active_replacement_player(const KboForeignInjuryReplacement* rec, const char* source);
int kbo_foreign_injury_release_replacement_player(uint32_t team_id, uint32_t player_id, const char* source);
int kbo_foreign_injury_release_injured_player(uint32_t team_id, uint32_t player_id, const char* source);
int kbo_foreign_injury_replacement_scan_source_is_read_only(const char* source);
int kbo_foreign_injury_same_date_idle_scan_cached(uint32_t today, const char* source);
void kbo_foreign_injury_note_same_date_idle_scan(uint32_t today, const char* source, int idle);
int kbo_foreign_injury_same_date_existing_idle_cached(uint32_t today, uint64_t fingerprint);
void kbo_foreign_injury_note_same_date_existing_idle(uint32_t today, uint64_t fingerprint, int idle);
int kbo_foreign_injury_pending_diagnosis_find(
    uint32_t player_id,
    KboForeignInjuryPendingDiagnosis* out);
void kbo_foreign_injury_pending_diagnosis_clear(uint32_t player_id);
void kbo_foreign_injury_note_pending_diagnosis(
    uint32_t player_id,
    uint32_t team_id,
    uint32_t league_id,
    uint8_t slot_type,
    uint32_t today,
    const char* source);
void kbo_foreign_injury_memory_probe_observe(
    uint8_t* player,
    uint32_t player_id,
    uint32_t team_id,
    uint32_t league_id,
    uint8_t slot_type,
    uint32_t today,
    uint8_t injury_active,
    int16_t injury_aux_0876,
    int inactive_roster_present,
    int has_assignment,
    const char* source);
void kbo_foreign_injury_process_existing_replacements(
    uint32_t today,
    const char* source,
    int* out_active_count,
    int* out_closed_count);

#endif
