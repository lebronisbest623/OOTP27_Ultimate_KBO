#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_team_acquisition_schedule.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/events/core_league_events.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/classification/team_classification.h"
#include "../../runtime/catalog/custom_event_catalog.h"
#include "../../runtime/ledger/custom_event_ledger.h"
#include "../../runtime/lookup/custom_event_lookup.h"
#include "../../runtime/markers/custom_event_markers.h"
#include "../../runtime/names/custom_event_names.h"
#include "../../runtime/runner/custom_event_runner.h"
#include "../../runtime/state/custom_event_state.h"
#include "../../../team/independent_acquisition/window/independent_acquisition_window.h"

static uint32_t g_kbo_independent_acquisition_schedule_ready_year = 0u;
static uint32_t g_kbo_independent_acquisition_schedule_ready_event_league_id = 0u;
static uint32_t g_kbo_independent_acquisition_schedule_ready_first_open_date = 0u;

#define KBO_INDEPENDENT_ACQUISITION_LEAGUE_ID_OFFSET_COUNT 4u
#define KBO_INDEPENDENT_ACQUISITION_LEAGUE_SCAN_MAX_REGION ((SIZE_T)0x00400000u)

typedef struct KboIndependentAcquisitionMemoryStartCache {
    uint32_t league_id;
    uint32_t season;
    uint32_t start_date;
    uintptr_t league_ptr;
    char save_path[MAX_PATH];
} KboIndependentAcquisitionMemoryStartCache;

static KboIndependentAcquisitionMemoryStartCache g_kbo_independent_acquisition_memory_start_cache = {0};

static int kbo_independent_team_acquisition_memory_executable(const void* address)
{
    if (address == NULL) {
        return 0;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) {
        return 0;
    }

    DWORD protect = mbi.Protect & 0xffu;
    return protect == PAGE_EXECUTE
        || protect == PAGE_EXECUTE_READ
        || protect == PAGE_EXECUTE_READWRITE
        || protect == PAGE_EXECUTE_WRITECOPY;
}

static int kbo_independent_team_acquisition_date_slot_looks_like_serializer_callback(
    uintptr_t league_ptr,
    uintptr_t* out_slot,
    uintptr_t* out_callback)
{
    if (out_slot != NULL) {
        *out_slot = 0u;
    }
    if (out_callback != NULL) {
        *out_callback = 0u;
    }
    if (league_ptr == 0u
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET),
                sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t slot = *(uintptr_t*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
    if (out_slot != NULL) {
        *out_slot = slot;
    }
    if (slot < 0x10000u || !memory_range_readable((void*)(slot + 0x10u), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t callback = *(uintptr_t*)(slot + 0x10u);
    if (out_callback != NULL) {
        *out_callback = callback;
    }
    return kbo_independent_team_acquisition_memory_executable((void*)callback);
}

static uint32_t kbo_independent_team_acquisition_add_months(
    uint32_t yyyymmdd,
    uint32_t months)
{
    uint32_t result = yyyymmdd;
    for (uint32_t i = 0u; i < months; i++) {
        result = kbo_add_one_month_yyyymmdd(result);
        if (result == 0u) {
            return 0u;
        }
    }
    return result;
}

static int kbo_independent_team_acquisition_start_date_plausible(
    uint32_t yyyymmdd,
    uint32_t expected_year)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year != expected_year
            || month < 2u
            || month > 5u
            || day < 1u
            || day > 31u
            || kbo_date_serial(year, month, day) == 0u) {
        return 0;
    }
    return 1;
}

static int kbo_independent_team_acquisition_read_start_date_from_ptr(
    uintptr_t league_ptr,
    uint32_t expected_year,
    uint32_t* out_start_date)
{
    if (out_start_date != NULL) {
        *out_start_date = 0u;
    }
    if (league_ptr == 0u
            || out_start_date == NULL
            || expected_year < 1982u
            || expected_year > 2200u
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET),
                OOTP27_SEASON_START_DATE_SEC_OFFSET - OOTP27_SEASON_START_DATE_YEAR_OFFSET + sizeof(uint8_t))) {
        return 0;
    }
    if (kbo_independent_team_acquisition_date_slot_looks_like_serializer_callback(league_ptr, NULL, NULL)) {
        return 0;
    }

    uint32_t year = *(uint16_t*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
    uint32_t day = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_DAY_OFFSET);
    uint32_t month = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_MONTH_OFFSET);
    if (year < 1982u || year > 2200u
            || year != expected_year
            || month < 1u || month > 12u
            || day < 1u || day > 31u
            || kbo_date_serial(year, month, day) == 0u) {
        return 0;
    }

    uint32_t start_date = year * 10000u + month * 100u + day;
    if (!kbo_independent_team_acquisition_start_date_plausible(start_date, expected_year)) {
        return 0;
    }

    *out_start_date = start_date;
    return 1;
}

