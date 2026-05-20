#include "foreign_injury_scanner_internal.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"

#define KBO_PLAYER_CONTRACT_STATUS_FREE   1u
#define KBO_PLAYER_STATUS_RELEASED        1u
#define KBO_PLAYER_LOAN_CLEARED           1u

typedef struct KboForeignInjuryPlayerSnapshot {
    uint32_t current_team_id;
    uint32_t current_league_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint32_t default_team_id;
    uint8_t  status_flags;
    uint8_t  contract_level;
    uint32_t contract_status;
} KboForeignInjuryPlayerSnapshot;

/* Caller must have verified the player passes memory_range_readable for
 * OOTP27_PLAYER_SCAN_BYTES (0x1800), which covers every offset below. */
static void kbo_foreign_injury_snapshot_player(uint8_t* player, KboForeignInjuryPlayerSnapshot* out)
{
    out->current_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    out->current_league_id  = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    out->active_team_id     = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    out->original_team_id   = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    out->original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    out->default_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    out->status_flags       = player[OOTP27_PLAYER_STATUS_FLAGS_OFFSET];
    out->contract_level     = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    out->contract_status    = *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET);
}

static int kbo_foreign_injury_player_already_released(
    const KboForeignInjuryPlayerSnapshot* s,
    uint32_t team_id)
{
    return s->current_team_id == 0u
        && s->active_team_id == 0u
        && s->original_team_id != team_id
        && s->default_team_id != team_id
        && s->contract_level == 0u
        && s->contract_status <= KBO_PLAYER_CONTRACT_STATUS_FREE
        && s->current_league_id == 0u
        && s->status_flags == KBO_PLAYER_STATUS_RELEASED;
}

static int kbo_foreign_injury_player_on_different_team(
    const KboForeignInjuryPlayerSnapshot* s,
    uint32_t team_id,
    int non_kbo_assignment)
{
    return s->current_team_id != 0u
        && s->current_team_id != team_id
        && s->active_team_id != team_id
        && s->original_team_id != team_id
        && !non_kbo_assignment;
}

static int kbo_foreign_injury_detect_non_kbo_assignment(
    const KboForeignInjuryPlayerSnapshot* s,
    uint32_t team_id,
    uint32_t* out_current_team_league_id)
{
    *out_current_team_league_id = 0u;
    if (s->current_team_id == 0u || s->current_team_id == team_id) {
        return 0;
    }
    uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(s->current_team_id, 1);
    if (current_team == NULL || !memory_range_readable(current_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    uint32_t league_id = *(uint32_t*)(current_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    *out_current_team_league_id = league_id;
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    return kbo_league_id != 0u
        && league_id != 0u
        && league_id != kbo_league_id;
}

static int kbo_foreign_injury_remove_player_from_team(uint32_t team_id, uint32_t player_id)
{
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    return kbo_remove_player_id_from_known_team_roster_arrays(team, player_id);
}

static void kbo_foreign_injury_clear_player_team_slots(
    uint8_t* player,
    uint32_t team_id,
    const KboForeignInjuryPlayerSnapshot* s,
    int non_kbo_assignment)
{
    if (s->current_team_id == team_id
            || s->active_team_id == team_id
            || s->original_team_id == team_id
            || non_kbo_assignment
            || (s->current_team_id == 0u && s->current_league_id != 0u)) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = 0u;
    }
    if (s->active_team_id == team_id
            || s->current_team_id == team_id
            || s->original_team_id == team_id
            || non_kbo_assignment) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) == team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = 0u;
    }
    uint32_t default_team = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    if (default_team == team_id || default_team == s->current_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = 0u;
    }
}

static void kbo_foreign_injury_clear_player_loan_if_matches(uint8_t* player, uint32_t team_id)
{
    if (*(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) != team_id) {
        return;
    }
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0u;
    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_LOAN_CLEARED_MARKER_OFFSET] = KBO_PLAYER_LOAN_CLEARED;
}

static void kbo_foreign_injury_clear_contract_for_market(uint8_t* player)
{
    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 0u;
    *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET) = KBO_PLAYER_CONTRACT_STATUS_FREE;
    *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 0u;
    int32_t* salaries = (int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    for (uint32_t i = 0; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        salaries[i] = 0;
    }
}

static void kbo_foreign_injury_emit_restore_audit(
    const char* source,
    uint32_t team_id,
    uint32_t player_id,
    int added_arrays,
    const KboForeignInjuryPlayerSnapshot* before)
{
    KboLogFields fields;
    kbo_log_fields_init(&fields);
    kbo_log_field_u32(&fields, "team_id", team_id);
    kbo_log_field_u32(&fields, "player_id", player_id);
    kbo_log_field_i32(&fields, "added_arrays", added_arrays);
    kbo_log_field_u32(&fields, "before_current_team", before->current_team_id);
    kbo_log_field_u32(&fields, "before_active_team", before->active_team_id);
    kbo_log_field_u32(&fields, "before_original_team", before->original_team_id);
    kbo_rule_audit_emit_fields(
        "foreign_injury.replacement.player",
        "restore_active_replacement",
        "active_slot_player_off_roster",
        source,
        &fields);
}

