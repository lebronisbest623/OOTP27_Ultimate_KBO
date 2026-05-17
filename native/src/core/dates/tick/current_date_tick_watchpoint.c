#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../core_flags/api/flags_api.h"
#include "../../logging/core_log.h"
#include "current_date_tick_capture.h"
#include "current_date_tick_watchpoint.h"

static volatile LONG g_kbo_current_date_tick_watchpoint_started = 0;
static volatile LONG g_kbo_current_date_tick_watchpoint_hits = 0;
static volatile LONG g_kbo_current_date_tick_watchpoint_published = 0;
static volatile LONG g_kbo_current_date_tick_watchpoint_context_sets = 0;
static volatile uintptr_t g_kbo_current_date_tick_watchpoint_address = 0u;
static volatile uintptr_t g_kbo_current_date_tick_watchpoint_last_rip = 0u;
static volatile LONG g_kbo_current_date_tick_watchpoint_last_date = 0;
static PVOID g_kbo_current_date_tick_watchpoint_handler = NULL;

static uintptr_t kbo_current_date_tick_watchpoint_live_field_address(void)
{
    uintptr_t global = get_ootp_cached_global_database();
    if (global == 0u) {
        global = get_ootp_global_database();
    }
    if (global == 0u
            || !memory_range_readable(
                (void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET),
                sizeof(uintptr_t))) {
        return 0u;
    }

    uintptr_t current_date = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    uintptr_t field = current_date + OOTP27_CURRENT_DATE_YEAR_OFFSET;
    if (current_date == 0u || !memory_range_readable((void*)field, sizeof(uint32_t))) {
        return 0u;
    }
    return field;
}

static uint32_t kbo_current_date_tick_watchpoint_read_date(uintptr_t field)
{
    if (field == 0u || !memory_range_readable((void*)field, sizeof(uint32_t))) {
        return 0u;
    }

    uint32_t year = *(uint16_t*)field;
    uint32_t day = *(uint8_t*)(field + 2u);
    uint32_t month = *(uint8_t*)(field + 3u);
    return year * 10000u + month * 100u + day;
}

static LONG CALLBACK kbo_current_date_tick_watchpoint_exception_handler(
    EXCEPTION_POINTERS* exception_info)
{
#if defined(_M_X64) || defined(__x86_64__)
    if (exception_info == NULL
            || exception_info->ExceptionRecord == NULL
            || exception_info->ContextRecord == NULL
            || exception_info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    CONTEXT* context = exception_info->ContextRecord;
    if ((context->Dr6 & 0x1u) == 0u) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    context->Dr6 = 0u;
    uintptr_t field = g_kbo_current_date_tick_watchpoint_address;
    uint32_t date = kbo_current_date_tick_watchpoint_read_date(field);
    int published = kbo_current_date_tick_publish(
        date,
        KBO_CURRENT_DATE_TICK_WATCHPOINT_SITE_RVA);

    g_kbo_current_date_tick_watchpoint_last_rip = (uintptr_t)context->Rip;
    InterlockedExchange(&g_kbo_current_date_tick_watchpoint_last_date, (LONG)date);
    InterlockedIncrement(&g_kbo_current_date_tick_watchpoint_hits);
    if (published) {
        InterlockedIncrement(&g_kbo_current_date_tick_watchpoint_published);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
#else
    (void)exception_info;
    return EXCEPTION_CONTINUE_SEARCH;
#endif
}

static int kbo_current_date_tick_watchpoint_set_thread(DWORD thread_id, uintptr_t field)
{
#if defined(_M_X64) || defined(__x86_64__)
    if (field == 0u || thread_id == GetCurrentThreadId()) {
        return 0;
    }

    HANDLE thread = OpenThread(
        THREAD_SUSPEND_RESUME
            | THREAD_GET_CONTEXT
            | THREAD_SET_CONTEXT
            | THREAD_QUERY_INFORMATION,
        FALSE,
        thread_id);
    if (thread == NULL) {
        return 0;
    }

    DWORD suspend_result = SuspendThread(thread);
    if (suspend_result == (DWORD)-1) {
        CloseHandle(thread);
        return 0;
    }

    CONTEXT context;
    ZeroMemory(&context, sizeof(context));
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    int ok = 0;
    if (GetThreadContext(thread, &context)) {
        context.Dr0 = (DWORD64)field;
        context.Dr6 = 0u;
        context.Dr7 &= ~(DWORD64)0x000F0003u;
        context.Dr7 |= (DWORD64)0x000D0001u; /* local DR0, write, 4 bytes */
        ok = SetThreadContext(thread, &context) ? 1 : 0;
    }

    ResumeThread(thread);
    CloseHandle(thread);
    if (ok) {
        InterlockedIncrement(&g_kbo_current_date_tick_watchpoint_context_sets);
    }
    return ok;
#else
    (void)thread_id;
    (void)field;
    return 0;
#endif
}

static int kbo_current_date_tick_watchpoint_set_all_threads(uintptr_t field)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD current_process_id = GetCurrentProcessId();
    int set_count = 0;
    THREADENTRY32 entry;
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == current_process_id) {
                set_count += kbo_current_date_tick_watchpoint_set_thread(
                    entry.th32ThreadID,
                    field);
            }
            entry.dwSize = sizeof(entry);
        } while (Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return set_count;
}

