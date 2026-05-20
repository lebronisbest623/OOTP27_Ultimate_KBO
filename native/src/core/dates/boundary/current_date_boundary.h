#ifndef KBOFIX_SRC_CORE_DATES_BOUNDARY_CURRENT_DATE_BOUNDARY_H_
#define KBOFIX_SRC_CORE_DATES_BOUNDARY_CURRENT_DATE_BOUNDARY_H_

#include <stdint.h>
#include <windows.h>

#define KBO_DATE_BOUNDARY_SOURCE_UNKNOWN 0u
#define KBO_DATE_BOUNDARY_SOURCE_LIVE_POST_ADVANCE 1u
#define KBO_DATE_BOUNDARY_SOURCE_SAVE_ENTER 2u
#define KBO_DATE_BOUNDARY_SOURCE_WATCHPOINT 3u

#define KBO_DATE_BOUNDARY_FLAG_ACCEPTED 0x00000001u
#define KBO_DATE_BOUNDARY_FLAG_TRUSTED 0x00000002u
#define KBO_DATE_BOUNDARY_FLAG_FIRST_DATE 0x00000004u
#define KBO_DATE_BOUNDARY_FLAG_SAVE_SCOPE_KNOWN 0x00000008u
#define KBO_DATE_BOUNDARY_FLAG_SAVE_SCOPE_CHANGED 0x00000010u
#define KBO_DATE_BOUNDARY_FLAG_REJECTED 0x00000020u
#define KBO_DATE_BOUNDARY_FLAG_NON_ADJACENT 0x00000040u
#define KBO_DATE_BOUNDARY_FLAG_SAVE_ENTER 0x00000080u

typedef struct KboDateBoundaryContext {
    uint32_t date;
    uint32_t previous_date;
    uint32_t expected_next_date;
    uint32_t sequence;
    uint32_t site_rva;
    uint32_t source_kind;
    uint32_t save_epoch;
    uint32_t flags;
    char save_path[MAX_PATH];
} KboDateBoundaryContext;

const char* kbo_date_boundary_source_label(uint32_t source_kind);
uint32_t kbo_date_boundary_current_save_epoch(void);
int kbo_date_boundary_refresh_save_scope(
    const char* label,
    uint32_t* out_save_epoch,
    int* out_save_scope_known);
void kbo_date_boundary_reset(const char* label, const char* reason);
int kbo_date_boundary_record_accept(
    uint32_t date,
    uint32_t previous_date,
    uint32_t expected_next_date,
    uint32_t sequence,
    uint32_t site_rva,
    uint32_t source_kind,
    uint32_t base_flags,
    KboDateBoundaryContext* out_context);
void kbo_date_boundary_record_reject(
    uint32_t date,
    uint32_t previous_date,
    uint32_t expected_next_date,
    uint32_t site_rva,
    uint32_t source_kind,
    uint32_t reason_flags);
int kbo_date_boundary_latest(KboDateBoundaryContext* out_context);

#endif
