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

int kbo_foreign_injury_inactive_roster_has_long_term_injury_basis(
    uint8_t injury_active,
    int16_t days_left,
    int min_days,
    int inactive_roster_present)
{
    if (!inactive_roster_present || injury_active == 0u) {
        return 0;
    }
    return days_left >= min_days;
}

int kbo_foreign_injury_duration_meets_minimum(int16_t days_left, int min_days)
{
    return min_days > 0 && days_left >= min_days;
}

static int kbo_foreign_injury_ascii_lower(int ch)
{
    return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch;
}

static int kbo_foreign_injury_ascii_starts_with_nocase(const char* text, const char* prefix)
{
    if (text == NULL || prefix == NULL) {
        return 0;
    }
    while (*prefix != '\0') {
        if (kbo_foreign_injury_ascii_lower((unsigned char)*text)
                != kbo_foreign_injury_ascii_lower((unsigned char)*prefix)) {
            return 0;
        }
        ++text;
        ++prefix;
    }
    return 1;
}

static int kbo_foreign_injury_ascii_contains_nocase(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    for (const char* p = text; *p != '\0'; ++p) {
        if (kbo_foreign_injury_ascii_starts_with_nocase(p, needle)) {
            return 1;
        }
    }
    return 0;
}

static const char* kbo_foreign_injury_ascii_match_unit(const char* text, const char* unit)
{
    if (text == NULL || unit == NULL) {
        return NULL;
    }
    while (*unit != '\0') {
        if (kbo_foreign_injury_ascii_lower((unsigned char)*text)
                != kbo_foreign_injury_ascii_lower((unsigned char)*unit)) {
            return NULL;
        }
        ++text;
        ++unit;
    }
    return text;
}

static int kbo_foreign_injury_ascii_unit_followed_by_old(const char* unit_end)
{
    if (unit_end == NULL) {
        return 0;
    }
    const char* p = unit_end;
    if (*p == 's' || *p == 'S') {
        ++p;
    }
    while (*p == ' ' || *p == '\t' || *p == '-' || *p == '\r' || *p == '\n') {
        ++p;
    }
    return kbo_foreign_injury_ascii_starts_with_nocase(p, "old");
}

int kbo_foreign_injury_duration_text_meets_minimum(
    const char* text,
    int min_days,
    int* out_days)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    if (text == NULL || min_days <= 0) {
        return 0;
    }

    int best_days = 0;
    if (kbo_foreign_injury_ascii_contains_nocase(text, "season is officially over")
            || kbo_foreign_injury_ascii_contains_nocase(text, "season is over")
            || kbo_foreign_injury_ascii_contains_nocase(text, "out for the season")
            || kbo_foreign_injury_ascii_contains_nocase(text, "miss the rest of the season")
            || kbo_foreign_injury_ascii_contains_nocase(text, "misses the rest of the season")
            || kbo_foreign_injury_ascii_contains_nocase(text, "season-ending")) {
        best_days = min_days > 180 ? min_days : 180;
    }
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            continue;
        }

        int value = 0;
        const char* n = p;
        while (*n >= '0' && *n <= '9') {
            value = value * 10 + (*n - '0');
            ++n;
            if (value > 10000) {
                value = 10000;
                break;
            }
        }
        p = n - 1;
        while (*n == ' ' || *n == '\t' || *n == '\r' || *n == '\n') {
            ++n;
        }

        int days = 0;
        const char* unit_end = NULL;
        if ((unit_end = kbo_foreign_injury_ascii_match_unit(n, "day")) != NULL) {
            days = value;
        } else if ((unit_end = kbo_foreign_injury_ascii_match_unit(n, "week")) != NULL) {
            days = value * 7;
        } else if ((unit_end = kbo_foreign_injury_ascii_match_unit(n, "month")) != NULL) {
            days = value * 30;
        } else if ((unit_end = kbo_foreign_injury_ascii_match_unit(n, "year")) != NULL) {
            days = value * 365;
        }

        if (days > 0 && kbo_foreign_injury_ascii_unit_followed_by_old(unit_end)) {
            days = 0;
        }
        if (days > best_days) {
            best_days = days;
        }
    }

    if (out_days != NULL) {
        *out_days = best_days;
    }
    return best_days >= min_days;
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

static int kbo_foreign_injury_state_record_has_minimum_injury_basis(
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

    uint8_t injury_active = injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    int16_t days_left = *(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
    int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
    if (kbo_foreign_injury_duration_meets_minimum(days_left, min_days)) {
        return 1;
    }
    if (days_left > 0) {
        return 0;
    }

    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);
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
    return kbo_foreign_injury_inactive_roster_has_long_term_injury_basis(
        injury_active,
        days_left,
        min_days,
        inactive_roster_present);
}

int kbo_foreign_injury_player_excluded_from_foreign_count_locked(uint32_t team_id, uint32_t player_id)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        const KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->team_id == team_id
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

LONG g_kbo_foreign_injury_replacement_thread_started = 0;

