#include "award_schedule_probe_internal.h"

volatile LONG g_kbo_award_schedule_probe_started = 0;

#define KBO_AWARD_SCHEDULE_NORMAL_SLEEP_MS 2000u
#define KBO_AWARD_SCHEDULE_FAST_SLEEP_MS 250u
#define KBO_AWARD_SCHEDULE_FAST_RETRY_PULSES 80

static int kbo_award_schedule_probe_fast_window(uint32_t date_key)
{
    uint32_t month_day = date_key % 10000u;
    return month_day >= 1001u && month_day <= 1215u;
}

static void kbo_award_probe_copy_name(
    uint8_t* league,
    uint32_t offset,
    const char* label,
    char* out,
    size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    if (league == NULL || label == NULL || out == NULL || out_size == 0u) {
        return;
    }

    if (!copy_ootp_string_object_text(league, offset, out, out_size)) {
        snprintf(out, out_size, "<unreadable>");
    }
}

static void kbo_log_award_schedule_probe(uintptr_t league_ptr, uint32_t league_id, uint32_t date_key)
{
    uint8_t* league = (uint8_t*)league_ptr;
    if (league == NULL
            || !memory_range_readable(league, KBO_AWARD_RULES_BASE_OFFSET + KBO_AWARD_RULES_BYTES)
            || !memory_range_readable(league + KBO_AWARD_HOF_RULES_OFFSET, KBO_AWARD_HOF_RULES_BYTES)) {
        kbo_log_runtimef(
            "KBO award schedule probe skipped league=%p league_id=%u reason=league_unreadable",
            (void*)league_ptr,
            league_id);
        return;
    }

    char pitcher[96] = {0};
    char mvp[96] = {0};
    char rookie[96] = {0};
    char defense[96] = {0};
    kbo_award_probe_copy_name(league, KBO_AWARD_PITCHER_NAME_OFFSET, "pitcher", pitcher, sizeof(pitcher));
    kbo_award_probe_copy_name(league, KBO_AWARD_MVP_NAME_OFFSET, "mvp", mvp, sizeof(mvp));
    kbo_award_probe_copy_name(league, KBO_AWARD_ROOKIE_NAME_OFFSET, "rookie", rookie, sizeof(rookie));
    kbo_award_probe_copy_name(league, KBO_AWARD_DEFENSE_NAME_OFFSET, "defense", defense, sizeof(defense));

    char flags[192] = {0};
    size_t used = 0u;
    for (uint32_t i = 0u; i < KBO_AWARD_FLAGS_COUNT; i++) {
        uint32_t offset = KBO_AWARD_FLAGS_BASE_OFFSET + i;
        int wrote = snprintf(
            flags + used,
            used < sizeof(flags) ? sizeof(flags) - used : 0u,
            "%s%03x=%u",
            i == 0u ? "" : " ",
            offset,
            (unsigned)league[offset]);
        if (wrote <= 0) {
            break;
        }
        used += (size_t)wrote;
        if (used >= sizeof(flags)) {
            break;
        }
    }

    kbo_log_runtimef(
        "KBO award schedule probe league=%p league_id=%u date=%u names={pitcher:'%s',mvp:'%s',rookie:'%s',defense:'%s'} flags=%s",
        (void*)league_ptr,
        league_id,
        date_key,
        pitcher,
        mvp,
        rookie,
        defense,
        flags);

    kbo_log_runtimef(
        "KBO award schedule probe bytes league=%p rules380=%08x/%08x/%08x/%08x/%08x/%08x/%08x/%08x hofc20=%08x/%08x/%08x/%08x",
        (void*)league_ptr,
        *(uint32_t*)(league + 0x380u),
        *(uint32_t*)(league + 0x384u),
        *(uint32_t*)(league + 0x388u),
        *(uint32_t*)(league + 0x38cu),
        *(uint32_t*)(league + 0x390u),
        *(uint32_t*)(league + 0x394u),
        *(uint32_t*)(league + 0x398u),
        *(uint32_t*)(league + 0x39cu),
        *(uint32_t*)(league + 0xc20u),
        *(uint32_t*)(league + 0xc24u),
        *(uint32_t*)(league + 0xc28u),
        *(uint32_t*)(league + 0xc2cu));
}

