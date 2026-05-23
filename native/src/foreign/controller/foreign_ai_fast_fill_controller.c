#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "foreign_ai_fast_fill_controller.h"
#include "foreign_ai_controller.h"
#include "../common/policy/foreign_waiver_policy.h"
#include "../quota/counts/foreign_quota_counts.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/logging/core_log.h"
#include "../../core/sync/lock.h"
#include "../../core/teams/core_team_collect.h"
#include "../../team/add_player_guard/team_add_player_guard_ai_roster.h"
#include "../../team/independent_acquisition/independent_acquisition_ai.h"

enum {
    KBO_FOREIGN_FAST_FILL_TEAM_MAX = 64,
    KBO_FOREIGN_FAST_FILL_COLLECT_TEAM_MAX = 64
};

typedef struct KboForeignFastFillTeamState {
    uint32_t team_id;
    uint32_t vacancy_started_on;
    uint32_t last_seen_on;
    uint32_t asian_count;
    uint32_t non_asian_count;
    uint32_t pending_asian_count;
    uint32_t pending_non_asian_count;
    uint32_t effective_count;
    uint32_t effective_with_pending;
    uint32_t limit;
    uint8_t effective_vacant;
    uint8_t asian_quota_vacant;
    uint8_t vacant;
} KboForeignFastFillTeamState;

static KboLock g_kbo_foreign_fast_fill_lock = KBO_LOCK_INIT;
static KboForeignFastFillTeamState g_kbo_foreign_fast_fill_teams[KBO_FOREIGN_FAST_FILL_TEAM_MAX];
static uint32_t g_kbo_foreign_fast_fill_last_attempt_date = 0u;

static uint32_t kbo_fast_fill_date_serial(uint32_t yyyymmdd)
{
    return kbo_date_serial(yyyymmdd / 10000u, (yyyymmdd / 100u) % 100u, yyyymmdd % 100u);
}

static uint32_t kbo_fast_fill_vacancy_days(uint32_t started_on, uint32_t today)
{
    uint32_t started = kbo_fast_fill_date_serial(started_on);
    uint32_t current = kbo_fast_fill_date_serial(today);
    if (started == 0u || current == 0u || current < started) {
        return 0u;
    }
    return current - started;
}

static KboForeignFastFillTeamState* kbo_fast_fill_find_or_add_locked(uint32_t team_id)
{
    if (team_id == 0u) {
        return NULL;
    }
    int empty = -1;
    for (int i = 0; i < KBO_FOREIGN_FAST_FILL_TEAM_MAX; i++) {
        if (g_kbo_foreign_fast_fill_teams[i].team_id == team_id) {
            return &g_kbo_foreign_fast_fill_teams[i];
        }
        if (empty < 0 && g_kbo_foreign_fast_fill_teams[i].team_id == 0u) {
            empty = i;
        }
    }
    if (empty < 0) {
        return NULL;
    }
    memset(&g_kbo_foreign_fast_fill_teams[empty], 0, sizeof(g_kbo_foreign_fast_fill_teams[empty]));
    g_kbo_foreign_fast_fill_teams[empty].team_id = team_id;
    return &g_kbo_foreign_fast_fill_teams[empty];
}

static void kbo_fast_fill_update_team_locked(
    uint32_t team_id,
    uint32_t today,
    uint32_t asian_count,
    uint32_t non_asian_count,
    uint32_t pending_asian_count,
    uint32_t pending_non_asian_count,
    uint32_t effective_count,
    uint32_t effective_with_pending,
    uint32_t limit)
{
    KboForeignFastFillTeamState* rec = kbo_fast_fill_find_or_add_locked(team_id);
    if (rec == NULL) {
        return;
    }
    int effective_vacant = limit > 0u && effective_with_pending < limit;
    int asian_quota_vacant = limit > 0u && asian_count == 0u && pending_asian_count == 0u;
    int vacant = effective_vacant || asian_quota_vacant;
    uint8_t was_vacant = rec->vacant;
    rec->last_seen_on = today;
    rec->asian_count = asian_count;
    rec->non_asian_count = non_asian_count;
    rec->pending_asian_count = pending_asian_count;
    rec->pending_non_asian_count = pending_non_asian_count;
    rec->effective_count = effective_count;
    rec->effective_with_pending = effective_with_pending;
    rec->limit = limit;
    rec->effective_vacant = effective_vacant ? 1u : 0u;
    rec->asian_quota_vacant = asian_quota_vacant ? 1u : 0u;
    rec->vacant = vacant ? 1u : 0u;
    if (vacant && rec->vacancy_started_on == 0u) {
        rec->vacancy_started_on = today;
    } else if (!vacant) {
        rec->vacancy_started_on = 0u;
    }

    if (was_vacant != rec->vacant) {
        kbo_log_runtimef(
            "foreign ai fast fill controller vacancy team=%u vacant=%u effective_vacant=%u asian_quota_vacant=%u started=%u asian=%u non_asian=%u pending_asian=%u pending_non_asian=%u effective=%u effective_with_pending=%u limit=%u today=%u",
            team_id,
            (uint32_t)rec->vacant,
            (uint32_t)rec->effective_vacant,
            (uint32_t)rec->asian_quota_vacant,
            rec->vacancy_started_on,
            asian_count,
            non_asian_count,
            pending_asian_count,
            pending_non_asian_count,
            effective_count,
            effective_with_pending,
            limit,
            today);
    }
}

