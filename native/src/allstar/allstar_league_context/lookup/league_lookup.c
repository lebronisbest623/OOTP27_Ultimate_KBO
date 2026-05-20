#include "../allstar_league_context.h"
#include "../../../core/dates/constants/kbo_date_constants.h"

#define KBO_ALLSTAR_GLOBAL_EXTENDED_VECTOR_START_OFFSET 0x130u
#define KBO_ALLSTAR_GLOBAL_EXTENDED_VECTOR_END_OFFSET 0x5F8u
#define KBO_ALLSTAR_GLOBAL_DIRECT_SCAN_BYTES 0x600u

/* Scan one vector entry in the global DB for a league matching league_id.
 * Returns the league pointer if found, 0 otherwise. */
uintptr_t kbo_scan_league_vec(uintptr_t global, uint32_t vec_off, uint32_t league_id, const KboAllstarLayout* layout)
{
    uint32_t cnt_off = vec_off + 8u;
    if (!memory_range_readable((void*)(global + vec_off), 16u)) {
        return 0;
    }
    uintptr_t candidate_vec = *(uintptr_t*)(global + vec_off);
    int32_t candidate_count = *(int32_t*)(global + cnt_off);
    if (candidate_vec == 0 || candidate_count <= 0 || candidate_count > 10000
            || !memory_range_readable((void*)candidate_vec, (SIZE_T)candidate_count * sizeof(uintptr_t))) {
        return 0;
    }
    for (int32_t j = 0; j < candidate_count; j++) {
        uintptr_t lc = *(uintptr_t*)(candidate_vec + (uintptr_t)j * sizeof(uintptr_t));
        if (lc == 0 || !memory_range_readable((void*)lc, layout->league_id_fallback_offset + 4u)) {
            continue;
        }
        uint8_t* ll = (uint8_t*)lc;
        uint32_t legacy_id = 0u;
        if (memory_range_readable(ll + OOTP27_KBO_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
            legacy_id = *(uint32_t*)(ll + OOTP27_KBO_LEAGUE_ID_OFFSET);
        }
        uint32_t primary_id = kbo_allstar_read_u32(ll, layout->league_id_primary_offset);
        uint32_t fallback_id = kbo_allstar_read_u32(ll, layout->league_id_fallback_offset);
        if ((legacy_id == league_id || primary_id == league_id || fallback_id == league_id)
                && kbo_allstar_league_core_plausible(lc)) {
            return lc;
        }
    }
    return 0;
}

uintptr_t kbo_find_allstar_league_ptr(uint32_t league_id)
{
    if (league_id == 0u) {
        return 0;
    }

    uintptr_t imported = (uintptr_t)InterlockedCompareExchangePointer(
        (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
        NULL,
        NULL);
    if (imported != 0 && kbo_allstar_league_core_plausible(imported)) {
        uint8_t* league = (uint8_t*)imported;
        KboAllstarLayout imported_layout = kbo_get_allstar_layout();
        uint32_t primary_id = kbo_allstar_read_u32(league, imported_layout.league_id_primary_offset);
        uint32_t fallback_id = kbo_allstar_read_u32(league, imported_layout.league_id_fallback_offset);
        if (primary_id == league_id || fallback_id == league_id) {
            return imported;
        }
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uintptr_t found = 0;

    /* Pass 1: original vector offsets (0xa0-0x128) */
    static const uint32_t league_vec_offsets[] = {
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0xc0u, 0xc8u, 0xd0u, 0xd8u,
        0xe0u, 0xe8u, 0xf0u, 0xf8u, 0x100u, 0x108u, 0x110u, 0x118u, 0x120u, 0x128u
    };
    for (size_t i = 0; i < sizeof(league_vec_offsets) / sizeof(league_vec_offsets[0]); i++) {
        found = kbo_scan_league_vec(global, league_vec_offsets[i], league_id, &layout);
        if (found) {
            return found;
        }
    }

    /* Pass 2: extended vector scan may catch builds that moved league vectors. */
    for (uint32_t vec_off = KBO_ALLSTAR_GLOBAL_EXTENDED_VECTOR_START_OFFSET;
            vec_off <= KBO_ALLSTAR_GLOBAL_EXTENDED_VECTOR_END_OFFSET;
            vec_off += 8u) {
        found = kbo_scan_league_vec(global, vec_off, league_id, &layout);
        if (found) {
            kbo_log_runtimef("KBO allstar: found league at extended global+0x%x ptr=%p", vec_off, (void*)found);
            return found;
        }
    }

    /* Pass 3: scan global DB directly for any pointer to a plausible league object.
     * Catches layouts where the league is stored as a direct pointer, not in a vector. */
    static volatile LONG s_reject_log_count = 0;
    if (memory_range_readable((void*)global, KBO_ALLSTAR_GLOBAL_DIRECT_SCAN_BYTES)) {
        for (uint32_t ptr_off = 0u;
                ptr_off + (uint32_t)sizeof(uintptr_t) <= KBO_ALLSTAR_GLOBAL_DIRECT_SCAN_BYTES;
                ptr_off += (uint32_t)sizeof(uintptr_t)) {
            uintptr_t candidate = *(uintptr_t*)((uint8_t*)global + ptr_off);
            if (candidate < KBO_RUNTIME_MIN_USER_POINTER) {
                continue;
            }
            /* Quick year probe before the expensive full check */
            if (!memory_range_readable((void*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
                continue;
            }
            uint32_t yr = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
            if (yr < KBO_SEASON_YEAR_MIN || yr > KBO_SIM_YEAR_MAX) {
                continue;
            }
            if (!kbo_allstar_league_core_plausible(candidate)) {
                LONG rl = InterlockedIncrement(&s_reject_log_count);
                if (rl <= 40) {
                    uint8_t ph = memory_range_readable((void*)(candidate + OOTP27_KBO_LEAGUE_PHASE_OFFSET), 1)
                        ? *(uint8_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_OFFSET) : 0xffu;
                    kbo_log_runtimef("KBO allstar: global+0x%x ptr=%p year=%u phase=%u rejected by plausibility",
                        ptr_off, (void*)candidate, yr, (unsigned)ph);
                }
                continue;
            }
            uint8_t* lp = (uint8_t*)candidate;
            uint32_t lid_a = memory_range_readable(lp + OOTP27_KBO_LEAGUE_ID_OFFSET, 4) ? *(uint32_t*)(lp + OOTP27_KBO_LEAGUE_ID_OFFSET) : 0u;
            uint32_t lid_p = kbo_allstar_read_u32(lp, layout.league_id_primary_offset);
            uint32_t lid_f = kbo_allstar_read_u32(lp, layout.league_id_fallback_offset);
            if (lid_a == league_id || lid_p == league_id || lid_f == league_id) {
                kbo_log_runtimef("KBO allstar: found league via direct db scan global+0x%x ptr=%p year=%u id=%u/%u/%u",
                    ptr_off, (void*)candidate, yr, lid_a, lid_p, lid_f);
                return candidate;
            }
        }
    }

    return 0;
}
