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
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) == 0) {
        return;
    }
    if (InterlockedExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0) == 0) {
        return;
    }

    uint8_t* financials = g_kbo_foreign_fa_demand_ladder_snapshot.financials;
    int restored = 0;
    if (financials != NULL) {
        for (int i = 0; i < 9; i++) {
            restored += kbo_write_i32(
                financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i],
                g_kbo_foreign_fa_demand_ladder_snapshot.values[i]);
        }
    }

    static LONG restore_log_count = 0;
    LONG slot = InterlockedIncrement(&restore_log_count);
    if (slot <= 80) {
        kbo_log_runtimef(
            "KBO foreign FA demand baseline restored source=%s financials=%p restored=%d",
            source != NULL ? source : "",
            (void*)financials,
            restored);
    }

    g_kbo_foreign_fa_demand_ladder_snapshot.financials = NULL;
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
