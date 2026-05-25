#include "../allstar_league_context.h"
#include <Windows.h>
#include "../../../core/dates/constants/kbo_date_constants.h"

/* All-Star league and team context helpers. */

int kbo_allstar_try_read_bytes(uintptr_t address, void* out, SIZE_T size)
{
    if (out == NULL || size == 0 || address < KBO_RUNTIME_MIN_USER_POINTER) {
        return 0;
    }

    uintptr_t end = address + (uintptr_t)size;
    if (end <= address || end > KBO_RUNTIME_MAX_USER_POINTER) {
        return 0;
    }

    SIZE_T bytes_read = 0u;
    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)address, out, size, &bytes_read)
            || bytes_read != size) {
        return 0;
    }

    return 1;
}

int kbo_allstar_try_read_ptr(uintptr_t address, uintptr_t* out_value)
{
    if (out_value != NULL) {
        *out_value = 0u;
    }
    if (out_value == NULL) {
        return 0;
    }
    return kbo_allstar_try_read_bytes(address, out_value, sizeof(*out_value));
}

int kbo_allstar_try_read_i32(uintptr_t address, int32_t* out_value)
{
    if (out_value != NULL) {
        *out_value = 0;
    }
    if (out_value == NULL) {
        return 0;
    }
    return kbo_allstar_try_read_bytes(address, out_value, sizeof(*out_value));
}

int kbo_allstar_try_read_u8(uint8_t* base, uint32_t offset, uint8_t* out_value)
{
    if (out_value != NULL) {
        *out_value = 0u;
    }
    if (base == NULL || out_value == NULL) {
        return 0;
    }

    uintptr_t address = (uintptr_t)base;
    if (address + (uintptr_t)offset < address) {
        return 0;
    }
    return kbo_allstar_try_read_bytes(address + (uintptr_t)offset, out_value, sizeof(*out_value));
}

int kbo_allstar_try_read_u32(uint8_t* base, uint32_t offset, uint32_t* out_value)
{
    if (out_value != NULL) {
        *out_value = 0u;
    }
    if (base == NULL || out_value == NULL) {
        return 0;
    }

    uintptr_t address = (uintptr_t)base;
    if (address + (uintptr_t)offset < address) {
        return 0;
    }
    return kbo_allstar_try_read_bytes(address + (uintptr_t)offset, out_value, sizeof(*out_value));
}

uint32_t kbo_allstar_read_u32(uint8_t* base, uint32_t offset)
{
    uint32_t value = 0u;
    kbo_allstar_try_read_u32(base, offset, &value);
    return value;
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
    uintptr_t vtable = 0u;
    if (!kbo_allstar_try_read_ptr(league_ptr, &vtable)) {
        return 0;
    }

    uintptr_t method = 0u;
    if (vtable == 0u
            || !kbo_allstar_try_read_ptr(vtable + OOTP27_ALLSTAR_LEAGUE_CONTEXT_VTABLE_METHOD_OFFSET, &method)) {
        return 0;
    }

    return kbo_allstar_memory_executable((void*)method);
}

int kbo_allstar_league_core_plausible(uintptr_t league_ptr)
{
    if (league_ptr == 0) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    uint32_t ignored = 0u;
    uint32_t year = 0u;
    uint8_t phase = 0u;
    uint32_t phase_year = 0u;
    if (!kbo_allstar_try_read_u32(league, layout.team_b_offset, &ignored)
            || !kbo_allstar_try_read_u32(league, layout.league_id_fallback_offset, &ignored)
            || !kbo_allstar_try_read_u32(league, OOTP27_KBO_LEAGUE_YEAR_OFFSET, &year)
            || !kbo_allstar_try_read_u8(league, OOTP27_KBO_LEAGUE_PHASE_OFFSET, &phase)
            || !kbo_allstar_try_read_u32(league, OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET, &phase_year)) {
        return 0;
    }

    if (year < KBO_SEASON_YEAR_MIN || year > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    if (phase > 4u) {
        return 0;
    }

    if (phase_year != 0u && (phase_year < KBO_SEASON_YEAR_MIN || phase_year > KBO_SIM_YEAR_MAX)) {
        return 0;
    }

    if (!kbo_allstar_league_vtable_plausible(league_ptr)) {
        return 0;
    }

    return 1;
}
