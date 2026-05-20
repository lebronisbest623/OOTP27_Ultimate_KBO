#include "foreign_injury_scanner_internal.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"

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

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    }
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

    uint32_t league_id = rec->league_id;
    if (league_id == 0u) {
        league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    }
    int added = kbo_add_player_id_to_team_assignment_arrays(team, rec->replacement_player_id);
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = rec->team_id;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = rec->team_id;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = rec->team_id;
    }
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
        current_team_id,
        active_team_id,
        original_team_id);
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "team_id", rec->team_id);
        kbo_log_field_u32(&audit_fields, "player_id", rec->replacement_player_id);
        kbo_log_field_i32(&audit_fields, "added_arrays", added);
        kbo_log_field_u32(&audit_fields, "before_current_team", current_team_id);
        kbo_log_field_u32(&audit_fields, "before_active_team", active_team_id);
        kbo_log_field_u32(&audit_fields, "before_original_team", original_team_id);
        kbo_rule_audit_emit_fields(
            "foreign_injury.replacement.player",
            "restore_active_replacement",
            "active_slot_player_off_roster",
            source,
            &audit_fields);
    } while (0);
    return 1;
}

static void kbo_foreign_injury_clear_contract_for_market(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET, sizeof(uint32_t))) {
        *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET) = 1u;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET, sizeof(uint32_t))) {
        *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 0u;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET, OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t))) {
        int32_t* salaries = (int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
        for (uint32_t i = 0; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
            salaries[i] = 0;
        }
    }
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

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = 0u;
    uint32_t original_league_id = 0u;
    uint32_t default_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    }
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    uint8_t old_status_flags = player[OOTP27_PLAYER_STATUS_FLAGS_OFFSET];
    uint8_t old_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    uint32_t old_contract_status = memory_range_readable(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET)
        : 0u;

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    uint32_t release_league_id = 0u;
    uint32_t current_team_league_id = 0u;
    int current_non_kbo_assignment = 0;

    if (current_team_id == 0u
            && active_team_id == 0u
            && original_team_id != team_id
            && default_team_id != team_id
            && old_contract_level == 0u
            && old_contract_status <= 1u
            && current_league_id == release_league_id
            && player[OOTP27_PLAYER_STATUS_FLAGS_OFFSET] == 1u) {
        return 0;
    }

    if (current_team_id != 0u && current_team_id != team_id) {
        uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 1);
        if (current_team != NULL && memory_range_readable(current_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            current_team_league_id = *(uint32_t*)(current_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
            current_non_kbo_assignment = kbo_league_id != 0u
                && current_team_league_id != 0u
                && current_team_league_id != kbo_league_id;
        }
    }

    if (current_team_id != 0u
            && current_team_id != team_id
            && active_team_id != team_id
            && original_team_id != team_id
            && !current_non_kbo_assignment) {
        return 0;
    }

    int removed = 0;
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        removed = kbo_remove_player_id_from_known_team_roster_arrays(team, player_id);
    }
    if (current_team_id != 0u && current_team_id != team_id) {
        uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 1);
        if (current_team != NULL && memory_range_readable(current_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            removed += kbo_remove_player_id_from_known_team_roster_arrays(current_team, player_id);
        }
    }

    if (current_team_id == team_id
            || active_team_id == team_id
            || original_team_id == team_id
            || current_non_kbo_assignment
            || (current_team_id == 0u && current_league_id != release_league_id)) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = release_league_id;
    }
    if (active_team_id == team_id || current_team_id == team_id || original_team_id == team_id || current_non_kbo_assignment) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0u;
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0u;
        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0u;
        player[OOTP27_PLAYER_LOAN_CLEARED_MARKER_OFFSET] = 1u;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))
            && *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) == team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = 0u;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            && (*(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) == team_id
                || *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) == current_team_id)) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = 0u;
    }
    kbo_foreign_injury_clear_contract_for_market(player);
    player[OOTP27_PLAYER_STATUS_FLAGS_OFFSET] = 1u;
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;

    kbo_log_runtimef(
        "foreign injury replacement: released %s source=%s team=%u player=%u removed_arrays=%d before_current=%u before_active=%u before_original=%u before_league=%u current_team_league=%u release_league=%u before_original_league=%u before_default=%u old_status41=%u old_contract_level=%u old_contract_status=%u non_kbo_repair=%d market=1",
        role,
        source != NULL ? source : "",
        team_id,
        player_id,
        removed,
        current_team_id,
        active_team_id,
        original_team_id,
        current_league_id,
        current_team_league_id,
        release_league_id,
        original_league_id,
        default_team_id,
        (uint32_t)old_status_flags,
        (uint32_t)old_contract_level,
        old_contract_status,
        current_non_kbo_assignment);
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_str(&audit_fields, "player_role", role);
        kbo_log_field_u32(&audit_fields, "team_id", team_id);
        kbo_log_field_u32(&audit_fields, "player_id", player_id);
        kbo_log_field_i32(&audit_fields, "removed_arrays", removed);
        kbo_log_field_u32(&audit_fields, "before_current_team", current_team_id);
        kbo_log_field_u32(&audit_fields, "before_active_team", active_team_id);
        kbo_log_field_u32(&audit_fields, "before_original_team", original_team_id);
        kbo_log_field_i32(&audit_fields, "non_kbo_repair", current_non_kbo_assignment);
        kbo_rule_audit_emit_fields(
            "foreign_injury.replacement.player",
            audit_action != NULL && audit_action[0] != '\0' ? audit_action : "release_player",
            audit_reason != NULL && audit_reason[0] != '\0' ? audit_reason : "foreign_injury_decision",
            source,
            &audit_fields);
    } while (0);
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
