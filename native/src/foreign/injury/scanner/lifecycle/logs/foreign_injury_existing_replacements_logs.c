#include "foreign_injury_existing_replacements_logs.h"

static LONG g_kbo_foreign_injury_return_wait_log_count = 0;

static int kbo_foreign_injury_existing_wait_log_allowed(void)
{
    LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_return_wait_log_count);
    return log_slot <= 80 || (log_slot % 100) == 0;
}

void kbo_foreign_injury_log_existing_replacement_wait(
    KboForeignInjuryExistingWaitLogKind kind,
    const KboForeignInjuryExistingWaitLogContext* context)
{
    if (context == NULL || context->rec == NULL || context->injured == NULL || context->live_injury == NULL) {
        return;
    }
    if (!kbo_foreign_injury_existing_wait_log_allowed()) {
        return;
    }

    const KboForeignInjuryReplacement* rec = context->rec;
    uint8_t* injured = context->injured;
    const KboForeignInjuryLiveMemory* live_injury = context->live_injury;
    const char* source = context->source != NULL ? context->source : "";

    switch (kind) {
    case KBO_FOREIGN_INJURY_EXISTING_WAIT_ACTIVE_RESTORE:
        kbo_log_runtimef(
            "foreign injury replacement: skipped active replacement restore without runtime injury source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d inactive_roster=%d today=%u expected_end=%u",
            source,
            rec->team_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
            *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
            *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
            rec->league_id,
            (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
            (uint32_t)live_injury->active,
            (int)live_injury->days_left,
            context->inactive_roster_present,
            context->today,
            rec->expected_end_yyyymmdd);
        break;
    case KBO_FOREIGN_INJURY_EXISTING_WAIT_SUPPRESS_EARLY_CLOSE:
        kbo_log_runtimef(
            "foreign injury replacement: suppressing early close before expected end source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d active_roster=%d inactive_roster=%d roster_hold_flags=%d today=%u expected_end=%u",
            source,
            rec->team_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
            *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
            *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
            rec->league_id,
            (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
            (uint32_t)live_injury->active,
            (int)live_injury->days_left,
            context->active_roster_present,
            context->inactive_roster_present,
            context->roster_hold_flags_present,
            context->today,
            rec->expected_end_yyyymmdd);
        break;
    case KBO_FOREIGN_INJURY_EXISTING_WAIT_STALE_WITHOUT_BASIS:
        kbo_log_runtimef(
            "foreign injury replacement: closing stale active slot without runtime injury source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d active_roster=%d inactive_roster=%d today=%u expected_end=%u",
            source,
            rec->team_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
            *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
            *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
            rec->league_id,
            (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
            (uint32_t)live_injury->active,
            (int)live_injury->days_left,
            context->active_roster_present,
            context->inactive_roster_present,
            context->today,
            rec->expected_end_yyyymmdd);
        break;
    case KBO_FOREIGN_INJURY_EXISTING_WAIT_INACTIVE_RETURN:
        kbo_log_runtimef(
            "foreign injury replacement: waiting inactive roster return source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d active_roster=%d inactive_roster=%d roster_hold_flags=%d today=%u expected_end=%u expected_end_reached=%d",
            source,
            rec->team_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
            *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
            *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
            rec->league_id,
            (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
            (uint32_t)live_injury->active,
            (int)live_injury->days_left,
            context->active_roster_present,
            context->inactive_roster_present,
            context->roster_hold_flags_present,
            context->today,
            rec->expected_end_yyyymmdd,
            context->expected_end_reached);
        break;
    case KBO_FOREIGN_INJURY_EXISTING_WAIT_TOP_TEAM_RETURN:
        kbo_log_runtimef(
            "foreign injury replacement: waiting top-team return source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d active_roster=%d inactive_roster=%d roster_hold_flags=%d today=%u expected_end=%u expected_end_reached=%d",
            source,
            rec->team_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
            *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
            *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
            rec->league_id,
            (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
            (uint32_t)live_injury->active,
            (int)live_injury->days_left,
            context->active_roster_present,
            context->inactive_roster_present,
            context->roster_hold_flags_present,
            context->today,
            rec->expected_end_yyyymmdd,
            context->expected_end_reached);
        break;
    }
}

void kbo_foreign_injury_emit_active_replacement_news_batch(
    const KboForeignInjuryReplacement* active_news,
    int active_count,
    uint32_t today,
    const char* source)
{
    if (active_news == NULL || active_count <= 0) {
        return;
    }
    for (int i = 0; i < active_count; i++) {
        do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_log_field_u32(&audit_fields, "team_id", active_news[i].team_id);
            kbo_log_field_u32(&audit_fields, "league_id", active_news[i].league_id);
            kbo_log_field_u32(&audit_fields, "injured_player_id", active_news[i].injured_player_id);
            kbo_log_field_u32(&audit_fields, "replacement_player_id", active_news[i].replacement_player_id);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.lifecycle",
                "activate_slot",
                "replacement_resolved",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign injury replacement: activated source=%s team=%u injured=%u replacement=%u league=%u",
            source != NULL ? source : "",
            active_news[i].team_id,
            active_news[i].injured_player_id,
            active_news[i].replacement_player_id,
            active_news[i].league_id);
    }
}
