#include "../internal/foreign_injury_internal.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"

static int kbo_foreign_injury_record_slot_matches_replacement(
    const KboForeignInjuryReplacement* rec,
    uint8_t* replacement,
    uint8_t requested_slot_type)
{
    if (rec == NULL || replacement == NULL) {
        return 0;
    }
    if (requested_slot_type != 0u) {
        return rec->slot_type == requested_slot_type;
    }

    uint8_t player_slot_type = kbo_foreign_injury_slot_type_for_player(replacement);
    if (rec->slot_type == player_slot_type) {
        return 1;
    }
    return player_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
        && rec->slot_type == KBO_FOREIGN_INJURY_SLOT_REGULAR;
}

static int kbo_foreign_injury_record_matches_signed_replacement(
    const KboForeignInjuryReplacement* rec,
    uint32_t team_id,
    uint8_t* replacement,
    uint32_t replacement_player_id,
    uint8_t slot_type,
    uint32_t injured_player_id)
{
    return rec != NULL
        && rec->team_id == team_id
        && kbo_foreign_injury_status_uses_slot(rec->status)
        && kbo_foreign_injury_record_has_minimum_injury_basis(rec)
        && rec->injured_player_id != replacement_player_id
        && (injured_player_id == 0u || rec->injured_player_id == injured_player_id)
        && (rec->replacement_player_id == 0u || rec->replacement_player_id == replacement_player_id)
        && kbo_foreign_injury_record_slot_matches_replacement(rec, replacement, slot_type);
}

int kbo_attach_foreign_injury_replacement_after_signing(
    uint32_t team_id,
    uint8_t* replacement,
    uint8_t slot_type,
    uint32_t injured_player_id,
    const char* source)
{
    if (!kbo_foreign_injury_replacement_enabled()
            || team_id == 0u
            || replacement == NULL
            || !memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(replacement)) {
        return 0;
    }

    uint32_t replacement_player_id = *(uint32_t*)(replacement + OOTP27_PLAYER_ID_OFFSET);
    if (replacement_player_id == 0u) {
        return 0;
    }

    uint32_t today = 0u;
    kbo_current_date_tick_latest_published_date(&today);
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (!kbo_foreign_injury_replacement_in_season_window(
            league_id,
            today,
            source,
            "attach_signed_replacement")) {
        return 0;
    }

    KboForeignInjuryReplacement updated_rec;
    memset(&updated_rec, 0, sizeof(updated_rec));
    int changed = 0;
    int already_attached = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (!kbo_foreign_injury_record_matches_signed_replacement(
                rec,
                team_id,
                replacement,
                replacement_player_id,
                slot_type,
                injured_player_id)) {
            continue;
        }
        if (kbo_foreign_injury_replacement_player_reserved_locked(replacement_player_id, rec)) {
            continue;
        }

        if (rec->replacement_player_id == replacement_player_id
                && rec->status == KBO_FOREIGN_INJURY_STATUS_ACTIVE) {
            updated_rec = *rec;
            already_attached = 1;
            break;
        }

        rec->replacement_player_id = replacement_player_id;
        rec->status = KBO_FOREIGN_INJURY_STATUS_ACTIVE;
        rec->converted = 0u;
        updated_rec = *rec;
        changed = 1;
        break;
    }
    if (changed) {
        changed = kbo_persist_foreign_injury_replacements_locked();
    }
    kbo_unlock_foreign_injury_replacements();

    if (!changed && !already_attached) {
        return 0;
    }

    if (changed) {
        int days_left = 0;
        uint8_t* injured = kbo_find_player_by_id(updated_rec.injured_player_id, NULL, NULL);
        if (injured != NULL && memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
            KboForeignInjuryLiveMemory live_injury;
            memset(&live_injury, 0, sizeof(live_injury));
            if (kbo_foreign_injury_read_live_memory(injured, &live_injury)) {
                days_left = (int)live_injury.days_left;
            }
        }
        kbo_emit_foreign_injury_replacement_news_on_date(&updated_rec, days_left, "active", today);
        do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_log_field_u32(&audit_fields, "team_id", updated_rec.team_id);
            kbo_log_field_u32(&audit_fields, "league_id", updated_rec.league_id);
            kbo_log_field_u32(&audit_fields, "injured_player_id", updated_rec.injured_player_id);
            kbo_log_field_u32(&audit_fields, "replacement_player_id", updated_rec.replacement_player_id);
            kbo_log_field_u32(&audit_fields, "slot_type", (uint32_t)updated_rec.slot_type);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.lifecycle",
                "attach_signed_replacement",
                "foreign_signing_filled_open_slot",
                source,
                &audit_fields);
        } while (0);
    }

    kbo_log_runtimef(
        "foreign injury replacement: signed replacement attached source=%s team=%u injured=%u replacement=%u league=%u slot=%s changed=%d already=%d",
        source != NULL ? source : "",
        updated_rec.team_id,
        updated_rec.injured_player_id,
        updated_rec.replacement_player_id,
        updated_rec.league_id,
        kbo_foreign_injury_slot_label(updated_rec.slot_type),
        changed,
        already_attached);
    return 1;
}
