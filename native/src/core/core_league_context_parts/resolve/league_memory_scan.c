#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#include "../api/league_context_lookup.h"
#include "named_scan/league_named_scan.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../logging/core_log.h"

#define KBO_LEAGUE_PTR_SCAN_COOLDOWN_MS 60000ull
#define KBO_LEAGUE_PTR_BUSY_LOG_COOLDOWN_MS 10000ull
#define KBO_LEAGUE_PTR_BOUNDED_SCAN_MAX_REGION ((SIZE_T)0x00400000u)

static volatile LONG g_kbo_league_ptr_memory_scan_in_progress = 0;

static int kbo_cached_league_ptr_valid(uintptr_t ptr, uint32_t league_id, uint32_t id_offset)
{
    return ptr != 0u
        && league_id != 0u
        && (kbo_league_candidate_matches_id(ptr, league_id, id_offset)
            || kbo_core_named_league_candidate_score(ptr, league_id, NULL, 0u) >= KBO_CORE_NAMED_LEAGUE_SCAN_MIN_SCORE);
}

static uintptr_t kbo_find_league_ptr_by_bounded_id_scan(
    uint32_t league_id,
    SIZE_T max_region_size,
    uint32_t* out_id_offset)
{
    static const uint32_t id_offsets[] = {
        OOTP27_KBO_LEAGUE_ID_OFFSET,
        OOTP27_KBO_LEAGUE_ID_OFFSET + 8u
    };

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
                && mbi.RegionSize <= max_region_size) {
            uintptr_t scan_end = end >= sizeof(uint32_t) ? end - sizeof(uint32_t) : base;
            for (uintptr_t p = base; p <= scan_end; p += sizeof(uint32_t)) {
                if (*(uint32_t*)p != league_id) {
                    continue;
                }

                for (size_t i = 0; i < sizeof(id_offsets) / sizeof(id_offsets[0]); i++) {
                    uint32_t id_offset = id_offsets[i];
                    if (p < base + id_offset) {
                        continue;
                    }

                    uintptr_t candidate = p - id_offset;
                    if ((candidate & 7u) != 0u
                            || candidate < base
                            || candidate + OOTP27_KBO_LEAGUE_ID_OFFSET + 16u > end
                            || !kbo_league_candidate_matches_id(candidate, league_id, id_offset)) {
                        continue;
                    }

                    if (out_id_offset != NULL) {
                        *out_id_offset = id_offset;
                    }
                    return candidate;
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

    return 0u;
}

uintptr_t kbo_find_league_ptr_by_memory_scan(uint32_t league_id)
{
    static uintptr_t cached_ptr = 0;
    static uint32_t cached_league_id = 0;
    static uint32_t cached_id_offset = 0;
    static ULONGLONG last_scan_ms = 0;
    static ULONGLONG last_busy_log_ms = 0;

    if (cached_league_id == league_id
            && kbo_cached_league_ptr_valid(cached_ptr, league_id, cached_id_offset)) {
        return cached_ptr;
    }
    cached_ptr = 0;
    cached_league_id = 0;
    cached_id_offset = 0;

    ULONGLONG now = GetTickCount64();
    if (last_scan_ms != 0u && now - last_scan_ms < KBO_LEAGUE_PTR_SCAN_COOLDOWN_MS) {
        return 0;
    }

    if (InterlockedCompareExchange(&g_kbo_league_ptr_memory_scan_in_progress, 1, 0) != 0) {
        if (now - last_busy_log_ms >= KBO_LEAGUE_PTR_BUSY_LOG_COOLDOWN_MS) {
            last_busy_log_ms = now;
            kbo_log_runtimef("KBO league ptr memory scan skipped league_id=%u reason=scan_in_progress", league_id);
        }
        return 0;
    }

    uintptr_t result = 0u;
    uint32_t result_id_offset = 0u;
    last_scan_ms = now;
    kbo_log_runtimef("KBO league ptr memory scan started league_id=%u mode=bounded_named", league_id);

    uintptr_t named_ptr = kbo_find_named_league_ptr_by_memory_scan_all(league_id);
    if (named_ptr != 0u) {
        result = named_ptr;
        result_id_offset = *(uint32_t*)(named_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET) == league_id
            ? OOTP27_KBO_LEAGUE_ID_OFFSET
            : OOTP27_KBO_LEAGUE_ID_OFFSET + 8u;
        goto done;
    }

    result = kbo_find_league_ptr_by_bounded_id_scan(
        league_id,
        KBO_LEAGUE_PTR_BOUNDED_SCAN_MAX_REGION,
        &result_id_offset);
    if (result != 0u) {
        kbo_log_runtimef(
            "KBO league ptr found by bounded memory scan league_id=%u ptr=%p id_offset=0x%x league_year=%u phase=%u phase_year=%u",
            league_id,
            (void*)result,
            result_id_offset,
            *(uint32_t*)(result + OOTP27_KBO_LEAGUE_YEAR_OFFSET),
            (unsigned)*(uint8_t*)(result + OOTP27_KBO_LEAGUE_PHASE_OFFSET),
            *(uint32_t*)(result + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET));
        goto done;
    }

    kbo_log_runtimef(
        "KBO league ptr memory scan missed league_id=%u mode=bounded_named max_region=0x%x",
        league_id,
        (unsigned)KBO_LEAGUE_PTR_BOUNDED_SCAN_MAX_REGION);

done:
    if (result != 0u) {
        cached_ptr = result;
        cached_league_id = league_id;
        cached_id_offset = result_id_offset;
    }
    InterlockedExchange(&g_kbo_league_ptr_memory_scan_in_progress, 0);
    return result;
}
