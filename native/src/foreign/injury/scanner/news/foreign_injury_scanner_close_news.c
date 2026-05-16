#include "../foreign_injury_scanner_internal.h"

void kbo_foreign_injury_emit_closed_news_batch(
    const KboForeignInjuryClosedNews* closed_news,
    int closed_count,
    uint32_t today,
    const char* source)
{
    if (closed_news == NULL || closed_count <= 0) {
        return;
    }

    for (int i = 0; i < closed_count; i++) {
        const KboForeignInjuryReplacement* rec = &closed_news[i].rec;
        const KboForeignInjuryReplacementDecision* decision = &closed_news[i].decision;
        const char* close_phase = closed_news[i].phase[0] != '\0' ? closed_news[i].phase : "closed";
        kbo_emit_foreign_injury_replacement_news_on_date(rec, 0, close_phase, today);
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_log_field_u32(&audit_fields, "team_id", rec->team_id);
            kbo_log_field_u32(&audit_fields, "league_id", rec->league_id);
            kbo_log_field_u32(&audit_fields, "injured_player_id", rec->injured_player_id);
            kbo_log_field_u32(&audit_fields, "replacement_player_id", rec->replacement_player_id);
            kbo_log_field_str(&audit_fields, "decision", kbo_foreign_injury_decision_choice_label(decision->choice));
            kbo_log_field_str(&audit_fields, "decision_reason", decision->reason);
            kbo_log_field_i32(&audit_fields, "injured_score", decision->injured_score);
            kbo_log_field_i32(&audit_fields, "replacement_score", decision->replacement_score);
            kbo_log_field_i32(&audit_fields, "score_margin", decision->score_margin);
            kbo_log_field_i32(&audit_fields, "required_margin", decision->required_margin);
            kbo_log_field_u32(&audit_fields, "retained_player_id",
                decision->choice == KBO_FOREIGN_INJURY_DECISION_KEEP_REPLACEMENT
                    ? rec->replacement_player_id
                    : rec->injured_player_id);
            kbo_log_field_u32(&audit_fields, "released_player_id",
                decision->choice == KBO_FOREIGN_INJURY_DECISION_KEEP_REPLACEMENT
                    ? rec->injured_player_id
                    : rec->replacement_player_id);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.lifecycle",
                "close_slot",
                decision->choice == KBO_FOREIGN_INJURY_DECISION_KEEP_REPLACEMENT
                    ? "replacement_retained"
                    : "injured_player_retained",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign injury replacement: closed source=%s team=%u injured=%u replacement=%u league=%u decision=%s reason=%s injured_score=%d replacement_score=%d required_margin=%d phase=%s",
            source != NULL ? source : "",
            rec->team_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            rec->league_id,
            kbo_foreign_injury_decision_choice_label(decision->choice),
            decision->reason,
            decision->injured_score,
            decision->replacement_score,
            decision->required_margin,
            close_phase);
    }
}
