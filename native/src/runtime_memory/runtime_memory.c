#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../bootstrap/abi/ootp_offsets.h"
#include "../core/dates/constants/kbo_date_constants.h"
#include "../core/logging/core_log.h"
#include "../core/runtime_tuning/runtime_tuning_policy.h"
#include "runtime_memory.h"

#if defined(_MSC_VER)
#define KBO_RUNTIME_MEMORY_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__)
#define KBO_RUNTIME_MEMORY_THREAD_LOCAL __thread
#else
#define KBO_RUNTIME_MEMORY_THREAD_LOCAL
#endif

typedef struct KboReadableRegionCacheEntry {
    uintptr_t base;
    uintptr_t end;
    DWORD tick;
} KboReadableRegionCacheEntry;

enum {
    KBO_READABLE_REGION_CACHE_SLOTS = 32,
    KBO_READABLE_REGION_CACHE_TTL_MS = 100u
};

static KBO_RUNTIME_MEMORY_THREAD_LOCAL KboReadableRegionCacheEntry
    g_kbo_readable_region_cache[KBO_READABLE_REGION_CACHE_SLOTS];

static int protect_allows_read(DWORD protect)
{
    if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0) {
        return 0;
    }
    protect &= 0xffu;
    return protect == PAGE_READONLY
        || protect == PAGE_READWRITE
        || protect == PAGE_WRITECOPY
        || protect == PAGE_EXECUTE_READ
        || protect == PAGE_EXECUTE_READWRITE
        || protect == PAGE_EXECUTE_WRITECOPY;
}

static int readable_region_cache_hit(uintptr_t start, uintptr_t end, DWORD now)
{
    for (int i = 0; i < KBO_READABLE_REGION_CACHE_SLOTS; i++) {
        KboReadableRegionCacheEntry* entry = &g_kbo_readable_region_cache[i];
        if (entry->base == 0u || entry->end <= entry->base || entry->tick == 0u) {
            continue;
        }
        if (now - entry->tick > KBO_READABLE_REGION_CACHE_TTL_MS) {
            continue;
        }
        if (start >= entry->base && end <= entry->end) {
            return 1;
        }
    }
    return 0;
}

static void readable_region_cache_store(uintptr_t region_base, uintptr_t region_end, DWORD now)
{
    if (region_base < KBO_RUNTIME_MIN_USER_POINTER || region_end <= region_base) {
        return;
    }

    uintptr_t slot = (region_base >> 12) & (KBO_READABLE_REGION_CACHE_SLOTS - 1u);
    g_kbo_readable_region_cache[slot].base = region_base;
    g_kbo_readable_region_cache[slot].end = region_end;
    g_kbo_readable_region_cache[slot].tick = now;
}

int memory_range_readable(const void* address, SIZE_T size)
{
    if (address == NULL || size == 0) {
        return 0;
    }
    uintptr_t start = (uintptr_t)address;
    if (start < KBO_RUNTIME_MIN_USER_POINTER) {
        return 0;
    }
    uintptr_t end = start + size;
    if (end <= start) {
        return 0;
    }

    DWORD now = GetTickCount();
    if (readable_region_cache_hit(start, end, now)) {
        return 1;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0) {
        return 0;
    }
    if (mbi.State != MEM_COMMIT || !protect_allows_read(mbi.Protect)) {
        return 0;
    }
    uintptr_t region_base = (uintptr_t)mbi.BaseAddress;
    uintptr_t region_end = region_base + mbi.RegionSize;
    if (region_end <= region_base || end > region_end) {
        return 0;
    }
    readable_region_cache_store(region_base, region_end, now);
    return 1;
}

/*
 * Returns 1 if 'candidate' has the hallmarks of the OOTP global game-database
 * object: a readable team vector and a sane team count.  Keep the checks cheap
 * so this can be called on every aligned pointer in the .data section.
 */
