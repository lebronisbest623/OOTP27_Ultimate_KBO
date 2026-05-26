#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/foreign/signability/submit_offer_probe/submit_offer_probe.h"
#include "../src/foreign/common/policy/foreign_player_policy.h"

LONG g_kbo_no_minor_contract_demand_floor_enabled = 0;
KboFinancialSalaryLadderSnapshot g_kbo_foreign_fa_demand_ladder_snapshot = {0};
KboLock g_kbo_foreign_fa_demand_ladder_snapshot_lock = KBO_LOCK_INIT;
volatile LONG g_kbo_foreign_fa_demand_restore_timer_pending = 0;
volatile LONG g_kbo_no_minor_contract_demand_floor_scanner_started = 0;

const uint32_t KBO_FINANCIALS_SALARY_LADDER_OFFSETS[9] = {
    OOTP27_FINANCIALS_SALARY_LADDER_MINIMUM_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_POOR_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_FAIR_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_BELOW_AVERAGE_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_AVERAGE_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_ABOVE_AVERAGE_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_GOOD_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_STAR_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_SUPERSTAR_OFFSET
};

int memory_range_readable(const void* address, SIZE_T size)
{
    if (address == NULL || size == 0u) {
        return 0;
    }

    uintptr_t start = (uintptr_t)address;
    uintptr_t end = start + (uintptr_t)size;
    if (start < 0x10000u || end <= start) {
        return 0;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) {
        return 0;
    }

    DWORD protect = mbi.Protect & 0xffu;
    int readable = protect == PAGE_READONLY
        || protect == PAGE_READWRITE
        || protect == PAGE_WRITECOPY
        || protect == PAGE_EXECUTE_READ
        || protect == PAGE_EXECUTE_READWRITE
        || protect == PAGE_EXECUTE_WRITECOPY;
    uintptr_t region_end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return readable && end <= region_end;
}

void kbo_log_runtime_line_at(const char* file, int line, const char* message)
{
    (void)file;
    (void)line;
    (void)message;
}

void kbo_log_runtimef_at(const char* file, int line, const char* format, ...)
{
    (void)file;
    (void)line;
    (void)format;
}

int kbo_runtime_sleep_should_continue(uint32_t total_ms)
{
    (void)total_ms;
    return 0;
}

int kbo_start_runtime_thread(LPTHREAD_START_ROUTINE start, LPVOID parameter, const char* label)
{
    (void)start;
    (void)parameter;
    (void)label;
    return 0;
}

const KboForeignPlayerPolicy* kbo_foreign_player_policy(void)
{
    static const KboForeignPlayerPolicy policy = {
        .no_minor_demand_restore_timer_delay_ms = 1
    };
    return &policy;
}

static DWORD test_page_protect(void* address)
{
    MEMORY_BASIC_INFORMATION mbi;
    assert(VirtualQuery(address, &mbi, sizeof(mbi)) != 0);
    return mbi.Protect & 0xffu;
}

static void test_seed_financials(
    uint8_t* financials,
    const int32_t ladder_values[9],
    int32_t demand_ceiling_value)
{
    for (int i = 0; i < 9; i++) {
        *(int32_t*)(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i]) = ladder_values[i];
    }
    *(int32_t*)(financials + OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET) = demand_ceiling_value;
}

static void test_assert_financials(
    uint8_t* financials,
    const int32_t ladder_values[9],
    int32_t demand_ceiling_value)
{
    for (int i = 0; i < 9; i++) {
        assert(*(int32_t*)(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i]) == ladder_values[i]);
    }
    assert(*(int32_t*)(financials + OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET) == demand_ceiling_value);
}

static void test_write_i32_updates_readonly_page_and_restores_protection(void)
{
    uint8_t* page = VirtualAlloc(NULL, 4096u, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(page != NULL);
    int32_t* value = (int32_t*)(page + 0x100u);
    *value = 1234;

    DWORD old_protect = 0;
    assert(VirtualProtect(page, 4096u, PAGE_READONLY, &old_protect));

    assert(kbo_write_i32((uint8_t*)value, 1234) == 1);
    assert(*value == 1234);
    assert(test_page_protect(value) == PAGE_READONLY);

    assert(kbo_write_i32((uint8_t*)value, 5678) == 1);
    assert(*value == 5678);
    assert(test_page_protect(value) == PAGE_READONLY);

    assert(VirtualFree(page, 0u, MEM_RELEASE));
    printf("test_write_i32_updates_readonly_page_and_restores_protection: PASS\n");
}

static void test_batch_foreign_fa_financials_write_updates_only_target_fields(void)
{
    uint8_t* financials = VirtualAlloc(NULL, 4096u, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(financials != NULL);

    const int32_t original_ladder[9] = {
        100, 200, 300, 400, 500, 600, 700, 800, 900
    };
    const int32_t patched_ladder[9] = {
        1100, 2200, 3300, 4400, 5500, 6600, 7700, 8800, 9900
    };
    test_seed_financials(financials, original_ladder, 1000);
    *(int32_t*)(financials + 0x2a0u) = 424242;
    *(int32_t*)(financials + 0x360u) = 515151;

    DWORD old_protect = 0;
    assert(VirtualProtect(financials, 4096u, PAGE_READONLY, &old_protect));

    assert(kbo_write_foreign_fa_financials_values(financials, patched_ladder, 11111) == 10);
    test_assert_financials(financials, patched_ladder, 11111);
    assert(*(int32_t*)(financials + 0x2a0u) == 424242);
    assert(*(int32_t*)(financials + 0x360u) == 515151);
    assert(test_page_protect(financials + OOTP27_FINANCIALS_SALARY_LADDER_MINIMUM_OFFSET) == PAGE_READONLY);

    assert(kbo_write_foreign_fa_financials_values(financials, patched_ladder, 11111) == 10);
    test_assert_financials(financials, patched_ladder, 11111);
    assert(test_page_protect(financials + OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET) == PAGE_READONLY);

    assert(VirtualFree(financials, 0u, MEM_RELEASE));
    printf("test_batch_foreign_fa_financials_write_updates_only_target_fields: PASS\n");
}

int main(void)
{
    test_write_i32_updates_readonly_page_and_restores_protection();
    test_batch_foreign_fa_financials_write_updates_only_target_fields();
    printf("Foreign FA financials write tests passed.\n");
    return 0;
}
