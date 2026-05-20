#ifndef KBOFIX_SRC_FOREIGN_QUOTA_FOREIGN_QUOTA_COUNTS_H_
#define KBOFIX_SRC_FOREIGN_QUOTA_FOREIGN_QUOTA_COUNTS_H_

#include <stdint.h>

void kbo_count_team_asian_quota_probe(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count);
void kbo_count_team_asian_quota_probe_fresh(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count);
uint32_t kbo_foreign_org_count_cache_generation(void);
uint32_t kbo_foreign_org_count_cache_generation_for_team(uint32_t team_id);
void kbo_foreign_org_count_cache_note_roster_mutation(void);
void kbo_foreign_org_count_cache_note_player_assignment_change(
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_loan_team_id,
    uint32_t after_current_team_id,
    uint32_t after_active_team_id,
    uint32_t after_loan_team_id,
    uint32_t player_id,
    int asian_quota);
void kbo_foreign_org_count_cache_note_observed_assignment_change(
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_loan_team_id,
    uint32_t after_current_team_id,
    uint32_t after_active_team_id,
    uint32_t after_loan_team_id);
#ifdef KBO_BENCHMARK_BUILD
void kbo_foreign_org_count_seed_benchmark_snapshot(
    uint32_t team_id,
    uint32_t foreign_count,
    uint32_t asian_count,
    uint32_t non_asian_count);
void kbo_foreign_org_count_refresh_benchmark_snapshot_tick(void);
#endif
uint32_t kbo_effective_foreign_count_with_asian_quota(
    uint32_t asian_count,
    uint32_t non_asian_foreign_count);
void kbo_count_active_asian_quota_by_position(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers);
void kbo_count_active_foreign_for_asian_quota(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers,
    uint32_t* out_non_asian_hitters,
    uint32_t* out_non_asian_pitchers);
int kbo_team_active_roster_contains_player(uintptr_t team_ptr, uint32_t player_id);

#endif
