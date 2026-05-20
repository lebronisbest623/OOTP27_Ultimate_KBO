#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_IO_CACHE_INDEPENDENT_ACQUISITION_DECISION_CACHE_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_IO_CACHE_INDEPENDENT_ACQUISITION_DECISION_CACHE_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>
#include <stdint.h>

typedef struct KboIndependentAcquisitionDecisionRecord {
    uint32_t date;
    uint32_t season;
    uint32_t seller_team_id;
    uint32_t buyer_team_id;
    uint32_t player_id;
    uint32_t transferred;
} KboIndependentAcquisitionDecisionRecord;

typedef struct KboIndependentAcquisitionDecisionCache {
    char path[MAX_PATH];
    FILETIME last_write_time;
    DWORD file_size;
    int has_file;
    int valid;
    KboIndependentAcquisitionDecisionRecord* records;
    int count;
    int capacity;
} KboIndependentAcquisitionDecisionCache;

extern CRITICAL_SECTION g_kbo_independent_acquisition_decision_cache_lock;
extern KboIndependentAcquisitionDecisionCache g_kbo_independent_acquisition_decision_cache;

void kbo_independent_acquisition_decision_cache_lock_init(void);
int kbo_independent_acquisition_decision_cache_ensure_locked(void);
int kbo_independent_acquisition_decision_cache_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id,
    int* out_exists);
int kbo_independent_acquisition_decision_cache_transferred_count(
    uint32_t season,
    uint32_t team_id,
    int seller_side,
    int* out_count);
int kbo_independent_acquisition_decision_cache_last_transfer_date(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t* out_last_date);

int kbo_independent_acquisition_decision_path(char* out, size_t out_size);
int kbo_independent_acquisition_json_u32(const char* line, const char* key, uint32_t* out);
int kbo_independent_acquisition_parse_decision_line(
    const char* line,
    uint32_t* out_season,
    uint32_t* out_seller_team_id,
    uint32_t* out_player_id,
    uint32_t* out_transferred);

#endif
