#include "../../runtime/common/custom_events_common.h"
#include "query_helpers.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../import/import_and_load.h"
#include "../projected/policy/projected_policy.h"
#include "../../../core/dates/constants/kbo_date_constants.h"

int kbo_get_asian_games_schedule_for_year(uint32_t year, KboAsianGamesScheduleSeed* out)
{
    if (year < KBO_SEASON_YEAR_MIN || year > KBO_SIM_YEAR_MAX) {
        if (!kbo_asian_games_year_is_projected(year)) {
            return 0;
        }
    }
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }

    kbo_ensure_asian_games_schedule_seeds_loaded();
    kbo_lock_asian_games_schedule_seeds();
    for (int i = 0; i < g_kbo_asian_games_schedule_seed_count; i++) {
        if (g_kbo_asian_games_schedule_seeds[i].year == year) {
            if (out != NULL) {
                *out = g_kbo_asian_games_schedule_seeds[i];
            }
            kbo_unlock_asian_games_schedule_seeds();
            return 1;
        }
    }
    kbo_unlock_asian_games_schedule_seeds();
    if (kbo_asian_games_year_is_projected(year)) {
        if (out != NULL) {
            kbo_build_projected_asian_games_schedule(year, out);
        }
        return 1;
    }
    return 0;
}

int kbo_get_next_asian_games_schedule(uint32_t from_year, KboAsianGamesScheduleSeed* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (from_year < KBO_SEASON_YEAR_MIN) {
        from_year = KBO_SEASON_YEAR_MIN;
    }
    KboAsianGamesProjectedPolicy policy;
    kbo_load_asian_games_projected_policy(&policy);
    if (from_year > policy.projected_end_year) {
        return 0;
    }

    kbo_ensure_asian_games_schedule_seeds_loaded();
    kbo_lock_asian_games_schedule_seeds();
    int best_index = -1;
    uint32_t best_year = 0u;
    for (int i = 0; i < g_kbo_asian_games_schedule_seed_count; i++) {
        uint32_t year = g_kbo_asian_games_schedule_seeds[i].year;
        if (year < from_year) {
            continue;
        }
        if (best_index < 0 || year < best_year) {
            best_index = i;
            best_year = year;
        }
    }
    if (best_index >= 0) {
        uint32_t projected_year = kbo_next_projected_asian_games_year(from_year);
        if (projected_year != 0u && projected_year < best_year) {
            if (out != NULL) {
                kbo_build_projected_asian_games_schedule(projected_year, out);
            }
            kbo_unlock_asian_games_schedule_seeds();
            return 1;
        }
        if (out != NULL) {
            *out = g_kbo_asian_games_schedule_seeds[best_index];
        }
        kbo_unlock_asian_games_schedule_seeds();
        return 1;
    }
    kbo_unlock_asian_games_schedule_seeds();
    uint32_t projected_year = kbo_next_projected_asian_games_year(from_year);
    if (projected_year != 0u) {
        if (out != NULL) {
            kbo_build_projected_asian_games_schedule(projected_year, out);
        }
        return 1;
    }
    return 0;
}

int kbo_get_asian_games_schedule_seed_list(KboAsianGamesScheduleSeed* out, int max_count)
{
    if (out == NULL || max_count <= 0) {
        return 0;
    }
    memset(out, 0, sizeof(*out) * (size_t)max_count);

    kbo_ensure_asian_games_schedule_seeds_loaded();
    kbo_lock_asian_games_schedule_seeds();
    int count = 0;
    for (int i = 0; i < g_kbo_asian_games_schedule_seed_count && count < max_count; i++) {
        out[count++] = g_kbo_asian_games_schedule_seeds[i];
    }
    kbo_unlock_asian_games_schedule_seeds();

    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (out[j].year < out[i].year) {
                KboAsianGamesScheduleSeed tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
    return count;
}

int kbo_asian_games_schedule_has_event_dates(const KboAsianGamesScheduleSeed* schedule)
{
    return schedule != NULL
        && schedule->selection_date != 0u
        && schedule->departure_date != 0u
        && schedule->final_date != 0u;
}

int kbo_asian_games_schedule_auto_events_enabled(const KboAsianGamesScheduleSeed* schedule)
{
    return schedule != NULL
        && schedule->auto_schedule != 0u
        && kbo_asian_games_schedule_has_event_dates(schedule);
}

const char* kbo_asian_games_schedule_status_label(const KboAsianGamesScheduleSeed* schedule)
{
    if (schedule == NULL || schedule->status[0] == '\0') {
        return "Seeded";
    }
    if (ascii_equals_ignore_case(schedule->status, "official")) {
        return "Official";
    }
    if (ascii_equals_ignore_case(schedule->status, "confirmed")) {
        return "Confirmed";
    }
    if (ascii_equals_ignore_case(schedule->status, "provisional")) {
        return "Provisional";
    }
    if (ascii_equals_ignore_case(schedule->status, "projected")) {
        return "Projected";
    }
    return schedule->status;
}
