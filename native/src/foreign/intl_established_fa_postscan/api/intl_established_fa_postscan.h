#ifndef KBOFIX_SRC_FOREIGN_INTL_ESTABLISHED_FA_POSTSCAN_H_
#define KBOFIX_SRC_FOREIGN_INTL_ESTABLISHED_FA_POSTSCAN_H_

#include <stdint.h>

void kbo_intl_established_fa_postscan_schedule(
    int32_t original_count,
    int32_t expected_count,
    int multiplier,
    uint32_t primary_league_id,
    uint32_t fallback_league_id);
void start_kbo_intl_established_fa_postscan_thread(void);
int kbo_handle_intl_established_fa_event(uint32_t event_yyyymmdd, const char* source);
int kbo_intl_established_fa_quality_shaping_enabled(void);
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
int kbo_intl_established_fa_apply_quality_cap(
    uint8_t* player,
    int32_t cap,
    int16_t field_cap,
    int32_t original_score,
    int32_t* out_adjusted_score);
void kbo_intl_established_fa_postscan_reset_observed_players(void);
void kbo_intl_established_fa_postscan_note_observed_player(uintptr_t player_ptr);
int kbo_intl_established_fa_postscan_observed_player_count(void);

#endif
