#ifndef KBOFIX_CBT_SERVICE_TIME_PROBE_INTERNAL_H_
#define KBOFIX_CBT_SERVICE_TIME_PROBE_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "cbt_service_time_probe.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/teams/core_team_collect.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"

int kbo_cbt_probe_year_plausible(uint32_t value, uint32_t current_year);
int kbo_cbt_probe_service_year_plausible(uint32_t value, uint32_t current_year);
int kbo_cbt_probe_team_plausible(uint32_t value, uint32_t a, uint32_t b, uint32_t c);
int kbo_cbt_probe_team_in_set(uint32_t value, const uint32_t* team_ids, int team_count);
void kbo_cbt_probe_append_row_hex(char* out, size_t out_size, uintptr_t row, uint32_t byte_count);
void kbo_cbt_probe_dump_candidate_rows(
    uint8_t* player,
    uint32_t player_id,
    uint32_t player_year,
    uint32_t current_team_id,
    uint32_t active_team_id,
    uint32_t original_team_id,
    const uint32_t* team_ids,
    int team_count,
    uint32_t vector_offset,
    uintptr_t begin,
    uintptr_t end);
void kbo_cbt_probe_scan_player_inline_tables(
    uint8_t* player,
    uint32_t player_id,
    uint32_t player_year,
    const uint32_t* team_ids,
    int team_count,
    int* emitted_inline);

#endif
