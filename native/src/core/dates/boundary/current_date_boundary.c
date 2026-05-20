#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "current_date_boundary.h"
#include "../core_text_date.h"
#include "../../files/save_paths/core_save_paths.h"
#include "../../logging/core_log.h"

static SRWLOCK g_kbo_date_boundary_lock = SRWLOCK_INIT;
static KboDateBoundaryContext g_kbo_date_boundary_latest_context;
static int g_kbo_date_boundary_latest_valid = 0;
static char g_kbo_date_boundary_save_path[MAX_PATH];
static volatile LONG g_kbo_date_boundary_save_epoch = 1;
static volatile LONG g_kbo_date_boundary_log_count = 0;

static const char* kbo_date_boundary_log_label(const char* label)
{
    return label != NULL && label[0] != '\0' ? label : "date_boundary";
}

static int kbo_date_boundary_log_allowed(void)
{
    LONG index = InterlockedIncrement(&g_kbo_date_boundary_log_count);
    return index <= 80 || (index % 500) == 0;
}

const char* kbo_date_boundary_source_label(uint32_t source_kind)
{
    switch (source_kind) {
    case KBO_DATE_BOUNDARY_SOURCE_LIVE_POST_ADVANCE:
        return "live_post_advance";
    case KBO_DATE_BOUNDARY_SOURCE_SAVE_ENTER:
        return "save_enter";
    case KBO_DATE_BOUNDARY_SOURCE_WATCHPOINT:
        return "watchpoint";
    default:
        return "unknown";
    }
}

uint32_t kbo_date_boundary_current_save_epoch(void)
{
    LONG epoch = InterlockedCompareExchange(&g_kbo_date_boundary_save_epoch, 0, 0);
    return epoch > 0 ? (uint32_t)epoch : 0u;
}

static int kbo_date_boundary_current_save_path(char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    return kbo_get_current_save_path(out, out_size) && out[0] != '\0';
}

int kbo_date_boundary_refresh_save_scope(
    const char* label,
    uint32_t* out_save_epoch,
    int* out_save_scope_known)
{
    if (out_save_epoch != NULL) {
        *out_save_epoch = kbo_date_boundary_current_save_epoch();
    }
    if (out_save_scope_known != NULL) {
        *out_save_scope_known = 0;
    }

    char save_path[MAX_PATH] = {0};
    int known = kbo_date_boundary_current_save_path(save_path, sizeof(save_path));
    int changed = 0;

    AcquireSRWLockExclusive(&g_kbo_date_boundary_lock);
    if (known) {
        if (strcmp(g_kbo_date_boundary_save_path, save_path) != 0) {
            snprintf(
                g_kbo_date_boundary_save_path,
                sizeof(g_kbo_date_boundary_save_path),
                "%s",
                save_path);
            LONG epoch = InterlockedIncrement(&g_kbo_date_boundary_save_epoch);
            if (epoch <= 0) {
                epoch = InterlockedExchange(&g_kbo_date_boundary_save_epoch, 1);
                epoch = epoch <= 0 ? 1 : epoch;
            }
            changed = 1;
        }
    }

    uint32_t epoch = kbo_date_boundary_current_save_epoch();
    int scope_known = g_kbo_date_boundary_save_path[0] != '\0';
    ReleaseSRWLockExclusive(&g_kbo_date_boundary_lock);

    if (out_save_epoch != NULL) {
        *out_save_epoch = epoch;
    }
    if (out_save_scope_known != NULL) {
        *out_save_scope_known = scope_known;
    }

    if (changed && kbo_date_boundary_log_allowed()) {
        kbo_log_runtimef(
            "KBO date boundary save scope changed label=\"%s\" epoch=%u save=%s",
            kbo_date_boundary_log_label(label),
            epoch,
            save_path);
    }
    return changed;
}

void kbo_date_boundary_reset(const char* label, const char* reason)
{
    AcquireSRWLockExclusive(&g_kbo_date_boundary_lock);
    g_kbo_date_boundary_latest_context = (KboDateBoundaryContext){0};
    g_kbo_date_boundary_latest_valid = 0;
    g_kbo_date_boundary_save_path[0] = '\0';
    LONG epoch = InterlockedIncrement(&g_kbo_date_boundary_save_epoch);
    if (epoch <= 0) {
        InterlockedExchange(&g_kbo_date_boundary_save_epoch, 1);
        epoch = 1;
    }
    ReleaseSRWLockExclusive(&g_kbo_date_boundary_lock);

    if (kbo_date_boundary_log_allowed()) {
        kbo_log_runtimef(
            "KBO date boundary reset label=\"%s\" reason=%s epoch=%u",
            kbo_date_boundary_log_label(label),
            reason != NULL && reason[0] != '\0' ? reason : "unspecified",
            (uint32_t)epoch);
    }
}