static DWORD WINAPI kbo_award_schedule_probe_thread(LPVOID parameter)
{
    (void)parameter;

    uintptr_t last_league = 0u;
    uint32_t last_date = 0u;
    char last_save_path[MAX_PATH] = {0};
    int logged = 0;
    int fast_retry_pulses = 0;
    KboCurrentDateTickConsumer date_consumer = {0};
    kbo_current_date_tick_consumer_init(
        &date_consumer,
        "award_schedule_probe",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER
            | KBO_CURRENT_DATE_TICK_CONSUMER_GAP_CATCHUP
            | KBO_CURRENT_DATE_TICK_CONSUMER_OBSERVE_CURRENT_WHEN_IDLE);

    kbo_log_runtime_line("KBO award schedule probe started");

    while (kbo_runtime_threads_should_continue()) {
        uint32_t sleep_ms = fast_retry_pulses > 0
            ? KBO_AWARD_SCHEDULE_FAST_SLEEP_MS
            : KBO_AWARD_SCHEDULE_NORMAL_SLEEP_MS;
        if (!kbo_runtime_sleep_should_continue(sleep_ms)) {
            break;
        }
        if (!kbo_fix_enabled()) {
            continue;
        }

        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
            continue;
        }
        if (last_save_path[0] == '\0' || strcmp(last_save_path, save_path) != 0) {
            snprintf(last_save_path, sizeof(last_save_path), "%s", save_path);
            last_league = 0u;
            last_date = 0u;
            logged = 0;
            fast_retry_pulses = 0;
        }

        KboCurrentDateTickWork date_work = {0};
        int has_hook_date = kbo_current_date_tick_consumer_next(&date_consumer, &date_work);
        uint32_t date_key = has_hook_date ? date_work.date : last_date;
        if (date_key == 0u || date_key == 0xffffffffu || (!has_hook_date && fast_retry_pulses <= 0)) {
            continue;
        }

        uint32_t league_id = kbo_resolve_kbo_league_id();
        uintptr_t league_ptr = kbo_find_league_ptr_from_id(league_id);
        if (league_ptr == 0u) {
            continue;
        }

        if (!logged || league_ptr != last_league || date_key != last_date) {
            kbo_log_award_schedule_probe(league_ptr, league_id, date_key);
            logged = 1;
            last_league = league_ptr;
            last_date = date_key;
            kbo_award_schedule_apply_once(league_id, date_key, 1);
            kbo_award_schedule_log_event_inventory(league_id, date_key);
            if (kbo_award_schedule_probe_fast_window(date_key)) {
                fast_retry_pulses = KBO_AWARD_SCHEDULE_FAST_RETRY_PULSES;
                kbo_log_runtimef(
                    "KBO award schedule fast retry window date=%u pulses=%d sleep_ms=%u",
                    date_key,
                    fast_retry_pulses,
                    (uint32_t)KBO_AWARD_SCHEDULE_FAST_SLEEP_MS);
            } else {
                fast_retry_pulses = 0;
            }
            if (has_hook_date) {
                kbo_current_date_tick_consumer_mark_processed(&date_consumer);
            }
        } else {
            kbo_award_schedule_apply_once(league_id, date_key, 0);
            if (fast_retry_pulses > 0) {
                fast_retry_pulses--;
            }
            if (has_hook_date) {
                kbo_current_date_tick_consumer_mark_processed(&date_consumer);
            }
        }
    }

    kbo_log_runtime_line("KBO award schedule probe stopped");
    InterlockedExchange(&g_kbo_award_schedule_probe_started, 0);
    return 0;
}

int start_kbo_award_schedule_probe_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_award_schedule_probe_started, 1, 0) != 0) {
        return 1;
    }
    if (!kbo_start_runtime_thread(kbo_award_schedule_probe_thread, NULL, "award schedule probe")) {
        InterlockedExchange(&g_kbo_award_schedule_probe_started, 0);
        return 0;
    }
    return 1;
}
