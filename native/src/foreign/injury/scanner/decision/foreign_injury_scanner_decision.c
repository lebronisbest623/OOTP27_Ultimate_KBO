#include "../foreign_injury_scanner_internal.h"

#include "../../../common/policy/foreign_player_policy.h"

#define KBO_FOREIGN_INJURY_DECISION_UNAVAILABLE_PENALTY 250000

static void kbo_foreign_injury_decision_set(
    KboForeignInjuryReplacementDecision* out,
    uint8_t choice,
    int32_t injured_score,
    int32_t replacement_score,
    int32_t required_margin,
    const char* reason)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->choice = choice;
    out->injured_score = injured_score;
    out->replacement_score = replacement_score;
    out->score_margin = replacement_score - injured_score;
    out->required_margin = required_margin;
    snprintf(out->reason, sizeof(out->reason), "%s", reason != NULL ? reason : "");
}

static int32_t kbo_foreign_injury_adjusted_player_score(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return -KBO_FOREIGN_INJURY_DECISION_UNAVAILABLE_PENALTY;
    }

    int32_t score = kbo_foreign_waiver_value_score(player);
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        score -= KBO_FOREIGN_INJURY_DECISION_UNAVAILABLE_PENALTY;
    }
    KboForeignInjuryLiveMemory live_injury;
    memset(&live_injury, 0, sizeof(live_injury));
    kbo_foreign_injury_read_live_memory(player, &live_injury);
    if (live_injury.active != 0u || live_injury.days_left > 0) {
        score -= KBO_FOREIGN_INJURY_DECISION_UNAVAILABLE_PENALTY;
    }
    return score;
}

const char* kbo_foreign_injury_decision_choice_label(uint8_t choice)
{
    switch (choice) {
    case KBO_FOREIGN_INJURY_DECISION_KEEP_REPLACEMENT:
        return "keep_replacement";
    case KBO_FOREIGN_INJURY_DECISION_KEEP_INJURED:
        return "keep_injured";
    default:
        return "unknown";
    }
}

int kbo_foreign_injury_choose_returning_player(
    const KboForeignInjuryReplacement* rec,
    uint8_t* injured,
    uint8_t* replacement,
    KboForeignInjuryReplacementDecision* out)
{
    int32_t required_margin = kbo_foreign_player_policy()->injury_replacement_decision_margin_min;
    int32_t injured_score = kbo_foreign_injury_adjusted_player_score(injured);
    int32_t replacement_score = kbo_foreign_injury_adjusted_player_score(replacement);

    if (rec == NULL || rec->team_id == 0u || rec->injured_player_id == 0u) {
        kbo_foreign_injury_decision_set(
            out,
            KBO_FOREIGN_INJURY_DECISION_KEEP_INJURED,
            injured_score,
            replacement_score,
            required_margin,
            "invalid_record");
        return 0;
    }

    if (replacement == NULL || !memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)) {
        kbo_foreign_injury_decision_set(
            out,
            KBO_FOREIGN_INJURY_DECISION_KEEP_INJURED,
            injured_score,
            replacement_score,
            required_margin,
            "replacement_unavailable");
        return 1;
    }
    if (!kbo_foreign_injury_replacement_player_attached_to_record(rec, replacement)) {
        kbo_foreign_injury_decision_set(
            out,
            KBO_FOREIGN_INJURY_DECISION_KEEP_INJURED,
            injured_score,
            replacement_score,
            required_margin,
            "replacement_detached");
        return 1;
    }
    if (injured == NULL || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
        kbo_foreign_injury_decision_set(
            out,
            KBO_FOREIGN_INJURY_DECISION_KEEP_REPLACEMENT,
            injured_score,
            replacement_score,
            required_margin,
            "injured_unavailable");
        return 1;
    }

    int injured_slot_ok = kbo_foreign_injury_candidate_matches_slot(injured, rec->slot_type);
    int replacement_slot_ok = kbo_foreign_injury_candidate_matches_slot(replacement, rec->slot_type);
    if (!replacement_slot_ok && injured_slot_ok) {
        kbo_foreign_injury_decision_set(
            out,
            KBO_FOREIGN_INJURY_DECISION_KEEP_INJURED,
            injured_score,
            replacement_score,
            required_margin,
            "replacement_slot_mismatch");
        return 1;
    }
    if (!injured_slot_ok && replacement_slot_ok) {
        kbo_foreign_injury_decision_set(
            out,
            KBO_FOREIGN_INJURY_DECISION_KEEP_REPLACEMENT,
            injured_score,
            replacement_score,
            required_margin,
            "injured_slot_mismatch");
        return 1;
    }

    if (replacement_score - injured_score >= required_margin) {
        kbo_foreign_injury_decision_set(
            out,
            KBO_FOREIGN_INJURY_DECISION_KEEP_REPLACEMENT,
            injured_score,
            replacement_score,
            required_margin,
            "replacement_score_margin");
        return 1;
    }

    kbo_foreign_injury_decision_set(
        out,
        KBO_FOREIGN_INJURY_DECISION_KEEP_INJURED,
        injured_score,
        replacement_score,
        required_margin,
        "injured_incumbent_margin");
    return 1;
}