static int kbo_independent_team_acquisition_candidate_matches_league_id(
    uintptr_t candidate,
    uint32_t league_id)
{
    static const uint32_t id_offsets[KBO_INDEPENDENT_ACQUISITION_LEAGUE_ID_OFFSET_COUNT] = {
        OOTP27_KBO_LEAGUE_ID_OFFSET,
        OOTP27_SEASON_PHASE_ALT_ID_A_OFFSET,
        OOTP27_SEASON_PHASE_ALT_ID_B_OFFSET,
        OOTP27_SEASON_PHASE_ALT_ID_C_OFFSET
    };
    if (candidate == 0u || league_id == 0u
            || !memory_range_readable((void*)candidate, OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)) {
        return 0;
    }
    for (uint32_t i = 0u; i < KBO_INDEPENDENT_ACQUISITION_LEAGUE_ID_OFFSET_COUNT; i++) {
        if (*(uint32_t*)(candidate + id_offsets[i]) == league_id) {
            return 1;
        }
    }
    return 0;
}

static int kbo_independent_team_acquisition_candidate_score(
    uintptr_t candidate,
    uint32_t league_id,
    uint32_t expected_year,
    uint32_t* out_start_date)
{
    uint32_t start_date = 0u;
    if (!kbo_independent_team_acquisition_candidate_matches_league_id(candidate, league_id)
            || !kbo_independent_team_acquisition_read_start_date_from_ptr(
                candidate,
                expected_year,
                &start_date)) {
        return 0;
    }

    int score = 100;
    if (memory_range_readable((void*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        uint32_t league_year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        if (league_year == expected_year) {
            score += 30;
        } else if (league_year + 1u == expected_year || league_year == expected_year + 1u) {
            score += 10;
        } else if (league_year >= 1982u && league_year <= 2200u) {
            score -= 20;
        }
    }

    if (out_start_date != NULL) {
        *out_start_date = start_date;
    }
    return score;
}

static int kbo_independent_team_acquisition_store_memory_start_cache(
    uint32_t league_id,
    uint32_t season,
    uint32_t start_date,
    uintptr_t league_ptr)
{
    char save_path[MAX_PATH] = {0};
    if (league_id == 0u || season < 1982u || season > 2200u
            || start_date / 10000u != season
            || league_ptr == 0u
            || !kbo_get_current_save_path(save_path, sizeof(save_path))
            || save_path[0] == '\0') {
        return 0;
    }
    g_kbo_independent_acquisition_memory_start_cache.league_id = league_id;
    g_kbo_independent_acquisition_memory_start_cache.season = season;
    g_kbo_independent_acquisition_memory_start_cache.start_date = start_date;
    g_kbo_independent_acquisition_memory_start_cache.league_ptr = league_ptr;
    snprintf(
        g_kbo_independent_acquisition_memory_start_cache.save_path,
        sizeof(g_kbo_independent_acquisition_memory_start_cache.save_path),
        "%s",
        save_path);
    return 1;
}

static int kbo_independent_team_acquisition_try_cached_memory_start(
    uint32_t league_id,
    uint32_t expected_year,
    uint32_t* out_start_date)
{
    KboIndependentAcquisitionMemoryStartCache cache = g_kbo_independent_acquisition_memory_start_cache;
    char save_path[MAX_PATH] = {0};
    if (cache.league_id != league_id
            || cache.season != expected_year
            || cache.start_date / 10000u != expected_year
            || cache.league_ptr == 0u
            || cache.save_path[0] == '\0'
            || !kbo_get_current_save_path(save_path, sizeof(save_path))
            || strcmp(cache.save_path, save_path) != 0) {
        return 0;
    }

    uint32_t current_start = 0u;
    if (!kbo_independent_team_acquisition_read_start_date_from_ptr(
            cache.league_ptr,
            expected_year,
            &current_start)) {
        if (out_start_date != NULL) {
            *out_start_date = cache.start_date;
        }
        return 1;
    }
    if (current_start != cache.start_date) {
        static uint32_t last_drift_logged_year = 0u;
        if (last_drift_logged_year != expected_year) {
            last_drift_logged_year = expected_year;
            kbo_log_runtimef(
                "KBO independent futures acquisition memory start drift ignored league_id=%u season=%u cached=%u current=%u ptr=%p",
                league_id,
                expected_year,
                cache.start_date,
                current_start,
                (void*)cache.league_ptr);
        }
        if (out_start_date != NULL) {
            *out_start_date = cache.start_date;
        }
        return 1;
    }

    if (out_start_date != NULL) {
        *out_start_date = cache.start_date;
    }
    return 1;
}

static int kbo_independent_team_acquisition_try_global_vector_memory_start(
    uint32_t league_id,
    uint32_t expected_year,
    uint32_t* out_start_date,
    uintptr_t* out_league_ptr)
{
    if (out_start_date != NULL) {
        *out_start_date = 0u;
    }
    if (out_league_ptr != NULL) {
        *out_league_ptr = 0u;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0u || league_id == 0u) {
        return 0;
    }

    static const uint32_t league_vec_offsets[] = {
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0xc0u, 0xc8u, 0xd0u, 0xd8u,
        0xe0u, 0xe8u, 0xf0u, 0xf8u, 0x100u, 0x108u, 0x110u, 0x118u, 0x120u, 0x128u
    };

    int best_score = 0;
    uintptr_t best_ptr = 0u;
    uint32_t best_start = 0u;
    for (size_t i = 0u; i < sizeof(league_vec_offsets) / sizeof(league_vec_offsets[0]); i++) {
        uint32_t vec_off = league_vec_offsets[i];
        uint32_t cnt_off = vec_off + 8u;
        if (!memory_range_readable((void*)(global + vec_off), 16u)) {
            continue;
        }

        uintptr_t candidate_vec = *(uintptr_t*)(global + vec_off);
        int32_t candidate_count = *(int32_t*)(global + cnt_off);
        if (candidate_vec == 0u || candidate_count <= 0 || candidate_count > 10000
                || !memory_range_readable((void*)candidate_vec, (SIZE_T)candidate_count * sizeof(uintptr_t))) {
            continue;
        }

        for (int32_t j = 0; j < candidate_count; j++) {
            uintptr_t candidate = *(uintptr_t*)(candidate_vec + (uintptr_t)j * sizeof(uintptr_t));
            uint32_t start_date = 0u;
            int score = kbo_independent_team_acquisition_candidate_score(
                candidate,
                league_id,
                expected_year,
                &start_date);
            if (score > best_score) {
                best_score = score;
                best_ptr = candidate;
                best_start = start_date;
            }
        }
    }

    if (best_score <= 0 || best_ptr == 0u || best_start == 0u) {
        return 0;
    }
    if (out_start_date != NULL) {
        *out_start_date = best_start;
    }
    if (out_league_ptr != NULL) {
        *out_league_ptr = best_ptr;
    }
    return 1;
}

static int kbo_independent_team_acquisition_scan_memory_start_by_league_id(
    uint32_t league_id,
    uint32_t expected_year,
    uint32_t* out_start_date,
    uintptr_t* out_league_ptr)
{
    static const uint32_t id_offsets[KBO_INDEPENDENT_ACQUISITION_LEAGUE_ID_OFFSET_COUNT] = {
        OOTP27_KBO_LEAGUE_ID_OFFSET,
        OOTP27_SEASON_PHASE_ALT_ID_A_OFFSET,
        OOTP27_SEASON_PHASE_ALT_ID_B_OFFSET,
        OOTP27_SEASON_PHASE_ALT_ID_C_OFFSET
    };
    if (out_start_date != NULL) {
        *out_start_date = 0u;
    }
    if (out_league_ptr != NULL) {
        *out_league_ptr = 0u;
    }
    if (league_id == 0u || expected_year < 1982u || expected_year > 2200u) {
        return 0;
    }

    int best_score = 0;
    uintptr_t best_ptr = 0u;
    uint32_t best_start = 0u;
    uintptr_t address = 0x10000u;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)address, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= base) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && mbi.Type == MEM_PRIVATE
                && kbo_league_scan_protect_allows_read(mbi.Protect)
                && mbi.RegionSize >= (SIZE_T)(OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)
                && mbi.RegionSize <= KBO_INDEPENDENT_ACQUISITION_LEAGUE_SCAN_MAX_REGION) {
            uintptr_t scan_end = end >= sizeof(uint32_t) ? end - sizeof(uint32_t) : base;
            for (uintptr_t p = base; p <= scan_end; p += sizeof(uint32_t)) {
                if (*(uint32_t*)p != league_id) {
                    continue;
                }

                for (uint32_t i = 0u; i < KBO_INDEPENDENT_ACQUISITION_LEAGUE_ID_OFFSET_COUNT; i++) {
                    uint32_t id_offset = id_offsets[i];
                    if (p < base + id_offset) {
                        continue;
                    }
                    uintptr_t candidate = p - id_offset;
                    if ((candidate & 7u) != 0u
                            || candidate < base
                            || candidate + OOTP27_KBO_LEAGUE_ID_OFFSET + 16u > end) {
                        continue;
                    }

                    uint32_t start_date = 0u;
                    int score = kbo_independent_team_acquisition_candidate_score(
                        candidate,
                        league_id,
                        expected_year,
                        &start_date);
                    if (score > best_score) {
                        best_score = score;
                        best_ptr = candidate;
                        best_start = start_date;
                    }
                }
            }
        }

        address = end;
