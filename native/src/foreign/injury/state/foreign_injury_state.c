#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/news/live/core_live_news.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/season/phase/season_phase.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_name_cache.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../paths/foreign_injury_paths.h"

#include "../internal/foreign_injury_internal.h"
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

KboForeignInjuryReplacement g_kbo_foreign_injury_replacements[KBO_FOREIGN_INJURY_REPLACEMENT_MAX] = {{0}};
int  g_kbo_foreign_injury_replacement_count = 0;
KboLock g_kbo_foreign_injury_replacement_lock = KBO_LOCK_INIT;
char g_kbo_foreign_injury_replacement_loaded_path[MAX_PATH] = {0};

int kbo_persist_foreign_injury_replacements_locked(void);
int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed);

/* Foreign injury replacement labels, slot helpers, and lock helpers. Included from native/KBOFix.c. */

const char* kbo_foreign_injury_slot_label(uint8_t slot_type)
{
    return slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular";
}

const char* kbo_foreign_injury_status_label(uint8_t status)
{
    switch (status) {
    case KBO_FOREIGN_INJURY_STATUS_OPEN:    return "Open";
    case KBO_FOREIGN_INJURY_STATUS_ACTIVE:  return "Active";
    case KBO_FOREIGN_INJURY_STATUS_PENDING: return "Decision due";
    case KBO_FOREIGN_INJURY_STATUS_CLOSED:  return "Closed";
    default:                                return "Unknown";
    }
}

int kbo_foreign_injury_duration_meets_minimum(int16_t days_left, int min_days)
{
    return min_days > 0 && days_left >= min_days;
}


int kbo_foreign_injury_live_memory_has_long_term_basis(
    const KboForeignInjuryLiveMemory* live,
    int min_days)
{
    if (live == NULL || min_days <= 0 || live->active == 0u || live->injury_object == 0u) {
        return 0;
    }
    if (live->career_ending != 0u) {
        return 1;
    }
    return live->days_left >= min_days;
}

int kbo_foreign_injury_runtime_injury_present_from_memory(uint8_t* player)
{
    KboForeignInjuryLiveMemory live;
    if (!kbo_foreign_injury_read_live_memory(player, &live)) {
        return 0;
    }
    return live.active != 0u;
}

int kbo_foreign_injury_expected_end_reached(
    uint32_t today_yyyymmdd,
    uint32_t expected_end_yyyymmdd)
{
    return today_yyyymmdd != 0u
        && expected_end_yyyymmdd != 0u
        && today_yyyymmdd >= expected_end_yyyymmdd;
}

int kbo_foreign_injury_expected_end_pending(
    uint32_t today_yyyymmdd,
    uint32_t expected_end_yyyymmdd)
{
    return today_yyyymmdd != 0u
        && expected_end_yyyymmdd != 0u
        && today_yyyymmdd < expected_end_yyyymmdd;
}

uint32_t kbo_foreign_injury_expected_end_from_duration(
    uint32_t anchor_yyyymmdd,
    int duration_days)
{
    if (anchor_yyyymmdd == 0u || duration_days <= 0) {
        return 0u;
    }
    return kbo_add_days_yyyymmdd(anchor_yyyymmdd, (uint32_t)duration_days);
}

int kbo_foreign_injury_replacement_phase_allows_signing(uint8_t effective_phase)
{
    return effective_phase == KBO_SEASON_PHASE_REGULAR_SEASON;
}

int kbo_foreign_injury_replacement_phase_allows_close(uint8_t effective_phase)
{
    return effective_phase != KBO_SEASON_PHASE_PRESEASON
        && effective_phase != KBO_SEASON_PHASE_UNKNOWN;
}

int kbo_foreign_injury_active_record_has_roster_basis(
    uint8_t status,
    uint32_t replacement_player_id,
    int inactive_roster_present)
{
    return status == KBO_FOREIGN_INJURY_STATUS_ACTIVE
        && replacement_player_id != 0u
        && inactive_roster_present;
}

int kbo_foreign_injury_live_memory_matches_record_episode(
    const KboForeignInjuryReplacement* rec,
    const KboForeignInjuryLiveMemory* live)
{
    if (rec == NULL || live == NULL || live->active == 0u || live->injury_object == 0u) {
        return 0;
    }
    if (rec->injury_id == 0u) {
        return 1;
    }
    return live->injury_id != 0u && live->injury_id == rec->injury_id;
}

int kbo_foreign_injury_live_memory_has_record_continuation_basis(
    const KboForeignInjuryReplacement* rec,
    const KboForeignInjuryLiveMemory* live,
    uint32_t today_yyyymmdd)
{
    if (rec == NULL || live == NULL || live->active == 0u) {
        return 0;
    }

    int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
    if (kbo_foreign_injury_live_memory_has_long_term_basis(live, min_days)
            && kbo_foreign_injury_live_memory_matches_record_episode(rec, live)) {
        return 1;
    }
    if (live->pending_diagnosis != 0u || live->day_to_day != 0u || live->injury_object == 0u) {
        return 0;
    }
    return rec->expected_end_yyyymmdd != 0u
        && kbo_foreign_injury_expected_end_pending(today_yyyymmdd, rec->expected_end_yyyymmdd)
        && kbo_foreign_injury_live_memory_matches_record_episode(rec, live);
}

