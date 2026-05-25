#ifndef NATIVE_SRC_FOREIGN_ROSTER_AUDIT_FOREIGN_ROSTER_AUDIT_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_ROSTER_AUDIT_FOREIGN_ROSTER_AUDIT_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/files/save_paths/platform/core_path_io.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../paths/foreign_roster_audit_paths.h"
#define KBO_FOREIGN_ROSTER_AUDIT_MAX 8192
typedef struct KboForeignRosterAuditState {
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t current_league_id;
    uint32_t loan_team_id;
    uint8_t restricted;
    uint8_t secondary_restricted;
    uint8_t dfa;
    uint8_t loan_active;
    uint8_t injury_active;
    int score;
    uint32_t seen_generation;
} KboForeignRosterAuditState;

extern KboForeignRosterAuditState g_kbo_foreign_roster_audit[KBO_FOREIGN_ROSTER_AUDIT_MAX];
extern int g_kbo_foreign_roster_audit_count;
extern uint32_t g_kbo_foreign_roster_audit_generation;
extern char g_kbo_foreign_roster_audit_save_path[KBO_UTF8_PATH_BYTES];

int kbo_foreign_roster_audit_csv_empty(HANDLE file);
uint32_t kbo_foreign_roster_audit_get_player_original_team_id(uint8_t* player);
const char* kbo_foreign_roster_audit_change_type(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state);
KboForeignRosterAuditState* kbo_find_foreign_roster_audit_state(uint32_t player_id);
void kbo_capture_foreign_roster_audit_state(uint8_t* player, KboForeignRosterAuditState* out);
int kbo_foreign_roster_audit_state_changed(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state);
int append_foreign_roster_audit_csv_header(HANDLE file);
int append_foreign_roster_snapshot_csv_header(HANDLE file);
HANDLE kbo_open_foreign_roster_audit_append_file(void);
void kbo_write_foreign_roster_audit_change(
    HANDLE file,
    const char* date,
    const char* source,
    const char* change_type,
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state);
void kbo_write_foreign_roster_snapshot_row(
    HANDLE file,
    const char* date,
    const char* source,
    const KboForeignRosterAuditState* state);
HANDLE kbo_open_foreign_roster_snapshot_file(void);
void kbo_close_foreign_roster_snapshot_file(HANDLE file);
void audit_foreign_roster_state(const char* source, int write_snapshot);
uint32_t kbo_foreign_roster_daily_load_last_audit_date(const char* source);
void kbo_foreign_roster_daily_persist_last_audit_date(uint32_t today, const char* source);

#endif