#if UINTPTR_MAX > 0xffffffffu
        if (address >= (uintptr_t)0x0000800000000000ull) {
            break;
        }
#endif
    }

    if (best_score <= 0 || best_ptr == 0u || best_start == 0u) {
        return 0;
    }
    if (out_start_date != NULL) {
        *out_start_date = best_start;
    }
    if (out_league_ptr != NULL) {
        *out_league_ptr = best_ptr;
    }
    return 1;
}

static int kbo_independent_team_acquisition_read_futures_league_start_date_from_memory(
    uint32_t league_id,
    uint32_t expected_year,
    uint32_t* out_start_date,
    const char** out_start_source)
{
    if (out_start_date != NULL) {
        *out_start_date = 0u;
    }
    if (out_start_source != NULL) {
        *out_start_source = "";
    }
    if (league_id == 0u || out_start_date == NULL
            || expected_year < 1982u || expected_year > 2200u) {
        return 0;
    }

    uint32_t start_date = 0u;
    uintptr_t league_ptr = 0u;
    if (kbo_independent_team_acquisition_try_cached_memory_start(
            league_id,
            expected_year,
            &start_date)) {
        *out_start_date = start_date;
        if (out_start_source != NULL) {
            *out_start_source = "futures_memory_cached";
        }
        return 1;
    }

    league_ptr = kbo_find_league_ptr_from_global_vectors(league_id);
    if (kbo_independent_team_acquisition_read_start_date_from_ptr(
            league_ptr,
            expected_year,
            &start_date)) {
        kbo_independent_team_acquisition_store_memory_start_cache(
            league_id,
            expected_year,
            start_date,
            league_ptr);
        *out_start_date = start_date;
        if (out_start_source != NULL) {
            *out_start_source = "futures_memory_global";
        }
        return 1;
    }

    league_ptr = kbo_find_league_ptr_by_memory_scan(league_id);
    if (kbo_independent_team_acquisition_read_start_date_from_ptr(
            league_ptr,
            expected_year,
            &start_date)) {
        kbo_independent_team_acquisition_store_memory_start_cache(
            league_id,
            expected_year,
            start_date,
            league_ptr);
        *out_start_date = start_date;
        if (out_start_source != NULL) {
            *out_start_source = "futures_memory_generic_scan";
        }
        return 1;
    }

    if (kbo_independent_team_acquisition_try_global_vector_memory_start(
            league_id,
            expected_year,
            &start_date,
            &league_ptr)) {
        kbo_independent_team_acquisition_store_memory_start_cache(
            league_id,
            expected_year,
            start_date,
            league_ptr);
        *out_start_date = start_date;
        if (out_start_source != NULL) {
            *out_start_source = "futures_memory_vector_scan";
        }
        kbo_log_runtimef(
            "KBO independent futures acquisition memory start resolved league_id=%u season=%u start=%u ptr=%p source=futures_memory_vector_scan",
            league_id,
            expected_year,
            start_date,
            (void*)league_ptr);
        return 1;
    }

    if (kbo_independent_team_acquisition_scan_memory_start_by_league_id(
            league_id,
            expected_year,
            &start_date,
            &league_ptr)) {
        kbo_independent_team_acquisition_store_memory_start_cache(
            league_id,
            expected_year,
            start_date,
            league_ptr);
        *out_start_date = start_date;
        if (out_start_source != NULL) {
            *out_start_source = "futures_memory_id_scan";
        }
        kbo_log_runtimef(
            "KBO independent futures acquisition memory start resolved league_id=%u season=%u start=%u ptr=%p source=futures_memory_id_scan",
            league_id,
            expected_year,
            start_date,
            (void*)league_ptr);
        return 1;
    }

    return 0;
}

