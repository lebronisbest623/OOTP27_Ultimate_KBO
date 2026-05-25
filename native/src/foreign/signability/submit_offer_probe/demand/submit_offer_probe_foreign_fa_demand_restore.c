#include "../submit_offer_probe.h"
#include "../../../common/policy/foreign_player_policy.h"

enum {
    KBO_FOREIGN_FA_FINANCIALS_WRITE_TARGET_COUNT = 10,
    KBO_FOREIGN_FA_FINANCIALS_WRITE_START_OFFSET = OOTP27_FINANCIALS_SALARY_LADDER_MINIMUM_OFFSET,
    KBO_FOREIGN_FA_FINANCIALS_WRITE_END_OFFSET =
        OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET + sizeof(int32_t)
};

static void kbo_restore_foreign_fa_demand_salary_ladder_for_generation(
    const char* source,
    LONG expected_generation);

DWORD WINAPI kbo_foreign_fa_demand_restore_timer_thread(void* param)
{
    LONG expected_generation = (LONG)(intptr_t)param;
    if (kbo_runtime_sleep_should_continue((uint32_t)kbo_foreign_player_policy()->no_minor_demand_restore_timer_delay_ms)) {
        kbo_restore_foreign_fa_demand_salary_ladder_for_generation(
            "offer_build_timer",
            expected_generation);
    }
    InterlockedExchange(&g_kbo_foreign_fa_demand_restore_timer_pending, 0);
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) != 0) {
        kbo_schedule_foreign_fa_demand_restore_timer();
    }
    return 0;
}

int kbo_write_i32(uint8_t* address, int32_t value)
{
    if (address == NULL || !memory_range_readable(address, sizeof(int32_t))) {
        return 0;
    }
    if (*(int32_t*)address == value) {
        return 1;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(address, sizeof(int32_t), PAGE_READWRITE, &old_protect)) {
        return 0;
    }

    *(int32_t*)address = value;
    DWORD ignored = 0;
    VirtualProtect(address, sizeof(int32_t), old_protect, &ignored);
    return 1;
}

static int kbo_foreign_fa_financials_targets_readable(uint8_t* financials)
{
    if (financials == NULL) {
        return 0;
    }

    for (int i = 0; i < 9; i++) {
        if (!memory_range_readable(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i], sizeof(int32_t))) {
            return 0;
        }
    }
    return memory_range_readable(
        financials + OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET,
        sizeof(int32_t));
}

static int kbo_foreign_fa_financials_values_match(
    uint8_t* financials,
    const int32_t ladder_values[9],
    int32_t demand_ceiling_value)
{
    for (int i = 0; i < 9; i++) {
        if (*(int32_t*)(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i]) != ladder_values[i]) {
            return 0;
        }
    }
    return *(int32_t*)(financials + OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET)
        == demand_ceiling_value;
}

static int kbo_write_foreign_fa_financials_values_individually(
    uint8_t* financials,
    const int32_t ladder_values[9],
    int32_t demand_ceiling_value)
{
    int written = 0;
    for (int i = 0; i < 9; i++) {
        written += kbo_write_i32(
            financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i],
            ladder_values[i]);
    }
    written += kbo_write_i32(
        financials + OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET,
        demand_ceiling_value);
    return written;
}

int kbo_write_foreign_fa_financials_values(
    uint8_t* financials,
    const int32_t ladder_values[9],
    int32_t demand_ceiling_value)
{
    if (financials == NULL
            || ladder_values == NULL
            || !kbo_foreign_fa_financials_targets_readable(financials)) {
        return 0;
    }
    if (kbo_foreign_fa_financials_values_match(financials, ladder_values, demand_ceiling_value)) {
        return KBO_FOREIGN_FA_FINANCIALS_WRITE_TARGET_COUNT;
    }

    uint8_t* write_start = financials + KBO_FOREIGN_FA_FINANCIALS_WRITE_START_OFFSET;
    SIZE_T write_size =
        (SIZE_T)(KBO_FOREIGN_FA_FINANCIALS_WRITE_END_OFFSET - KBO_FOREIGN_FA_FINANCIALS_WRITE_START_OFFSET);
    DWORD old_protect = 0;
    if (!memory_range_readable(write_start, write_size)
            || !VirtualProtect(write_start, write_size, PAGE_READWRITE, &old_protect)) {
        return kbo_write_foreign_fa_financials_values_individually(
            financials,
            ladder_values,
            demand_ceiling_value);
    }

    int written = 0;
    for (int i = 0; i < 9; i++) {
        *(int32_t*)(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i]) = ladder_values[i];
        written++;
    }
    *(int32_t*)(financials + OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET) = demand_ceiling_value;
    written++;

    DWORD ignored = 0;
    VirtualProtect(write_start, write_size, old_protect, &ignored);
    return written;
}

