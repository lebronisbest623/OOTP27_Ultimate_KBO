#include "../allstar_league_context.h"
#include <Windows.h>
#include "../../../core/dates/constants/kbo_date_constants.h"

/* All-Star league and team context helpers. */

uint32_t kbo_allstar_read_u32(uint8_t* base, uint32_t offset)
{
    if (base == NULL || !memory_range_readable(base + offset, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(base + offset);
}

int kbo_allstar_memory_executable(const void* address)
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

int kbo_allstar_league_vtable_plausible(uintptr_t league_ptr)
{
    if (league_ptr == 0 || !memory_range_readable((void*)league_ptr, sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t vtable = *(uintptr_t*)league_ptr;
    if (vtable == 0 || !memory_range_readable((void*)(vtable + OOTP27_ALLSTAR_LEAGUE_CONTEXT_VTABLE_METHOD_OFFSET), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t method = *(uintptr_t*)(vtable + OOTP27_ALLSTAR_LEAGUE_CONTEXT_VTABLE_METHOD_OFFSET);
    return kbo_allstar_memory_executable((void*)method);
}

int kbo_allstar_league_core_plausible(uintptr_t league_ptr)
{
    if (league_ptr == 0) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    if (!memory_range_readable((void*)league_ptr, layout.team_b_offset + sizeof(uint32_t))
            || !memory_range_readable((void*)(league_ptr + layout.league_id_fallback_offset), sizeof(uint32_t))
            || !memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    uint8_t* league = (uint8_t*)league_ptr;
    uint32_t year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    if (year < KBO_SEASON_YEAR_MIN || year > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    uint8_t phase = *(uint8_t*)(league + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    if (phase > 4u) {
        return 0;
    }

    uint32_t phase_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET);
    if (phase_year != 0u && (phase_year < KBO_SEASON_YEAR_MIN || phase_year > KBO_SIM_YEAR_MAX)) {
        return 0;
    }

    if (!kbo_allstar_league_vtable_plausible(league_ptr)) {
        return 0;
    }

    return 1;
}