static int looks_like_ootp_global_db(uintptr_t candidate)
{
    /* Must be readable up past the team-count field (offset 0x9c). */
    if (!memory_range_readable((void*)candidate, 0xb0)) {
        return 0;
    }

    uint8_t* db = (uint8_t*)candidate;

    int32_t team_count = *(int32_t*)(db + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_count < 2 || team_count > 500) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(db + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    if (team_vector == 0) {
        return 0;
    }
    if (!memory_range_readable((void*)team_vector,
            (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    /* Validate each of the first several team pointers against known OOTP
     * team-object field ranges.  This rejects non-team arrays that happen to
     * satisfy the coarser count/vector checks above. */
    int checks = team_count < 5 ? team_count : 5;
    for (int i = 0; i < checks; i++) {
        uintptr_t tp = *(uintptr_t*)(team_vector + (uintptr_t)i * sizeof(uintptr_t));
        if (tp == 0 || !memory_range_readable((void*)tp, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            return 0;
        }
        uint8_t* team = (uint8_t*)tp;

        /* Team ID at 0x4450: must be a small positive integer. */
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id == 0 || team_id > 100000) {
            return 0;
        }

        /* League ID at 0x120: must be a small positive integer. */
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0 || league_id > 100000) {
            return 0;
        }
    }

    return 1;
}

static uintptr_t g_ootp_cached_global_db  = 0;
static ULONGLONG g_ootp_last_db_scan_ms   = 0;

uintptr_t get_ootp_cached_global_database(void)
{
    return g_ootp_cached_global_db;
}

/*
 * Scans the writable, non-executable PE sections of the host executable for a
 * pointer that satisfies looks_like_ootp_global_db().  This replaces the old
 * hard-coded RVA approach so the DLL keeps working across game patch updates.
 */
uintptr_t get_ootp_global_database(void)
{
    /* ---- fast path: cached pointer still valid ---- */
    uintptr_t cached = g_ootp_cached_global_db;
    if (cached != 0) {
        if (memory_range_readable((void*)cached, 0xb0)) {
            int32_t tc = *(int32_t*)((uint8_t*)cached + OOTP27_KBO_TEAM_COUNT_OFFSET);
            if (tc >= 2 && tc <= 5000) {
                return cached;
            }
        }
        g_ootp_cached_global_db = 0;
    }

    /* ---- rate-limit the scan so an empty main-menu doesn't burn CPU ---- */
    ULONGLONG now = GetTickCount64();
    if (now - g_ootp_last_db_scan_ms < (ULONGLONG)kbo_runtime_tuning_policy()->global_db_scan_interval_ms) {
        return 0;
    }
    g_ootp_last_db_scan_ms = now;

    /* ---- walk writable data sections looking for the DB pointer ---- */
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return 0;
    }

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;

    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_WRITE))   continue;
        if (!(chars & IMAGE_SCN_MEM_READ))    continue;
        if (  chars & IMAGE_SCN_MEM_EXECUTE)  continue;

        uint8_t* sec  = base + sections[s].VirtualAddress;
        DWORD    vsz  = sections[s].Misc.VirtualSize;
        if (vsz < sizeof(uintptr_t)) {
            continue;
        }
        DWORD limit = vsz - (DWORD)sizeof(uintptr_t);

        for (DWORD off = 0; off <= limit; off += sizeof(uintptr_t)) {
            uintptr_t v = *(uintptr_t*)(sec + off);
            /* Skip obvious non-pointers quickly before doing VirtualQuery. */
            if (v < KBO_RUNTIME_MIN_USER_POINTER || v > KBO_RUNTIME_MAX_USER_POINTER) {
                continue;
            }
            if (!looks_like_ootp_global_db(v)) {
                continue;
            }
            g_ootp_cached_global_db = v;
            kbo_log_runtimef(
                "KBO: global db found by scan section=%d rva=0x%08lX ptr=0x%llX teams=%d",
                (int)s,
                (unsigned long)(sections[s].VirtualAddress + off),
                (unsigned long long)v,
                *(int32_t*)((uint8_t*)v + OOTP27_KBO_TEAM_COUNT_OFFSET));
            return v;
        }
    }

    return 0;
}

uint8_t* find_ootp_executable_pattern(const uint8_t* pattern, size_t pattern_len)
{
    if (pattern == NULL || pattern_len == 0 || pattern_len > 0x200) {
        return NULL;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return NULL;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return NULL;
    }

    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;

    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ)) {
            continue;
        }
        if (!(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD vsz = sections[s].Misc.VirtualSize;
        if (vsz < pattern_len || vsz < 32) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)pattern_len;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (memcmp(candidate, pattern, pattern_len) == 0) {
                return candidate;
            }
        }
    }

    return NULL;
}

int is_kbo_historical_league_context(uintptr_t league_ptr)
{
    if (league_ptr == 0 || !memory_range_readable((void*)league_ptr, OOTP27_HISTORICAL_LEAGUE_READABLE_BYTES)) {
        return 0;
    }
    uint8_t* league = (uint8_t*)league_ptr;
    uint8_t  historical_gate = *(uint8_t*)(league + OOTP27_HISTORICAL_LEAGUE_GATE_OFFSET);
    uint32_t league_year     = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    return historical_gate == 1
        && league_year >= KBO_SEASON_YEAR_MIN
        && league_year <= KBO_RUNTIME_HISTORICAL_YEAR_MAX;
}

