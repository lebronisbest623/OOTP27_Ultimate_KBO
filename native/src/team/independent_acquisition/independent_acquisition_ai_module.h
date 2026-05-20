#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_INDEPENDENT_ACQUISITION_AI_MODULE_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_INDEPENDENT_ACQUISITION_AI_MODULE_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#include "ai/independent_acquisition_ai_internal.h"

extern volatile LONG g_kbo_runtime_date_stable_ready;
extern volatile LONG g_kbo_independent_acquisition_ai_last_processed_date;
extern volatile LONG g_kbo_independent_acquisition_ai_running;
extern char g_kbo_independent_acquisition_ai_cursor_save_path[MAX_PATH];

#define KBO_INDEPENDENT_ACQUISITION_AI_MAX_CATCHUP_DAYS 220

typedef struct KboIndependentAcquisitionSellerAvailability {
    int seed_rows;
    int unresolved_rows;
    int seller_count;
    int available_seller_count;
    int capped_sellers;
    int32_t seller_transfer_limit;
} KboIndependentAcquisitionSellerAvailability;

int kbo_independent_acquisition_window_active_silent(uint32_t today);
uint32_t kbo_independent_acquisition_next_date(uint32_t today);
uint32_t kbo_independent_acquisition_processed_date(void);
void kbo_independent_acquisition_mark_processed_date(uint32_t today, const char* source);
int kbo_independent_acquisition_has_pending_requests(uint32_t today);
KboIndependentAcquisitionSellerAvailability
kbo_independent_acquisition_collect_available_sellers_for_date(
    uint32_t today,
    KboIndependentFuturesTeamLeague* out_available_sellers,
    int max_available_sellers);

int kbo_run_independent_team_acquisition_ai_with_snapshot_for_date(
    uint32_t today,
    const uintptr_t* snapshot,
    int32_t player_count,
    const char* source,
    int* out_abort_for_save);

#endif