static void kbo_restore_foreign_fa_demand_salary_ladder_for_generation(
    const char* source,
    LONG expected_generation)
{
    KboFinancialSalaryLadderSnapshot snapshot = {0};
    int restored = 0;
    int restore_complete = 0;
    int should_retry = 0;

    kbo_lock_enter(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) == 0) {
        kbo_lock_leave(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);
        return;
    }

    snapshot = g_kbo_foreign_fa_demand_ladder_snapshot;
    if (expected_generation != 0 && snapshot.generation != expected_generation) {
        kbo_lock_leave(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);

        static LONG stale_timer_log_count = 0;
        LONG stale_slot = InterlockedIncrement(&stale_timer_log_count);
        if (stale_slot <= 80) {
            kbo_log_runtimef(
                "KBO foreign FA demand baseline restore skipped source=%s reason=stale_timer expected_generation=%ld active_generation=%ld player=%u baseline_source=0x%x",
                source != NULL ? source : "",
                expected_generation,
                snapshot.generation,
                snapshot.player_id,
                snapshot.source_rva);
        }
        return;
    }

    if (snapshot.financials != NULL) {
        restored = kbo_write_foreign_fa_financials_values(
            snapshot.financials,
            snapshot.values,
            snapshot.demand_ceiling_value);
        restore_complete = restored == 10;
    } else {
        restore_complete = 1;
    }

    if (restore_complete) {
        memset(&g_kbo_foreign_fa_demand_ladder_snapshot, 0, sizeof(g_kbo_foreign_fa_demand_ladder_snapshot));
    } else {
        InterlockedExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 1);
        should_retry = 1;
    }
    kbo_lock_leave(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);

    static LONG restore_log_count = 0;
    LONG slot = InterlockedIncrement(&restore_log_count);
    if (slot <= 80 || !restore_complete) {
        kbo_log_runtimef(
            "KBO foreign FA demand baseline restored source=%s baseline_source=0x%x generation=%ld player=%u asian_quota=%u reserve_right=%u holder_team=%u today=%u financials=%p restored=%d complete=%d original_min=%d original_superstar=%d original_ceiling=%d patched_min=%d patched_superstar=%d patched_ceiling=%d",
            source != NULL ? source : "",
            snapshot.source_rva,
            snapshot.generation,
            snapshot.player_id,
            snapshot.asian_quota,
            snapshot.reserve_right,
            snapshot.holder_team_id,
            snapshot.today,
            (void*)snapshot.financials,
            restored,
            restore_complete,
            snapshot.values[0],
            snapshot.values[8],
            snapshot.demand_ceiling_value,
            snapshot.patched_values[0],
            snapshot.patched_values[8],
            snapshot.patched_demand_ceiling_value);
    }

    if (should_retry) {
        kbo_schedule_foreign_fa_demand_restore_timer();
    }
}

void kbo_restore_foreign_fa_demand_salary_ladder(const char* source)
{
    kbo_restore_foreign_fa_demand_salary_ladder_for_generation(source, 0);
}

void kbo_schedule_foreign_fa_demand_restore_timer(void)
{
    LONG generation = 0;
    kbo_lock_enter(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) != 0) {
        generation = g_kbo_foreign_fa_demand_ladder_snapshot.generation;
    }
    kbo_lock_leave(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);
    if (generation == 0) {
        return;
    }

    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_restore_timer_pending, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(
            kbo_foreign_fa_demand_restore_timer_thread,
            (LPVOID)(intptr_t)generation,
            "foreign FA demand restore timer")) {
        InterlockedExchange(&g_kbo_foreign_fa_demand_restore_timer_pending, 0);
    }
}