static int kbo_date_boundary_context_is_trusted(
    uint32_t date,
    uint32_t previous_date,
    uint32_t expected_next_date,
    uint32_t source_kind)
{
    if (!kbo_yyyymmdd_valid(date)) {
        return 0;
    }
    if (previous_date == 0u) {
        return 1;
    }
    if (source_kind == KBO_DATE_BOUNDARY_SOURCE_SAVE_ENTER) {
        return 0;
    }
    return expected_next_date != 0u && date == expected_next_date;
}

int kbo_date_boundary_record_accept(
    uint32_t date,
    uint32_t previous_date,
    uint32_t expected_next_date,
    uint32_t sequence,
    uint32_t site_rva,
    uint32_t source_kind,
    uint32_t base_flags,
    KboDateBoundaryContext* out_context)
{
    uint32_t save_epoch = 0u;
    int save_scope_known = 0;
    int save_scope_changed = kbo_date_boundary_refresh_save_scope(
        kbo_date_boundary_source_label(source_kind),
        &save_epoch,
        &save_scope_known);

    KboDateBoundaryContext context = {
        .date = date,
        .previous_date = previous_date,
        .expected_next_date = expected_next_date,
        .sequence = sequence,
        .site_rva = site_rva,
        .source_kind = source_kind,
        .save_epoch = save_epoch,
        .flags = base_flags | KBO_DATE_BOUNDARY_FLAG_ACCEPTED
    };
    if (previous_date == 0u) {
        context.flags |= KBO_DATE_BOUNDARY_FLAG_FIRST_DATE;
    }
    if (save_scope_known) {
        context.flags |= KBO_DATE_BOUNDARY_FLAG_SAVE_SCOPE_KNOWN;
    }
    if (save_scope_changed) {
        context.flags |= KBO_DATE_BOUNDARY_FLAG_SAVE_SCOPE_CHANGED;
    }
    if (source_kind == KBO_DATE_BOUNDARY_SOURCE_SAVE_ENTER) {
        context.flags |= KBO_DATE_BOUNDARY_FLAG_SAVE_ENTER;
    }
    if (kbo_date_boundary_context_is_trusted(
            date,
            previous_date,
            expected_next_date,
            source_kind)) {
        context.flags |= KBO_DATE_BOUNDARY_FLAG_TRUSTED;
    }

    AcquireSRWLockExclusive(&g_kbo_date_boundary_lock);
    if (g_kbo_date_boundary_save_path[0] != '\0') {
        snprintf(context.save_path, sizeof(context.save_path), "%s", g_kbo_date_boundary_save_path);
    }
    g_kbo_date_boundary_latest_context = context;
    g_kbo_date_boundary_latest_valid = 1;
    ReleaseSRWLockExclusive(&g_kbo_date_boundary_lock);

    if (out_context != NULL) {
        *out_context = context;
    }

    if ((context.flags & (KBO_DATE_BOUNDARY_FLAG_FIRST_DATE
                    | KBO_DATE_BOUNDARY_FLAG_SAVE_SCOPE_CHANGED)) != 0u
            || source_kind != KBO_DATE_BOUNDARY_SOURCE_LIVE_POST_ADVANCE
            || (context.flags & KBO_DATE_BOUNDARY_FLAG_TRUSTED) == 0u) {
        if (kbo_date_boundary_log_allowed()) {
            kbo_log_runtimef(
                "KBO date boundary accepted date=%u previous=%u expected=%u seq=%u source=%s site=0x%x epoch=%u flags=0x%x",
                context.date,
                context.previous_date,
                context.expected_next_date,
                context.sequence,
                kbo_date_boundary_source_label(context.source_kind),
                context.site_rva,
                context.save_epoch,
                context.flags);
        }
    }
    return 1;
}

void kbo_date_boundary_record_reject(
    uint32_t date,
    uint32_t previous_date,
    uint32_t expected_next_date,
    uint32_t site_rva,
    uint32_t source_kind,
    uint32_t reason_flags)
{
    if (!kbo_date_boundary_log_allowed()) {
        return;
    }

    kbo_log_runtimef(
        "KBO date boundary rejected date=%u previous=%u expected=%u source=%s site=0x%x epoch=%u flags=0x%x",
        date,
        previous_date,
        expected_next_date,
        kbo_date_boundary_source_label(source_kind),
        site_rva,
        kbo_date_boundary_current_save_epoch(),
        reason_flags | KBO_DATE_BOUNDARY_FLAG_REJECTED);
}

int kbo_date_boundary_latest(KboDateBoundaryContext* out_context)
{
    if (out_context != NULL) {
        *out_context = (KboDateBoundaryContext){0};
    }
    if (out_context == NULL) {
        return 0;
    }

    AcquireSRWLockShared(&g_kbo_date_boundary_lock);
    int valid = g_kbo_date_boundary_latest_valid;
    if (valid) {
        *out_context = g_kbo_date_boundary_latest_context;
    }
    ReleaseSRWLockShared(&g_kbo_date_boundary_lock);
    return valid;
}
