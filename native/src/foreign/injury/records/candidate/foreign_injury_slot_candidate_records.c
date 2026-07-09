#include "../../internal/foreign_injury_internal.h"

int kbo_team_has_foreign_injury_slot_for_candidate_locked(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (out_replacement_player_id != NULL) { *out_replacement_player_id = 0u; }
    if (team_id == 0u || slot_type == 0u || candidate_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (!kbo_team_ids_share_org(rec->team_id, team_id)
                || rec->slot_type != slot_type
                || !kbo_foreign_injury_status_uses_slot(rec->status)
                || !kbo_foreign_injury_record_has_minimum_injury_basis(rec)
                || rec->injured_player_id == candidate_player_id) {
            continue;
        }
        if (rec->replacement_player_id != 0u && rec->replacement_player_id != candidate_player_id) {
            continue;
        }
        if (kbo_foreign_injury_replacement_player_reserved_locked(candidate_player_id, rec)) {
            continue;
        }
        if (out_injured_player_id != NULL) {
            *out_injured_player_id = rec->injured_player_id;
        }
        if (out_replacement_player_id != NULL) {
            *out_replacement_player_id = rec->replacement_player_id;
        }
        return 1;
    }
    return 0;
}

int kbo_team_has_foreign_injury_slot_for_candidate(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    KBO_PROFILE_BEGIN(profile_foreign_injury_slot_candidate);
    int result = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements_shared();
    result = kbo_team_has_foreign_injury_slot_for_candidate_locked(
        team_id,
        slot_type,
        candidate_player_id,
        out_injured_player_id,
        out_replacement_player_id);
    kbo_unlock_foreign_injury_replacements_shared();
    KBO_PROFILE_END(profile_foreign_injury_slot_candidate, result
        ? "foreign_injury.slot_candidate.hit"
        : "foreign_injury.slot_candidate.miss");
    return result;
}

int kbo_team_has_foreign_injury_slot_for_candidate_type_any(
    uint32_t team_id,
    int allow_asian_slot,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id)
{
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (team_id == 0u) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_foreign_injury_slot_candidate_type_any);
    int result = 0;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;

    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements_shared();
    if (allow_asian_slot
            && kbo_team_has_foreign_injury_slot_locked(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA,
                &injured_player_id)) {
        slot_type = KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA;
        result = 1;
    }
    if (!result
            && kbo_team_has_foreign_injury_slot_locked(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_REGULAR,
                &injured_player_id)) {
        slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR;
        result = 1;
    }
    kbo_unlock_foreign_injury_replacements_shared();

    if (result) {
        if (out_slot_type != NULL) { *out_slot_type = slot_type; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
    }
    KBO_PROFILE_END(profile_foreign_injury_slot_candidate_type_any, result
        ? "foreign_injury.slot_candidate_type_any.hit"
        : "foreign_injury.slot_candidate_type_any.miss");
    return result;
}

int kbo_team_has_foreign_injury_slot_for_candidate_any(
    uint32_t team_id,
    int allow_asian_slot,
    uint32_t candidate_player_id,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (out_replacement_player_id != NULL) { *out_replacement_player_id = 0u; }
    if (team_id == 0u || candidate_player_id == 0u) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_foreign_injury_slot_candidate_any);
    int result = 0;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t replacement_player_id = 0u;

    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements_shared();
    if (allow_asian_slot
            && kbo_team_has_foreign_injury_slot_for_candidate_locked(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA,
                candidate_player_id,
                &injured_player_id,
                &replacement_player_id)) {
        slot_type = KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA;
        result = 1;
    }
    if (!result
            && kbo_team_has_foreign_injury_slot_for_candidate_locked(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_REGULAR,
                candidate_player_id,
                &injured_player_id,
                &replacement_player_id)) {
        slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR;
        result = 1;
    }
    kbo_unlock_foreign_injury_replacements_shared();

    if (result) {
        if (out_slot_type != NULL) { *out_slot_type = slot_type; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        if (out_replacement_player_id != NULL) { *out_replacement_player_id = replacement_player_id; }
    }
    KBO_PROFILE_END(profile_foreign_injury_slot_candidate_any, result
        ? "foreign_injury.slot_candidate_any.hit"
        : "foreign_injury.slot_candidate_any.miss");
    return result;
}