static void kbo_foreign_injury_emit_release_audit(
    const char* source,
    const char* role,
    const char* audit_action,
    const char* audit_reason,
    uint32_t team_id,
    uint32_t player_id,
    int removed_arrays,
    const KboForeignInjuryPlayerSnapshot* before,
    int non_kbo_repair)
{
    KboLogFields fields;
    kbo_log_fields_init(&fields);
    kbo_log_field_str(&fields, "player_role", role);
    kbo_log_field_u32(&fields, "team_id", team_id);
    kbo_log_field_u32(&fields, "player_id", player_id);
    kbo_log_field_i32(&fields, "removed_arrays", removed_arrays);
    kbo_log_field_u32(&fields, "before_current_team", before->current_team_id);
    kbo_log_field_u32(&fields, "before_active_team", before->active_team_id);
    kbo_log_field_u32(&fields, "before_original_team", before->original_team_id);
    kbo_log_field_i32(&fields, "non_kbo_repair", non_kbo_repair);
    kbo_rule_audit_emit_fields(
        "foreign_injury.replacement.player",
        audit_action != NULL && audit_action[0] != '\0' ? audit_action : "release_player",
        audit_reason != NULL && audit_reason[0] != '\0' ? audit_reason : "foreign_injury_decision",
        source,
        &fields);
}

int kbo_foreign_injury_restore_active_replacement_player(const KboForeignInjuryReplacement* rec, const char* source)
{
    if (rec == NULL
            || rec->status != KBO_FOREIGN_INJURY_STATUS_ACTIVE
            || rec->team_id == 0u
            || rec->replacement_player_id == 0u) {
        return 0;
    }

    uint8_t* player = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    KboForeignInjuryPlayerSnapshot before;
    kbo_foreign_injury_snapshot_player(player, &before);

    if (kbo_foreign_injury_replacement_player_attached_to_record(rec, player)) {
        return 0;
    }
    if (!kbo_foreign_injury_replacement_player_can_restore_to_record(rec, player)) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(rec->team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t league_id = rec->league_id != 0u
        ? rec->league_id
        : *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);

    int added = kbo_add_player_id_to_team_assignment_arrays(team, rec->replacement_player_id);
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = rec->team_id;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = rec->team_id;
    *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = rec->team_id;
    if (*(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == rec->team_id) {
        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0u;
    }
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;

    kbo_log_runtimef(
        "foreign injury replacement: restored active replacement source=%s team=%u player=%u added_arrays=%d before_current=%u before_active=%u before_original=%u",
        source != NULL ? source : "",
        rec->team_id,
        rec->replacement_player_id,
        added,
        before.current_team_id,
        before.active_team_id,
        before.original_team_id);
    kbo_foreign_injury_emit_restore_audit(
        source, rec->team_id, rec->replacement_player_id, added, &before);
    return 1;
}

static int kbo_foreign_injury_release_player_from_team(
    uint32_t team_id,
    uint32_t player_id,
    const char* source,
    const char* player_role,
    const char* audit_action,
    const char* audit_reason)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }
    const char* role = player_role != NULL && player_role[0] != '\0'
        ? player_role
        : "player";

    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    KboForeignInjuryPlayerSnapshot before;
    kbo_foreign_injury_snapshot_player(player, &before);

    if (kbo_foreign_injury_player_already_released(&before, team_id)) {
        return 0;
    }

    uint32_t current_team_league_id = 0u;
    int non_kbo_assignment = kbo_foreign_injury_detect_non_kbo_assignment(
        &before, team_id, &current_team_league_id);

    if (kbo_foreign_injury_player_on_different_team(&before, team_id, non_kbo_assignment)) {
        return 0;
    }

    int removed = kbo_foreign_injury_remove_player_from_team(team_id, player_id);
    if (before.current_team_id != 0u && before.current_team_id != team_id) {
        removed += kbo_foreign_injury_remove_player_from_team(before.current_team_id, player_id);
    }

    kbo_foreign_injury_clear_player_team_slots(player, team_id, &before, non_kbo_assignment);
    kbo_foreign_injury_clear_player_loan_if_matches(player, team_id);
    kbo_foreign_injury_clear_contract_for_market(player);
    player[OOTP27_PLAYER_STATUS_FLAGS_OFFSET] = KBO_PLAYER_STATUS_RELEASED;
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;

    kbo_log_runtimef(
        "foreign injury replacement: released %s source=%s team=%u player=%u removed_arrays=%d before_current=%u before_active=%u before_original=%u before_league=%u current_team_league=%u release_league=0 before_original_league=%u before_default=%u old_status41=%u old_contract_level=%u old_contract_status=%u non_kbo_repair=%d market=1",
        role,
        source != NULL ? source : "",
        team_id,
        player_id,
        removed,
        before.current_team_id,
        before.active_team_id,
        before.original_team_id,
        before.current_league_id,
        current_team_league_id,
        before.original_league_id,
        before.default_team_id,
        (uint32_t)before.status_flags,
        (uint32_t)before.contract_level,
        before.contract_status,
        non_kbo_assignment);
    kbo_foreign_injury_emit_release_audit(
        source, role, audit_action, audit_reason,
        team_id, player_id, removed, &before, non_kbo_assignment);
    return 1;
}

int kbo_foreign_injury_release_replacement_player(uint32_t team_id, uint32_t player_id, const char* source)
{
    return kbo_foreign_injury_release_player_from_team(
        team_id,
        player_id,
        source,
        "replacement",
        "release_replacement",
        "injured_player_retained");
}

int kbo_foreign_injury_release_injured_player(uint32_t team_id, uint32_t player_id, const char* source)
{
    return kbo_foreign_injury_release_player_from_team(
        team_id,
        player_id,
        source,
        "injured",
        "release_injured",
        "replacement_retained");
}