static int kbo_independent_team_acquisition_resolve_start_date(
    uint32_t anchor_league_id,
    uint32_t event_league_id,
    uint32_t today,
    uint32_t* out_start_date,
    const char** out_start_source)
{
    if (out_start_date != NULL) {
        *out_start_date = 0u;
    }
    if (out_start_source != NULL) {
        *out_start_source = "";
    }
    if (out_start_date == NULL) {
        return 0;
    }

    uint32_t expected_year = today / 10000u;
    uint32_t start_date = 0u;
    (void)event_league_id;

    if (kbo_independent_team_acquisition_read_futures_league_start_date_from_memory(
            anchor_league_id,
            expected_year,
            &start_date,
            out_start_source)) {
        *out_start_date = start_date;
        return 1;
    }
    return 0;
}

static int kbo_process_due_independent_team_acquisition_open_event(
    uint32_t today,
    uint32_t league_id,
    uint32_t open_date,
    const char* title,
    const char* source)
{
    if (today == 0u || open_date == 0u || today < open_date) {
        return 0;
    }
    int completed = kbo_custom_event_processed_marker_exists_for_kind(
            open_date,
            KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN)
        || kbo_custom_event_ledger_completed(
            league_id,
            open_date,
            KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN);
    if (completed
            && kbo_independent_team_acquisition_completion_valid(league_id, open_date)) {
        return 0;
    }
    if (completed) {
        kbo_log_runtimef(
            "KBO independent futures acquisition stale completion ignored source=%s event_date=%u today=%u",
            source != NULL ? source : "",
            open_date,
            today);
    }

    int result = kbo_run_custom_event_by_kind(
        0,
        league_id,
        open_date,
        KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN,
        title,
        source);
    if (result > 0) {
        if (result == KBO_CUSTOM_EVENT_RUN_ALREADY_COMPLETED) {
            return 0;
        }
        kbo_log_runtimef(
            "KBO independent futures acquisition due event handled source=%s event_date=%u today=%u result=%d",
            source != NULL ? source : "",
            open_date,
            today,
            result);
        return 1;
    }

    kbo_log_runtimef(
        "KBO independent futures acquisition due event deferred source=%s event_date=%u today=%u result=%d",
        source != NULL ? source : "",
        open_date,
        today,
        result);
    return -1;
}

