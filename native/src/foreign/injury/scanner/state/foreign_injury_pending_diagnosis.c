#include "../foreign_injury_scanner_internal.h"

#define KBO_FOREIGN_INJURY_PENDING_DIAGNOSIS_MAX 256

static KboLock g_kbo_foreign_injury_pending_diagnosis_lock = KBO_LOCK_INIT;
static KboForeignInjuryPendingDiagnosis
    g_kbo_foreign_injury_pending_diagnoses[KBO_FOREIGN_INJURY_PENDING_DIAGNOSIS_MAX];

int kbo_foreign_injury_pending_diagnosis_find(
    uint32_t player_id,
    KboForeignInjuryPendingDiagnosis* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (player_id == 0u) {
        return 0;
    }

    kbo_lock_enter(&g_kbo_foreign_injury_pending_diagnosis_lock);
    for (int i = 0; i < KBO_FOREIGN_INJURY_PENDING_DIAGNOSIS_MAX; i++) {
        KboForeignInjuryPendingDiagnosis rec = g_kbo_foreign_injury_pending_diagnoses[i];
        if (rec.active && rec.player_id == player_id) {
            kbo_lock_leave(&g_kbo_foreign_injury_pending_diagnosis_lock);
            if (out != NULL) {
                *out = rec;
            }
            return 1;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_injury_pending_diagnosis_lock);
    return 0;
}

void kbo_foreign_injury_pending_diagnosis_clear(uint32_t player_id)
{
    if (player_id == 0u) {
        return;
    }

    kbo_lock_enter(&g_kbo_foreign_injury_pending_diagnosis_lock);
    for (int i = 0; i < KBO_FOREIGN_INJURY_PENDING_DIAGNOSIS_MAX; i++) {
        if (g_kbo_foreign_injury_pending_diagnoses[i].active
                && g_kbo_foreign_injury_pending_diagnoses[i].player_id == player_id) {
            memset(
                &g_kbo_foreign_injury_pending_diagnoses[i],
                0,
                sizeof(g_kbo_foreign_injury_pending_diagnoses[i]));
            break;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_injury_pending_diagnosis_lock);
}

void kbo_foreign_injury_note_pending_diagnosis(
    uint32_t player_id,
    uint32_t team_id,
    uint32_t league_id,
    uint8_t slot_type,
    uint32_t today,
    const char* source)
{
    if (player_id == 0u || team_id == 0u || today == 0u) {
        return;
    }

    KboForeignInjuryPendingDiagnosis snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    int created = 0;
    int slot = -1;

    kbo_lock_enter(&g_kbo_foreign_injury_pending_diagnosis_lock);
    for (int i = 0; i < KBO_FOREIGN_INJURY_PENDING_DIAGNOSIS_MAX; i++) {
        if (g_kbo_foreign_injury_pending_diagnoses[i].active
                && g_kbo_foreign_injury_pending_diagnoses[i].player_id == player_id) {
            slot = i;
            break;
        }
        if (!g_kbo_foreign_injury_pending_diagnoses[i].active && slot < 0) {
            slot = i;
        }
    }
    if (slot >= 0) {
        KboForeignInjuryPendingDiagnosis* rec = &g_kbo_foreign_injury_pending_diagnoses[slot];
        if (!rec->active) {
            memset(rec, 0, sizeof(*rec));
            rec->player_id = player_id;
            rec->first_seen_date = today;
            rec->active = 1u;
            created = 1;
        }
        rec->team_id = team_id;
        rec->league_id = league_id;
        rec->slot_type = slot_type;
        rec->last_seen_date = today;
        snapshot = *rec;
    }
    kbo_lock_leave(&g_kbo_foreign_injury_pending_diagnosis_lock);

    if (slot < 0 || !created) {
        return;
    }

    do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", today);
        kbo_log_field_u32(&audit_fields, "player_id", snapshot.player_id);
        kbo_log_field_u32(&audit_fields, "team_id", snapshot.team_id);
        kbo_log_field_u32(&audit_fields, "league_id", snapshot.league_id);
        kbo_log_field_u32(&audit_fields, "first_seen_date", snapshot.first_seen_date);
        kbo_log_field_u32(&audit_fields, "slot_type", (uint32_t)snapshot.slot_type);
        kbo_rule_audit_emit_fields(
            "foreign_injury.replacement.lifecycle",
            "pending_candidate",
            "active_injury_waiting_for_diagnosis_or_minimum_days",
            source,
            &audit_fields);
    } while (0);

    kbo_log_runtimef(
        "foreign injury replacement: pending diagnosis source=%s player=%u team=%u league=%u first_seen=%u slot=%s",
        source != NULL ? source : "",
        snapshot.player_id,
        snapshot.team_id,
        snapshot.league_id,
        snapshot.first_seen_date,
        kbo_foreign_injury_slot_label(snapshot.slot_type));
}