int kbo_foreign_ai_fast_fill_controller_tick(uint32_t today, const char* source)
{
    if (today == 0u
            || !kbo_custom_foreign_policy_enabled()
            || !kbo_foreign_ai_controller_enabled()) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    uint32_t team_ids[KBO_FOREIGN_FAST_FILL_COLLECT_TEAM_MAX] = {0};
    int scanned = 0;
    int unreadable = 0;
    int team_count = collect_kbo_league_team_ids(
        league_id,
        team_ids,
        KBO_FOREIGN_FAST_FILL_COLLECT_TEAM_MAX,
        &scanned,
        &unreadable);
    if (team_count <= 0) {
        return 0;
    }

    uint32_t limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    int vacancy_count = 0;
    int asian_quota_vacancy_count = 0;
    uint32_t longest_vacancy_days = 0u;
    kbo_lock_enter(&g_kbo_foreign_fast_fill_lock);
    for (int i = 0; i < team_count; i++) {
        uint32_t team_id = team_ids[i];
        uint32_t foreign_count = 0u;
        uint32_t asian_count = 0u;
        uint32_t non_asian_count = 0u;
        kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
        (void)foreign_count;

        uint32_t pending_asian = 0u;
        uint32_t pending_non_asian = 0u;
        int candidate_pending = 0;
        kbo_custom_foreign_count_pending_offers(
            team_id,
            today,
            0u,
            &pending_asian,
            &pending_non_asian,
            &candidate_pending);
        (void)candidate_pending;

        uint32_t effective = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
        uint32_t effective_with_pending = kbo_effective_foreign_count_with_asian_quota(
            asian_count + pending_asian,
            non_asian_count + pending_non_asian);
        kbo_fast_fill_update_team_locked(
            team_id,
            today,
            asian_count,
            non_asian_count,
            pending_asian,
            pending_non_asian,
            effective,
            effective_with_pending,
            limit);
        KboForeignFastFillTeamState* rec = kbo_fast_fill_find_or_add_locked(team_id);
        if (rec != NULL && rec->vacant) {
            vacancy_count++;
            if (rec->asian_quota_vacant) {
                asian_quota_vacancy_count++;
            }
            uint32_t days = kbo_fast_fill_vacancy_days(rec->vacancy_started_on, today);
            if (days > longest_vacancy_days) {
                longest_vacancy_days = days;
            }
        }
    }

    int should_run = vacancy_count > 0
        && g_kbo_foreign_fast_fill_last_attempt_date != today;
    if (should_run) {
        g_kbo_foreign_fast_fill_last_attempt_date = today;
    }
    kbo_lock_leave(&g_kbo_foreign_fast_fill_lock);

    if (!should_run) {
        return 0;
    }

    kbo_mark_foreign_ai_roster_daily_callup_dirty("foreign_fast_fill_controller");
    int callup_result = kbo_consume_foreign_ai_roster_daily_callup_dirty(
        source != NULL ? source : "foreign_fast_fill_controller");
    int acquisition_result = kbo_run_independent_team_acquisition_ai_for_date(
        today,
        source != NULL ? source : "foreign_fast_fill_controller");
    kbo_log_runtimef(
        "foreign ai fast fill controller tick source=%s today=%u teams=%d vacancies=%d asian_quota_vacancies=%d longest_days=%u callup=%d acquisition=%d scanned=%d unreadable=%d",
        source != NULL ? source : "",
        today,
        team_count,
        vacancy_count,
        asian_quota_vacancy_count,
        longest_vacancy_days,
        callup_result,
        acquisition_result,
        scanned,
        unreadable);
    return callup_result + acquisition_result;
}
