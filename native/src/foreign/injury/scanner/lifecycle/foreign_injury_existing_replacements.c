#include "logs/foreign_injury_existing_replacements_logs.h"
#include "player_state/foreign_injury_existing_replacements_player_state.h"
#include "team_cache/foreign_injury_existing_replacements_team_cache.h"

void kbo_foreign_injury_process_existing_replacements(
    uint32_t today,
    const char* source,
    int* out_active_count,
    int* out_closed_count)
{
    if (out_active_count != NULL) {
        *out_active_count = 0;
    }
    if (out_closed_count != NULL) {
        *out_closed_count = 0;
    }

    KboForeignInjuryClosedNews closed_news[16];
    int closed_count = 0;
    int invalid_closed_count = 0;
    KboForeignInjuryReplacement active_news[16];
    int active_count = 0;
    int changed = 0;
    memset(closed_news, 0, sizeof(closed_news));
    memset(active_news, 0, sizeof(active_news));
    KboForeignInjuryTeamLookupCacheEntry team_cache[32];
    int team_cache_count = 0;
    memset(team_cache, 0, sizeof(team_cache));
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            if (rec->close_choice == KBO_FOREIGN_INJURY_CLOSE_OFFSEASON_RESET) {
                continue;
            }
            if (rec->converted == 0u && rec->replacement_player_id != 0u) {
                uint8_t* top_team = kbo_foreign_injury_cached_team_lookup(
                    rec->team_id,
                    team_cache,
                    &team_cache_count,
                    (int)(sizeof(team_cache) / sizeof(team_cache[0])));
                int inactive_roster_present = top_team != NULL && memory_range_readable(top_team, OOTP27_KBO_TEAM_READABLE_BYTES)
                    ? kbo_foreign_injury_team_inactive_roster_contains_player(top_team, rec->injured_player_id)
                    : 0;
                int active_roster_present = top_team != NULL && memory_range_readable(top_team, OOTP27_KBO_TEAM_READABLE_BYTES)
                    ? kbo_foreign_injury_team_active_roster_contains_player(top_team, rec->injured_player_id)
                    : 0;
                uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
                KboForeignInjuryLiveMemory live_injury;
                memset(&live_injury, 0, sizeof(live_injury));
                kbo_foreign_injury_read_live_memory(injured, &live_injury);
                int runtime_injury_present = kbo_foreign_injury_runtime_injury_present(injured);
                int roster_hold_flags_present = kbo_foreign_injury_roster_hold_flags_present(injured);
                int return_close_allowed = injured != NULL && memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)
                    ? kbo_foreign_injury_return_state_allows_close(
                        live_injury.active,
                        (int16_t)live_injury.days_left,
                        injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
                        active_roster_present,
                        inactive_roster_present,
                        roster_hold_flags_present,
                        1)
                    : 0;
                int repair_allowed = !return_close_allowed
                    && !kbo_foreign_injury_replacement_player_reserved_locked(rec->replacement_player_id, rec)
                    && kbo_foreign_injury_closed_record_can_repair_on_date(
                        rec,
                        &live_injury,
                        today,
                        inactive_roster_present,
                        roster_hold_flags_present);
                if (repair_allowed) {
                    uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
                    repair_allowed = kbo_foreign_injury_replacement_player_attached_to_record(rec, replacement)
                        || kbo_foreign_injury_replacement_player_can_restore_to_record(rec, replacement);
                }
                if (repair_allowed) {
                    rec->status = KBO_FOREIGN_INJURY_STATUS_ACTIVE;
                    rec->closed_on_yyyymmdd = 0u;
                    rec->close_choice = 0u;
                    if (rec->injury_id == 0u && live_injury.injury_id != 0u) {
                        rec->injury_id = live_injury.injury_id;
                    }
                    kbo_foreign_injury_restore_active_replacement_player(rec, source);
                    changed = 1;
                    if (active_count < (int)(sizeof(active_news) / sizeof(active_news[0]))) {
                        active_news[active_count++] = *rec;
                    }
                    kbo_log_runtimef(
                        "foreign injury replacement: repaired premature close source=%s team=%u injured=%u replacement=%u league=%u today=%u expected_end=%u reason=closed_before_real_return runtime_injury=%d active_roster=%d inactive_roster=%d roster_hold_flags=%d",
                        source != NULL ? source : "",
                        rec->team_id,
                        rec->injured_player_id,
                        rec->replacement_player_id,
                        rec->league_id,
                        today,
                        rec->expected_end_yyyymmdd,
                        runtime_injury_present,
                        active_roster_present,
                        inactive_roster_present,
                        roster_hold_flags_present);
                }
            }
            continue;
        }
        uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
        if (injured == NULL || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        KboForeignInjuryLiveMemory live_injury;
        memset(&live_injury, 0, sizeof(live_injury));
        kbo_foreign_injury_read_live_memory(injured, &live_injury);
        uint8_t* top_team = kbo_foreign_injury_cached_team_lookup(
            rec->team_id,
            team_cache,
            &team_cache_count,
            (int)(sizeof(team_cache) / sizeof(team_cache[0])));
        int inactive_roster_present = top_team != NULL && memory_range_readable(top_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            ? kbo_foreign_injury_team_inactive_roster_contains_player(top_team, rec->injured_player_id)
            : 0;
        int active_roster_present = top_team != NULL && memory_range_readable(top_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            ? kbo_foreign_injury_team_active_roster_contains_player(top_team, rec->injured_player_id)
            : 0;
        int roster_hold_flags_present = kbo_foreign_injury_roster_hold_flags_present(injured);
        int uses_slot = kbo_foreign_injury_status_uses_slot(rec->status);
        if (!uses_slot && rec->status != KBO_FOREIGN_INJURY_STATUS_PENDING) {
            continue;
        }
        int expected_end_pending = kbo_foreign_injury_expected_end_pending(today, rec->expected_end_yyyymmdd);
        int runtime_injury_present = kbo_foreign_injury_runtime_injury_present(injured);
        int minimum_injury_basis = kbo_foreign_injury_record_has_minimum_injury_basis_on_date(rec, today);
        int record_continuation_basis = kbo_foreign_injury_live_memory_has_record_continuation_basis(
            rec,
            &live_injury,
            today);
        if (rec->injury_id == 0u && live_injury.injury_id != 0u && record_continuation_basis) {
            rec->injury_id = live_injury.injury_id;
            changed = 1;
        }
        int stale_without_injury_basis = uses_slot
            && !runtime_injury_present
            && !minimum_injury_basis
            && !expected_end_pending
            && !inactive_roster_present
            && !active_roster_present
            && !roster_hold_flags_present;
        int close_decision_allowed = rec->league_id != 0u
            && kbo_foreign_injury_replacement_close_decision_allowed(
                rec->league_id,
                today,
                source,
                "existing_close_slot");
        int detached_reserved_replacement = 0;
        KboForeignInjuryExistingWaitLogContext wait_log_context = {
            rec,
            injured,
            &live_injury,
            active_roster_present,
            inactive_roster_present,
            roster_hold_flags_present,
            today,
            0,
            source
        };
        if (uses_slot
                && rec->replacement_player_id != 0u
                && kbo_foreign_injury_replacement_player_reserved_locked(rec->replacement_player_id, rec)) {
            uint32_t detached_replacement_player_id = rec->replacement_player_id;
            rec->replacement_player_id = 0u;
            if (rec->status == KBO_FOREIGN_INJURY_STATUS_ACTIVE) {
                rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
            }
            detached_reserved_replacement = 1;
            changed = 1;
            do {
                KboLogFields audit_fields;
                kbo_log_fields_init(&audit_fields);
                kbo_log_field_u32(&audit_fields, "date", today);
                kbo_log_field_u32(&audit_fields, "team_id", rec->team_id);
                kbo_log_field_u32(&audit_fields, "league_id", rec->league_id);
                kbo_log_field_u32(&audit_fields, "injured_player_id", rec->injured_player_id);
                kbo_log_field_u32(&audit_fields, "replacement_player_id", detached_replacement_player_id);
                kbo_rule_audit_emit_fields(
                    "foreign_injury.replacement.lifecycle",
                    "detach_replacement",
                    "replacement_reserved_elsewhere",
                    source,
                    &audit_fields);
            } while (0);
            kbo_log_runtimef(
                "foreign injury replacement: detached reserved replacement source=%s team=%u injured=%u replacement=%u league=%u",
                source != NULL ? source : "",
                rec->team_id,
                rec->injured_player_id,
                detached_replacement_player_id,
                rec->league_id);
        }
        if (uses_slot
                && rec->replacement_player_id != 0u
                && kbo_foreign_injury_replacement_unavailable_by_long_injury(rec, today)) {
            uint32_t unavailable_replacement_player_id = rec->replacement_player_id;
            kbo_foreign_injury_release_replacement_player(
                rec->team_id,
                unavailable_replacement_player_id,
                source);
            rec->replacement_player_id = 0u;
            if (rec->status == KBO_FOREIGN_INJURY_STATUS_ACTIVE) {
                rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
            }
            detached_reserved_replacement = 1;
            changed = 1;
            do {
                KboLogFields audit_fields;
                kbo_log_fields_init(&audit_fields);
                kbo_log_field_u32(&audit_fields, "date", today);
                kbo_log_field_u32(&audit_fields, "team_id", rec->team_id);
                kbo_log_field_u32(&audit_fields, "league_id", rec->league_id);
                kbo_log_field_u32(&audit_fields, "injured_player_id", rec->injured_player_id);
                kbo_log_field_u32(&audit_fields, "replacement_player_id", unavailable_replacement_player_id);
                kbo_rule_audit_emit_fields(
                    "foreign_injury.replacement.lifecycle",
                    "detach_replacement",
                    "replacement_long_term_injured",
                    source,
                    &audit_fields);
            } while (0);
            kbo_log_runtimef(
                "foreign injury replacement: detached injured replacement source=%s team=%u injured=%u replacement=%u league=%u",
                source != NULL ? source : "",
                rec->team_id,
                rec->injured_player_id,
                unavailable_replacement_player_id,
                rec->league_id);
        }
        if (uses_slot && rec->replacement_player_id != 0u) {
            uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
            if (!kbo_foreign_injury_replacement_player_attached_to_record(rec, replacement)
                    && !kbo_foreign_injury_replacement_player_can_restore_to_record(rec, replacement)) {
                uint32_t detached_replacement_player_id = rec->replacement_player_id;
                rec->replacement_player_id = 0u;
                if (rec->status == KBO_FOREIGN_INJURY_STATUS_ACTIVE) {
                    rec->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
                }
                detached_reserved_replacement = 1;
                changed = 1;
                do {
                    KboLogFields audit_fields;
                    kbo_log_fields_init(&audit_fields);
                    kbo_log_field_u32(&audit_fields, "date", today);
                    kbo_log_field_u32(&audit_fields, "team_id", rec->team_id);
                    kbo_log_field_u32(&audit_fields, "league_id", rec->league_id);
                    kbo_log_field_u32(&audit_fields, "injured_player_id", rec->injured_player_id);
                    kbo_log_field_u32(&audit_fields, "replacement_player_id", detached_replacement_player_id);
                    kbo_rule_audit_emit_fields(
                        "foreign_injury.replacement.lifecycle",
                        "detach_replacement",
                        "replacement_detached_from_record",
                        source,
                        &audit_fields);
                } while (0);
                kbo_log_runtimef(
                    "foreign injury replacement: detached off-record replacement source=%s team=%u injured=%u replacement=%u league=%u",
                    source != NULL ? source : "",
                    rec->team_id,
                    rec->injured_player_id,
                    detached_replacement_player_id,
                    rec->league_id);
            }
        }
        if (uses_slot
                && !runtime_injury_present
                && !active_roster_present
                && !inactive_roster_present
                && !roster_hold_flags_present
                && !expected_end_pending
                && !minimum_injury_basis) {
            if (!close_decision_allowed) {
                continue;
            }
            uint32_t replacement_player_id = kbo_foreign_injury_resolve_replacement_for_record(rec);
            if (replacement_player_id != 0u) {
                rec->replacement_player_id = replacement_player_id;
                kbo_foreign_injury_release_replacement_player(
                    rec->team_id,
                    replacement_player_id,
                    source);
            } else {
                rec->replacement_player_id = 0u;
            }
            rec->converted = 0u;
            rec->status = KBO_FOREIGN_INJURY_STATUS_CLOSED;
            rec->closed_on_yyyymmdd = today;
            rec->close_choice = KBO_FOREIGN_INJURY_CLOSE_KEEP_INJURED;
            if (closed_count < (int)(sizeof(closed_news) / sizeof(closed_news[0]))) {
                closed_news[closed_count].rec = *rec;
                closed_news[closed_count].decision.choice = KBO_FOREIGN_INJURY_DECISION_KEEP_INJURED;
                snprintf(
                    closed_news[closed_count].decision.reason,
                    sizeof(closed_news[closed_count].decision.reason),
                    "%s",
                    "invalid_no_long_term_injury_basis");
                snprintf(closed_news[closed_count].phase, sizeof(closed_news[closed_count].phase), "%s", "closed_invalid");
                closed_count++;
            }
            invalid_closed_count++;
            changed = 1;
            do {
                KboLogFields audit_fields;
                kbo_log_fields_init(&audit_fields);
                kbo_log_field_u32(&audit_fields, "date", today);
                kbo_log_field_u32(&audit_fields, "team_id", rec->team_id);
                kbo_log_field_u32(&audit_fields, "league_id", rec->league_id);
                kbo_log_field_u32(&audit_fields, "injured_player_id", rec->injured_player_id);
                kbo_log_field_u32(&audit_fields, "replacement_player_id", rec->replacement_player_id);
                kbo_rule_audit_emit_fields(
                    "foreign_injury.replacement.lifecycle",
                    "close_slot",
                    "invalid_no_long_term_injury_basis",
                    source,
                    &audit_fields);
            } while (0);
            kbo_log_runtimef(
                "foreign injury replacement: closed invalid slot source=%s team=%u injured=%u replacement=%u league=%u reason=no_long_term_injury_basis",
                source != NULL ? source : "",
                rec->team_id,
                rec->injured_player_id,
                rec->replacement_player_id,
                rec->league_id);
            continue;
        }
        if (uses_slot && record_continuation_basis) {
            kbo_foreign_injury_restore_active_replacement_player(rec, source);
        } else if (uses_slot && rec->replacement_player_id != 0u && expected_end_pending) {
            kbo_foreign_injury_log_existing_replacement_wait(
                KBO_FOREIGN_INJURY_EXISTING_WAIT_ACTIVE_RESTORE,
                &wait_log_context);
        }
        if (uses_slot && rec->replacement_player_id == 0u && expected_end_pending && record_continuation_basis) {
            uint32_t replacement_player_id = kbo_foreign_injury_resolve_replacement_for_record(rec);
            if (replacement_player_id != 0u) {
                rec->replacement_player_id = replacement_player_id;
                rec->status = KBO_FOREIGN_INJURY_STATUS_ACTIVE;
                rec->closed_on_yyyymmdd = 0u;
                rec->close_choice = 0u;
                uses_slot = 1;
                if (active_count < (int)(sizeof(active_news) / sizeof(active_news[0]))) {
                    active_news[active_count++] = *rec;
                }
                changed = 1;
            }
        }
        if (!close_decision_allowed) {
            continue;
        }

        int returned_to_org_roster = (close_decision_allowed
            && kbo_foreign_injury_injured_player_returned_to_org_roster(rec, injured))
            || kbo_foreign_injury_return_state_allows_close(
                live_injury.active,
                (int16_t)live_injury.days_left,
                injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
                active_roster_present,
                inactive_roster_present,
                roster_hold_flags_present,
                close_decision_allowed);
        if (stale_without_injury_basis) {
            returned_to_org_roster = 1;
        }
        if (returned_to_org_roster && (inactive_roster_present || roster_hold_flags_present) && !stale_without_injury_basis) {
            returned_to_org_roster = 0;
        }
        if (returned_to_org_roster
                && expected_end_pending
                && (!active_roster_present || rec->replacement_player_id == 0u)
                && !stale_without_injury_basis) {
            returned_to_org_roster = 0;
            kbo_foreign_injury_log_existing_replacement_wait(
                KBO_FOREIGN_INJURY_EXISTING_WAIT_SUPPRESS_EARLY_CLOSE,
                &wait_log_context);
        }
        if (stale_without_injury_basis) {
            kbo_foreign_injury_log_existing_replacement_wait(
                KBO_FOREIGN_INJURY_EXISTING_WAIT_STALE_WITHOUT_BASIS,
                &wait_log_context);
        }
        int expected_end_reached = close_decision_allowed
            && kbo_foreign_injury_expected_end_reached(today, rec->expected_end_yyyymmdd);
        wait_log_context.expected_end_reached = expected_end_reached;
        if (!returned_to_org_roster && (inactive_roster_present || roster_hold_flags_present)) {
            if (live_injury.active == 0u) {
                kbo_foreign_injury_log_existing_replacement_wait(
                    KBO_FOREIGN_INJURY_EXISTING_WAIT_INACTIVE_RETURN,
                    &wait_log_context);
            }
            continue;
        }
        if (!returned_to_org_roster) {
            if (live_injury.active == 0u) {
                kbo_foreign_injury_log_existing_replacement_wait(
                    KBO_FOREIGN_INJURY_EXISTING_WAIT_TOP_TEAM_RETURN,
                    &wait_log_context);
            }
            continue;
        }

        uint32_t replacement_player_id = detached_reserved_replacement
            ? 0u
            : kbo_foreign_injury_resolve_replacement_for_record(rec);
        if (replacement_player_id != 0u) {
            rec->replacement_player_id = replacement_player_id;
        } else {
            rec->replacement_player_id = 0u;
        }
        KboForeignInjuryReplacementDecision decision;
        memset(&decision, 0, sizeof(decision));
        const char* close_phase = "closed_without_replacement";
        if (rec->replacement_player_id != 0u) {
            uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
            kbo_foreign_injury_choose_returning_player(rec, injured, replacement, &decision);
            if (decision.choice == KBO_FOREIGN_INJURY_DECISION_KEEP_REPLACEMENT) {
                kbo_foreign_injury_release_injured_player(
                    rec->team_id,
                    rec->injured_player_id,
                    source);
                rec->converted = 1u;
                rec->close_choice = KBO_FOREIGN_INJURY_CLOSE_KEEP_REPLACEMENT;
                close_phase = "closed_keep_replacement";
            } else {
                kbo_foreign_injury_release_replacement_player(
                    rec->team_id,
                    rec->replacement_player_id,
                    source);
                rec->converted = 0u;
                rec->close_choice = KBO_FOREIGN_INJURY_CLOSE_KEEP_INJURED;
                close_phase = "closed_keep_injured";
            }
        } else {
            kbo_foreign_injury_choose_returning_player(rec, injured, NULL, &decision);
            rec->converted = 0u;
            rec->close_choice = KBO_FOREIGN_INJURY_CLOSE_KEEP_INJURED;
        }
        rec->status = KBO_FOREIGN_INJURY_STATUS_CLOSED;
        rec->closed_on_yyyymmdd = today;
        if (closed_count < (int)(sizeof(closed_news) / sizeof(closed_news[0]))) {
            closed_news[closed_count].rec = *rec;
            closed_news[closed_count].decision = decision;
            snprintf(closed_news[closed_count].phase, sizeof(closed_news[closed_count].phase), "%s", close_phase);
            closed_count++;
        }
        changed = 1;
    }
    if (changed) {
        kbo_persist_foreign_injury_replacements_locked();
    }
    kbo_unlock_foreign_injury_replacements();

    kbo_foreign_injury_emit_active_replacement_news_batch(active_news, active_count, today, source);
    kbo_foreign_injury_emit_closed_news_batch(closed_news, closed_count, today, source);
    if (out_active_count != NULL) {
        *out_active_count = active_count;
    }
    if (out_closed_count != NULL) {
        *out_closed_count = closed_count + invalid_closed_count;
    }
}
