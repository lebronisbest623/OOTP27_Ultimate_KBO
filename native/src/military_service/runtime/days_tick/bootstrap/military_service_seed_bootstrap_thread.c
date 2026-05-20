#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/dates/core_current_date.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../../../core/season/opening_day_storyline_guard.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/lookup/team_lookup.h"
#include "../../../calendar/military_service_date.h"
#include "../military_service_days_tick.h"
#include "../military_service_tick.h"

DWORD WINAPI kbo_military_seed_bootstrap_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO military service seed bootstrap thread started");

    char last_save_path[MAX_PATH] = {0};
    int settled_attempts = 0;
    const KboRuntimeTuningPolicy* tuning = kbo_runtime_tuning_policy();
    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "military_seed_bootstrap",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    for (int attempt = 1; attempt <= tuning->military_seed_bootstrap_attempts; attempt++) {
        uint32_t sleep_ms = attempt == 1
            ? (uint32_t)tuning->military_seed_bootstrap_first_sleep_ms
            : (uint32_t)tuning->military_seed_bootstrap_sleep_ms;
        if (!kbo_runtime_sleep_should_continue(sleep_ms)) {
            break;
        }

        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
            if (attempt <= tuning->military_seed_bootstrap_log_initial_attempts
                    || attempt % tuning->military_seed_bootstrap_log_interval == 0) {
                kbo_log_runtimef("KBO military service seed bootstrap waiting attempt=%d reason=no_save_path", attempt);
            }
            continue;
        }
        if (!kbo_runtime_pause_for_save_if_needed("military_seed_bootstrap")) {
            break;
        }

        if (_stricmp(last_save_path, save_path) != 0) {
            snprintf(last_save_path, sizeof(last_save_path), "%s", save_path);
            settled_attempts = 0;
            kbo_military_prewarm_save_scoped_bootstrap_files(save_path);
        }

        KboCurrentDateTickWork work = {0};
        if (!kbo_current_date_tick_consumer_next(&consumer, &work)) {
            if (attempt <= tuning->military_seed_bootstrap_log_initial_attempts
                    || attempt % tuning->military_seed_bootstrap_log_interval == 0) {
                kbo_log_runtimef("KBO military service seed bootstrap waiting attempt=%d reason=no_hook_date save=%s", attempt, save_path);
            }
            continue;
        }

        uint32_t today_serial = kbo_date_serial(
            work.date / 10000u,
            (work.date / 100u) % 100u,
            work.date % 100u);
        uintptr_t player_vector = 0;
        int32_t player_count = 0;
        uint32_t vector_offset = 0;
        uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
        uint8_t* kpb = find_kbo_team_by_csv_id_any_league("KPB", 0);
        if (today_serial == 0u
                || !find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)
                || (sang == NULL && kpb == NULL)) {
            if (attempt <= tuning->military_seed_bootstrap_log_initial_attempts
                    || attempt % tuning->military_seed_bootstrap_log_interval == 0) {
                kbo_log_runtimef(
                    "KBO military service seed bootstrap waiting attempt=%d reason=state_not_ready date_serial=%u player_count=%d sang=%p kpb=%p save=%s",
                    attempt,
                    today_serial,
                    player_count,
                    (void*)sang,
                    (void*)kpb,
                    save_path);
            }
            continue;
        }

        if (kbo_opening_day_storyline_guard_active("military_seed_bootstrap", NULL, NULL)) {
            settled_attempts = 0;
            continue;
        }

        int seeded = 0;
        int returned = kbo_tick_military_service_days_for_date(
            work.date,
            "military_seed_bootstrap",
            &seeded);
        if (kbo_runtime_save_in_progress()) {
            continue;
        }
        if (seeded > 0 || returned > 0) {
            kbo_current_date_tick_consumer_mark_processed(&consumer);
            kbo_log_runtimef(
                "KBO military service seed bootstrap applied attempt=%d seeded=%d returned=%d save=%s",
                attempt,
                seeded,
                returned,
                save_path);
            return 0;
        }

        if (GetFileAttributesA(save_path) != INVALID_FILE_ATTRIBUTES) {
            settled_attempts++;
        }
        if (settled_attempts >= 3) {
            kbo_current_date_tick_consumer_mark_processed(&consumer);
            kbo_log_runtimef(
                "KBO military service seed bootstrap settled attempt=%d seeded=0 returned=0 save=%s",
                attempt,
                save_path);
            return 0;
        }
    }

    kbo_log_runtime_line("KBO military service seed bootstrap ended without settled save");
    return 0;
}