static DWORD WINAPI kbo_current_date_tick_watchpoint_thread(LPVOID parameter)
{
    (void)parameter;

    if (g_kbo_current_date_tick_watchpoint_handler == NULL) {
        g_kbo_current_date_tick_watchpoint_handler =
            AddVectoredExceptionHandler(
                1,
                kbo_current_date_tick_watchpoint_exception_handler);
    }
    if (g_kbo_current_date_tick_watchpoint_handler == NULL) {
        kbo_log_runtime_line("KBO current date tick watchpoint skipped reason=veh_failed");
        InterlockedExchange(&g_kbo_current_date_tick_watchpoint_started, 0);
        return 0;
    }

    kbo_log_runtime_line("KBO current date tick watchpoint thread started");
    uintptr_t last_logged_field = 0u;
    LONG last_logged_hits = 0;
    while (kbo_runtime_threads_should_continue()) {
        uintptr_t field = kbo_current_date_tick_watchpoint_live_field_address();
        if (field != 0u) {
            g_kbo_current_date_tick_watchpoint_address = field;
            int set_count = kbo_current_date_tick_watchpoint_set_all_threads(field);
            if (field != last_logged_field) {
                last_logged_field = field;
                kbo_log_runtimef(
                    "KBO current date tick watchpoint armed field=%p threads=%d",
                    (void*)field,
                    set_count);
            }
        }

        LONG hits = InterlockedCompareExchange(
            &g_kbo_current_date_tick_watchpoint_hits,
            0,
            0);
        if (hits != last_logged_hits) {
            last_logged_hits = hits;
            LONG published = InterlockedCompareExchange(
                &g_kbo_current_date_tick_watchpoint_published,
                0,
                0);
            LONG date = InterlockedCompareExchange(
                &g_kbo_current_date_tick_watchpoint_last_date,
                0,
                0);
            kbo_log_runtimef(
                "KBO current date tick watchpoint hits=%ld published=%ld field=%p rip=%p date=%ld contexts=%ld",
                (long)hits,
                (long)published,
                (void*)g_kbo_current_date_tick_watchpoint_address,
                (void*)g_kbo_current_date_tick_watchpoint_last_rip,
                (long)date,
                (long)InterlockedCompareExchange(
                    &g_kbo_current_date_tick_watchpoint_context_sets,
                    0,
                    0));
        }

        if (!kbo_runtime_sleep_should_continue(500u)) {
            break;
        }
    }

    InterlockedExchange(&g_kbo_current_date_tick_watchpoint_started, 0);
    kbo_log_runtime_line("KBO current date tick watchpoint thread stopped");
    return 0;
}

void start_kbo_current_date_tick_watchpoint_thread(void)
{
    if (InterlockedCompareExchange(
            &g_kbo_current_date_tick_watchpoint_started,
            1,
            0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(
            kbo_current_date_tick_watchpoint_thread,
            NULL,
            "current date tick watchpoint")) {
        InterlockedExchange(&g_kbo_current_date_tick_watchpoint_started, 0);
    }
}
