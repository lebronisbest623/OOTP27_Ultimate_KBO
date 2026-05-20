#ifndef NATIVE_SRC_FOREIGN_INTL_ESTABLISHED_FA_POSTSCAN_INTL_ESTABLISHED_FA_POSTSCAN_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_INTL_ESTABLISHED_FA_POSTSCAN_INTL_ESTABLISHED_FA_POSTSCAN_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_flags/api/settings/economic/economic_defaults.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_player_policy.h"
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_DELAY_MS ((ULONGLONG)kbo_runtime_tuning_policy()->intl_established_fa_postscan_delay_ms)
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS ((ULONGLONG)kbo_runtime_tuning_policy()->intl_established_fa_postscan_retry_ms)
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_RETRIES (kbo_runtime_tuning_policy()->intl_established_fa_postscan_max_retries)
#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_DETAIL_LOGS (kbo_runtime_tuning_policy()->intl_established_fa_postscan_max_detail_logs)
enum {
    KBO_INTL_FA_POSTSCAN_IDLE = 0,
    KBO_INTL_FA_POSTSCAN_PENDING = 1,
    KBO_INTL_FA_POSTSCAN_RUNNING = 2,
};

typedef struct KboIntlEstablishedFaPostscanState {
    LONG pending;
    LONG batch_id;
    int32_t before_count;
    uint32_t before_max_player_id;
    int32_t original_count;
    int32_t expected_count;
    int multiplier;
    uint32_t primary_league_id;
    uint32_t fallback_league_id;
    uint32_t scheduled_date;
    ULONGLONG due_tick;
    int attempts;
} KboIntlEstablishedFaPostscanState;

typedef struct KboIntlEstablishedFaMarketNormalization {
    uint32_t before_original_league_id;
    uint32_t before_draft_league_id;
    uint8_t before_draft_class;
    uint8_t before_draft_subtype;
    uint8_t before_draft_eligible;
    uint8_t before_draft_extra;
    uint8_t before_contract_level;
    int32_t before_fa_demand;
    uint32_t after_original_league_id;
    uint32_t after_draft_league_id;
    uint8_t after_draft_class;
    uint8_t after_draft_subtype;
    uint8_t after_draft_eligible;
    uint8_t after_draft_extra;
    uint8_t after_contract_level;
    int32_t after_fa_demand;
    int changed;
    int original_league_seeded;
    int draft_league_cleared;
    int draft_fields_cleared;
    int contract_level_cleared;
    int demand_initialized;
    int market_ready;
} KboIntlEstablishedFaMarketNormalization;

extern KboIntlEstablishedFaPostscanState g_kbo_intl_established_fa_postscan;
extern volatile LONG g_kbo_intl_established_fa_postscan_worker_started;

int kbo_intl_established_fa_quality_shaping_enabled(void);
int kbo_intl_established_fa_pitcher_role_is_starter(uint8_t position_role);
int kbo_intl_established_fa_pitcher_role_is_bullpen(uint8_t position_role);
int kbo_intl_established_fa_position_is_catcher(uint8_t position_group, uint8_t position_role);
const char* kbo_intl_established_fa_quality_policy_label(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role);
int32_t kbo_intl_established_fa_quality_score_cap(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role);
int16_t kbo_intl_established_fa_quality_field_cap(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role);
int16_t kbo_intl_established_fa_scale_value_i16(int16_t value, int32_t original_score, int32_t cap);
int16_t kbo_intl_established_fa_clamp_value_i16(int16_t value, int16_t cap);
int kbo_intl_established_fa_apply_quality_cap(
    uint8_t* player,
    int32_t cap,
    int16_t field_cap,
    int32_t original_score,
    int32_t* out_adjusted_score);
int kbo_intl_established_fa_postscan_csv_empty(HANDLE file);
int kbo_intl_established_fa_postscan_open_csv(HANDLE* out_file, char* out_path, size_t out_path_size);
void kbo_intl_established_fa_postscan_write_csv_row(
    HANDLE file,
    const char* date,
    LONG batch_id,
    int32_t before_count,
    int32_t after_count,
    int32_t expected_count,
    int32_t index,
    uint8_t* player);
void kbo_intl_established_fa_postscan_schedule(
    int32_t original_count,
    int32_t expected_count,
    int multiplier,
    uint32_t primary_league_id,
    uint32_t fallback_league_id);
int kbo_intl_established_fa_postscan_persist_state(
    const KboIntlEstablishedFaPostscanState* state,
    const char* source);
int kbo_intl_established_fa_postscan_load_state(
    KboIntlEstablishedFaPostscanState* out,
    const char* source);
int kbo_intl_established_fa_postscan_batch_matches_event_date(
    const KboIntlEstablishedFaPostscanState* batch,
    uint32_t event_yyyymmdd);
int kbo_intl_established_fa_event_is_stale(
    uint32_t event_yyyymmdd,
    uint32_t today_yyyymmdd);
int kbo_intl_established_fa_postscan_candidate_matches(
    const KboIntlEstablishedFaPostscanState* batch,
    int32_t index,
    int32_t player_count,
    uint8_t* player);
int kbo_intl_established_fa_normalize_market_state(
    uint8_t* player,
    uint32_t primary_league_id,
    uint32_t fallback_league_id,
    int asian_quota,
    int32_t value_score,
    KboIntlEstablishedFaMarketNormalization* out);
void kbo_intl_established_fa_postscan_run(const KboIntlEstablishedFaPostscanState* batch);
void kbo_intl_established_fa_postscan_try_run(void);
int kbo_intl_established_fa_postscan_run_pending_now(
    uint32_t event_yyyymmdd,
    const char* source);
DWORD WINAPI kbo_intl_established_fa_postscan_thread(LPVOID parameter);
void start_kbo_intl_established_fa_postscan_thread(void);

#endif
