#include "../submit_offer_probe.h"
#include "../../../common/policy/foreign_player_policy.h"

DWORD WINAPI kbo_foreign_fa_demand_restore_timer_thread(void* param)
{
    (void)param;
    if (kbo_runtime_sleep_should_continue((uint32_t)kbo_foreign_player_policy()->no_minor_demand_restore_timer_delay_ms)) {
        kbo_restore_foreign_fa_demand_salary_ladder("offer_build_timer");
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

    DWORD old_protect = 0;
    if (!VirtualProtect(address, sizeof(int32_t), PAGE_READWRITE, &old_protect)) {
        return 0;
    }

    *(int32_t*)address = value;
    DWORD ignored = 0;
    VirtualProtect(address, sizeof(int32_t), old_protect, &ignored);
    return 1;
}

void kbo_restore_foreign_fa_demand_salary_ladder(const char* source)
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
    if (snapshot.financials != NULL) {
        for (int i = 0; i < 9; i++) {
            restored += kbo_write_i32(
                snapshot.financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i],
                snapshot.values[i]);
        }
        restored += kbo_write_i32(
            snapshot.financials + OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET,
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
            "KBO foreign FA demand baseline restored source=%s baseline_source=0x%x player=%u asian_quota=%u reserve_right=%u holder_team=%u today=%u financials=%p restored=%d complete=%d original_min=%d original_superstar=%d original_ceiling=%d patched_min=%d patched_superstar=%d patched_ceiling=%d",
            source != NULL ? source : "",
            snapshot.source_rva,
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

void kbo_schedule_foreign_fa_demand_restore_timer(void)
{
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_restore_timer_pending, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(
            kbo_foreign_fa_demand_restore_timer_thread,
            NULL,
            "foreign FA demand restore timer")) {
        InterlockedExchange(&g_kbo_foreign_fa_demand_restore_timer_pending, 0);
    }
}
