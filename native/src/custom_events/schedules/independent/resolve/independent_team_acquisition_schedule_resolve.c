#include "../independent_team_acquisition_schedule_module.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../core/dates/constants/kbo_date_constants.h"

KboIndependentAcquisitionMemoryStartCache g_kbo_independent_acquisition_memory_start_cache = {0};

static int kbo_independent_team_acquisition_store_memory_start_cache(
    uint32_t league_id,
    uint32_t season,
    uint32_t start_date,
    uintptr_t league_ptr)
{
    char save_path[MAX_PATH] = {0};
    if (league_id == 0u || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX
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
    if (league_id == 0u || expected_year < KBO_SEASON_YEAR_MIN || expected_year > KBO_SIM_YEAR_MAX) {
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
            || expected_year < KBO_SEASON_YEAR_MIN || expected_year > KBO_SIM_YEAR_MAX) {
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

int kbo_independent_team_acquisition_resolve_start_date(
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
