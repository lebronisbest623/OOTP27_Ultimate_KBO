#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_AI_ROSTER_DAILY_INTERNAL_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_AI_ROSTER_DAILY_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../lookup/team_lookup.h"
#include "../team_add_player_guard.h"

#define KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET 0xfe0u
#define KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET 0xfe4u
#define KBO_AI_ROSTER_FOREIGN_F25_MIN 100u
#define KBO_AI_ROSTER_DAILY_TRIED_MAX 64

typedef struct KboAiRosterDailyCandidateSummary {
    int32_t index;
    uintptr_t player_ptr;
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t league_id;
    uint32_t default_team_id;
    uint32_t status24;
    uint32_t status25;
    uint32_t status26;
    uint32_t f25;
    uint32_t f62;
    uint32_t f65;
    int16_t f06;
    int32_t score_fe0;
    int32_t score_fe4;
    int16_t overall;
    int16_t talent;
    int16_t ratings;
} KboAiRosterDailyCandidateSummary;

typedef struct KboAiRosterDailyCallupScan {
    int scanned;
    int foreign_seen;
    int eligible_seen;
    int blocked_limit;
    int skipped_status;
    int skipped_not_minor;
    int skipped_no_team;
} KboAiRosterDailyCallupScan;

void kbo_ai_roster_daily_fill_summary(KboAiRosterDailyCandidateSummary* summary, int32_t index, uint8_t* player);
int64_t kbo_ai_roster_daily_score(const KboAiRosterDailyCandidateSummary* summary, uint8_t* player);
int kbo_ai_roster_daily_minor_callup_allows(int32_t team_arg, uint8_t* player, uint32_t* out_team_id);

#endif