int kbo_schedule_independent_team_acquisition_custom_events_for_date(
    uint32_t today,
    const char* source)
{
    if (today == 0u) {
        kbo_log_runtimef(
            "KBO independent futures acquisition schedule skipped source=%s reason=current_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }

    KboIndependentFuturesTeamLeague leagues[16];
    int seed_rows = 0;
    int unresolved_rows = 0;
    int league_count = kbo_collect_independent_futures_team_leagues(
        leagues,
        (int)(sizeof(leagues) / sizeof(leagues[0])),
        &seed_rows,
        &unresolved_rows);
    if (seed_rows <= 0) {
        if (!kbo_team_classification_seed_source_available()) {
            static uint32_t last_logged_seed_missing_date = 0u;
            if (last_logged_seed_missing_date != today) {
                last_logged_seed_missing_date = today;
                kbo_log_runtimef(
                    "KBO independent futures acquisition schedule skipped source=%s reason=team_classification_seed_unavailable today=%u",
                    source != NULL ? source : "",
                    today);
            }
            return 0;
        }
        return 0;
    }
    if (league_count <= 0) {
        static uint32_t last_logged_unresolved_date = 0u;
        if (last_logged_unresolved_date != today) {
            last_logged_unresolved_date = today;
            kbo_log_runtimef(
                "KBO independent futures acquisition schedule skipped source=%s reason=seeded_team_unresolved today=%u seed_rows=%d unresolved=%d",
                source != NULL ? source : "",
                today,
                seed_rows,
                unresolved_rows);
        }
        return 0;
    }

    uint32_t event_league_id = kbo_get_foreign_waiver_league_id();
    if (event_league_id == 0u) {
        event_league_id = kbo_resolve_kbo_league_id();
    }
    if (event_league_id == 0u) {
        kbo_log_runtimef(
            "KBO independent futures acquisition schedule skipped source=%s reason=event_league_id_unavailable today=%u seed_rows=%d leagues=%d",
            source != NULL ? source : "",
            today,
            seed_rows,
            league_count);
        return -1;
    }
    uint32_t schedule_year = today / 10000u;
    uint32_t existing_open_date = kbo_independent_team_acquisition_window_open_date_for_date(today);
    if (existing_open_date != 0u && today >= existing_open_date) {
        static uint32_t last_logged_existing_window_date = 0u;
        if (last_logged_existing_window_date != today) {
            last_logged_existing_window_date = today;
            kbo_log_runtimef(
                "KBO independent futures acquisition schedule skipped source=%s reason=window_already_open today=%u open=%u",
                source != NULL ? source : "",
                today,
                existing_open_date);
        }
        return 0;
    }
    if (g_kbo_independent_acquisition_schedule_ready_year == schedule_year
            && g_kbo_independent_acquisition_schedule_ready_event_league_id == event_league_id
            && g_kbo_independent_acquisition_schedule_ready_first_open_date != 0u
            && today < g_kbo_independent_acquisition_schedule_ready_first_open_date) {
        return 0;
    }

    char title[160] = {0};
    if (!kbo_custom_event_title_for_kind(
            KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN,
            title,
            sizeof(title))) {
        kbo_log_runtimef(
            "KBO independent futures acquisition schedule skipped source=%s reason=title_unavailable today=%u",
            source != NULL ? source : "",
            today);
        return -1;
    }

    const KboCustomEventSchedulePolicy* policy = kbo_custom_event_schedule_policy();
    uint32_t offset_months = policy->independent_team_acquisition_open_offset_months >= 0
        ? (uint32_t)policy->independent_team_acquisition_open_offset_months
        : 2u;

    int created = 0;
    int direct_processed = 0;
    int direct_deferred = 0;
    int ready = 0;
    int failed = 0;
    int hard_failed = 0;
    uint32_t first_open_date = 0u;
    for (int i = 0; i < league_count; i++) {
        uint32_t anchor_league_id = leagues[i].league_id;
        uint32_t season_start = 0u;
        const char* start_source = "";
        if (!kbo_independent_team_acquisition_resolve_start_date(
                anchor_league_id,
                event_league_id,
                today,
                &season_start,
                &start_source)) {
            failed++;
            kbo_log_runtimef(
                "KBO independent futures acquisition schedule waiting source=%s reason=season_start_unavailable today=%u event_league_id=%u anchor_league_id=%u team=%u csv=%s",
                source != NULL ? source : "",
                today,
                event_league_id,
                anchor_league_id,
                leagues[i].team_id,
                leagues[i].team_csv_id);
            continue;
        }

        uint32_t open_date = kbo_independent_team_acquisition_add_months(season_start, offset_months);
        if (open_date == 0u) {
            failed++;
            kbo_log_runtimef(
                "KBO independent futures acquisition schedule skipped source=%s reason=derived_date_invalid today=%u event_league_id=%u anchor_league_id=%u start=%u offset_months=%u",
                source != NULL ? source : "",
                today,
                event_league_id,
                anchor_league_id,
                season_start,
                offset_months);
            hard_failed = 1;
            continue;
        }
        if (first_open_date == 0u || open_date < first_open_date) {
            first_open_date = open_date;
        }

        int direct_result = kbo_process_due_independent_team_acquisition_open_event(
            today,
            event_league_id,
            open_date,
            title,
            source);
        if (direct_result > 0) {
            direct_processed = 1;
        } else if (direct_result < 0) {
            direct_deferred = 1;
        }

        int exists = kbo_custom_event_exists_by_kind_for_date(
            event_league_id,
            open_date,
            KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN);
        int created_event = 0;
        if (!exists) {
            created_event = create_kbo_league_event(
                open_date / 10000u,
                (open_date / 100u) % 100u,
                open_date % 100u,
                event_league_id,
                OOTP27_EVENT_TYPE_CUSTOM_EVENT,
                title,
                0,
                source != NULL ? source : g_kbo_default_event_source);
        }

        exists = exists
            || created_event
            || kbo_custom_event_exists_by_kind_for_date(
                event_league_id,
                open_date,
                KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN);
        if (!exists) {
            hard_failed = 1;
            failed++;
        } else {
            ready++;
        }
        if (created_event) {
            created++;
        }

        kbo_log_runtimef(
            "KBO independent futures acquisition schedule source=%s today=%u event_league_id=%u anchor_league_id=%u team=%u csv=%s start=%u start_source=%s open=%u offset_months=%u created=%d ready=%d",
            source != NULL ? source : "",
            today,
            event_league_id,
            anchor_league_id,
            leagues[i].team_id,
            leagues[i].team_csv_id,
            season_start,
            start_source != NULL ? start_source : "",
            open_date,
            offset_months,
            created_event,
            exists);
    }

    if (ready <= 0 && failed > 0) {
        if (today % 10000u >= 501u) {
            kbo_log_runtimef(
                "KBO independent futures acquisition schedule deferred source=%s reason=futures_memory_start_required today=%u year=%u failed=%d hard_failed=%d",
                source != NULL ? source : "",
                today,
                schedule_year,
                failed,
                hard_failed);
            return -1;
        }
        return hard_failed ? -1 : 0;
    }
    if (direct_deferred) {
        return -1;
    }
    if (ready > 0 && failed == 0 && first_open_date != 0u) {
        g_kbo_independent_acquisition_schedule_ready_year = schedule_year;
        g_kbo_independent_acquisition_schedule_ready_event_league_id = event_league_id;
        g_kbo_independent_acquisition_schedule_ready_first_open_date = first_open_date;
    }
    return created || direct_processed;
}

int kbo_schedule_independent_team_acquisition_custom_events(const char* source)
{
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        kbo_log_runtimef(
            "KBO independent futures acquisition schedule skipped source=%s reason=ssot_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }
    return kbo_schedule_independent_team_acquisition_custom_events_for_date(today, source);
}