int kbo_foreign_injury_closed_record_can_repair_on_date(
    const KboForeignInjuryReplacement* rec,
    const KboForeignInjuryLiveMemory* live,
    uint32_t today_yyyymmdd,
    int inactive_roster_present,
    int roster_hold_flags_present)
{
    if (rec == NULL
            || rec->status != KBO_FOREIGN_INJURY_STATUS_CLOSED
            || rec->converted != 0u
            || rec->replacement_player_id == 0u
            || rec->expected_end_yyyymmdd == 0u
            || rec->close_choice == KBO_FOREIGN_INJURY_CLOSE_OFFSEASON_RESET) {
        return 0;
    }

    if (kbo_foreign_injury_live_memory_has_record_continuation_basis(rec, live, today_yyyymmdd)) {
        return 1;
    }

    if (!kbo_foreign_injury_expected_end_pending(today_yyyymmdd, rec->expected_end_yyyymmdd)) {
        return 0;
    }
    return inactive_roster_present || roster_hold_flags_present;
}

static int kbo_foreign_injury_replacement_slot_matches_record(
    const KboForeignInjuryReplacement* rec,
    uint8_t* replacement)
{
    if (rec == NULL || replacement == NULL || !memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    uint8_t replacement_slot = kbo_player_is_asian_quota_candidate(replacement)
        ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
        : KBO_FOREIGN_INJURY_SLOT_REGULAR;
    return replacement_slot == rec->slot_type
        || (replacement_slot == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
            && rec->slot_type == KBO_FOREIGN_INJURY_SLOT_REGULAR);
}

int kbo_foreign_injury_replacement_player_attached_to_record(
    const KboForeignInjuryReplacement* rec,
    uint8_t* replacement)
{
    if (rec == NULL
            || rec->team_id == 0u
            || replacement == NULL
            || !memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(replacement)
            || !kbo_foreign_injury_replacement_slot_matches_record(rec, replacement)) {
        return 0;
    }
    return kbo_player_current_assignment_matches_team_or_affiliate(replacement, rec->team_id);
}

int kbo_foreign_injury_replacement_player_can_restore_to_record(
    const KboForeignInjuryReplacement* rec,
    uint8_t* replacement)
{
    if (rec == NULL
            || rec->team_id == 0u
            || replacement == NULL
            || !memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(replacement)
            || !kbo_foreign_injury_replacement_slot_matches_record(rec, replacement)) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(replacement + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(replacement + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (current_team_id != 0u || active_team_id != 0u) {
        return 0;
    }

    uint32_t original_team_id = 0u;
    if (memory_range_readable(replacement + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        original_team_id = *(uint32_t*)(replacement + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    }
    return original_team_id == 0u || kbo_team_ids_share_org(original_team_id, rec->team_id);
}

int kbo_foreign_injury_return_state_allows_close(
    uint8_t injury_active,
    int16_t days_left,
    uint8_t loan_active,
    int active_roster_present,
    int inactive_roster_present,
    int roster_hold_flags_present,
    int close_decision_allowed)
{
    if (!close_decision_allowed) {
        return 0;
    }
    return injury_active == 0u
        && days_left <= 0
        && loan_active == 0u
        && active_roster_present
        && !inactive_roster_present
        && !roster_hold_flags_present;
}

int kbo_foreign_injury_open_news_allowed(
    uint32_t scan_date,
    uint32_t live_date,
    uint32_t opened_on,
    int process_existing_replacements,
    int captured_live_date)
{
    return process_existing_replacements
        && scan_date != 0u
        && (live_date == scan_date || captured_live_date)
        && opened_on != 0u
        && opened_on == scan_date;
}

int kbo_foreign_injury_state_record_has_minimum_injury_basis(
    const KboForeignInjuryReplacement* rec)
{
    if (rec == NULL || rec->injured_player_id == 0u) {
        return 0;
    }
    if (rec->expected_end_yyyymmdd != 0u) {
        return 1;
    }

    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, &team_id, &league_id);
    if (injured == NULL || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    KboForeignInjuryLiveMemory live_injury;
    memset(&live_injury, 0, sizeof(live_injury));
    int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
    if (kbo_foreign_injury_read_live_memory(injured, &live_injury)
            && kbo_foreign_injury_live_memory_has_long_term_basis(&live_injury, min_days)) {
        return 1;
    }
    if (live_injury.active != 0u || live_injury.days_left > 0) {
        return 0;
    }

    uint32_t today = 0u;
    kbo_current_date_tick_latest_published_date(&today);
    int inactive_roster_present = kbo_foreign_injury_player_on_inactive_replacement_roster(
        injured,
        rec->injured_player_id,
        rec->team_id,
        today);
    if (kbo_foreign_injury_active_record_has_roster_basis(
            rec->status,
            rec->replacement_player_id,
            inactive_roster_present)) {
        return 1;
    }
    return 0;
}

int kbo_foreign_injury_player_excluded_from_foreign_count_locked(uint32_t team_id, uint32_t player_id)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        const KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (kbo_team_ids_share_org(rec->team_id, team_id)
                && rec->injured_player_id == player_id
                && (rec->status == KBO_FOREIGN_INJURY_STATUS_OPEN
                    || rec->status == KBO_FOREIGN_INJURY_STATUS_ACTIVE)
                && kbo_foreign_injury_state_record_has_minimum_injury_basis(rec)) {
            return 1;
        }
    }
    return 0;
}

/* Foreign injury replacement CSV loading. Included from native/KBOFix.c. */

/* Foreign injury replacement seed import. Included from native/KBOFix.c. */

/* Foreign injury replacement CSV persistence. Included from native/KBOFix.c. */

/* Foreign injury replacement lazy-load orchestration. Included from native/KBOFix.c. */

/* Foreign injury replacement lookup and counting helpers. Included from native/KBOFix.c. */

/* Foreign injury replacement native news emission. Included from native/KBOFix.c. */

LONG g_kbo_foreign_injury_date_tick_thread_started = 0;
