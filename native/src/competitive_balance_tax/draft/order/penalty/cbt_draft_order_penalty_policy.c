#include "cbt_draft_order_penalty_policy.h"

#include <string.h>

#include "../../../records/cbt_records.h"
#include "../../../rules/cbt_rules.h"
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/league_roles/kbo_league_roles.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/lookup/team_lookup.h"

int kbo_cbt_draft_order_penalty_for_team(
    uint32_t team_id,
    KboCbtDraftPenaltyInfo* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (team_id == 0u) {
        return 0;
    }

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);

    KboCbtRecord records[KBO_CBT_RECORDS_MAX];
    int record_count = kbo_cbt_load_records(records, KBO_CBT_RECORDS_MAX, NULL, 0);
    if (record_count <= 0) {
        return 0;
    }

    int best_index = -1;
    uint32_t best_season = 0u;
    for (int i = 0; i < record_count; i++) {
        const KboCbtRecord* rec = &records[i];
        if (rec->team_id == team_id && rec->season > best_season) {
            best_index = i;
            best_season = rec->season;
        }
    }
    if (best_index < 0) {
        return 0;
    }

    const KboCbtRecord* rec = &records[best_index];
    if (rec->overage <= 0 || rec->consecutive_count < rules.draft_penalty_min_consecutive) {
        return 0;
    }

    if (out != NULL) {
        out->season = rec->season;
        out->team_id = team_id;
        out->stages = rules.draft_penalty_stages;
    }
    return rules.draft_penalty_stages > 0u;
}

int kbo_cbt_draft_order_team_is_main_kbo(uint32_t team_id)
{
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) == kbo_league_role_main_league_id();
}
