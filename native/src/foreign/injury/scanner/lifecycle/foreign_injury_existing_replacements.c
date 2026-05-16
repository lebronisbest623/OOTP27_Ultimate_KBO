#include "../foreign_injury_scanner_internal.h"

static LONG g_kbo_foreign_injury_return_wait_log_count = 0;

typedef struct KboForeignInjuryTeamLookupCacheEntry {
    uint32_t team_id;
    uint8_t* team;
} KboForeignInjuryTeamLookupCacheEntry;

static uint8_t* kbo_foreign_injury_cached_team_lookup(
    uint32_t team_id,
    KboForeignInjuryTeamLookupCacheEntry* cache,
    int* cache_count,
    int cache_capacity)
{
    if (team_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < *cache_count; i++) {
        if (cache[i].team_id == team_id) {
            return cache[i].team;
        }
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (*cache_count < cache_capacity) {
        cache[*cache_count].team_id = team_id;
        cache[*cache_count].team = team;
        (*cache_count)++;
    }
    return team;
}

static int kbo_foreign_injury_replacement_unavailable_by_long_injury(
    const KboForeignInjuryReplacement* rec,
    uint32_t today)
{
    if (rec == NULL || rec->replacement_player_id == 0u) {
        return 0;
    }
    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, &team_id, &league_id);
    if (replacement == NULL || !memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
    int16_t days_left = *(int16_t*)(replacement + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
    if (kbo_foreign_injury_duration_meets_minimum(days_left, min_days)) {
        return 1;
    }

    int message_days = 0;
    if (kbo_foreign_injury_recent_message_has_long_term_injury(
            rec->replacement_player_id,
            min_days,
            &message_days)) {
        return 1;
    }

    int sql_days = 0;
    uint32_t sql_date = 0u;
    if (kbo_foreign_injury_recent_sql_has_long_term_injury_date(
            rec->replacement_player_id,
            min_days,
            &sql_days,
            &sql_date)) {
        if (sql_date == 0u || !kbo_foreign_injury_expected_end_reached(
                today,
                kbo_foreign_injury_expected_end_from_duration(sql_date, sql_days))) {
            return 1;
        }
    }

    (void)team_id;
    (void)league_id;
    return 0;
}

static int kbo_foreign_injury_runtime_injury_present(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] != 0u
        || *(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET) > 0;
}

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
            if (rec->converted == 0u && rec->replacement_player_id != 0u) {
                uint8_t* top_team = kbo_foreign_injury_cached_team_lookup(
                    rec->team_id,
                    team_cache,
                    &team_cache_count,
                    (int)(sizeof(team_cache) / sizeof(team_cache[0])));
                int inactive_roster_present = top_team != NULL && memory_range_readable(top_team, OOTP27_KBO_TEAM_READABLE_BYTES)
                    ? kbo_foreign_injury_team_inactive_roster_contains_player(top_team, rec->injured_player_id)
                    : 0;
                uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
                int runtime_injury_present = kbo_foreign_injury_runtime_injury_present(injured);
                int repair_allowed = inactive_roster_present
                    && runtime_injury_present
                    && kbo_foreign_injury_expected_end_pending(today, rec->expected_end_yyyymmdd)
                    && !kbo_foreign_injury_replacement_player_reserved_locked(rec->replacement_player_id, rec)
                    && kbo_foreign_injury_record_has_minimum_injury_basis(rec);
                if (repair_allowed) {
                    rec->status = KBO_FOREIGN_INJURY_STATUS_ACTIVE;
                    kbo_foreign_injury_restore_active_replacement_player(rec, source);
                    changed = 1;
                    if (active_count < (int)(sizeof(active_news) / sizeof(active_news[0]))) {
                        active_news[active_count++] = *rec;
                    }
                    kbo_log_runtimef(
                        "foreign injury replacement: repaired premature close source=%s team=%u injured=%u replacement=%u league=%u today=%u expected_end=%u reason=injured_still_inactive_roster",
                        source != NULL ? source : "",
                        rec->team_id,
                        rec->injured_player_id,
                        rec->replacement_player_id,
                        rec->league_id,
                        today,
                        rec->expected_end_yyyymmdd);
                }
            }
            continue;
        }
        uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
        if (injured == NULL || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* top_team = kbo_foreign_injury_cached_team_lookup(
            rec->team_id,
            team_cache,
            &team_cache_count,
            (int)(sizeof(team_cache) / sizeof(team_cache[0])));
        int inactive_roster_present = top_team != NULL && memory_range_readable(top_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            ? kbo_foreign_injury_team_inactive_roster_contains_player(top_team, rec->injured_player_id)
            : 0;
        int uses_slot = kbo_foreign_injury_status_uses_slot(rec->status);
        if (!uses_slot && rec->status != KBO_FOREIGN_INJURY_STATUS_PENDING) {
            continue;
        }
        int expected_end_pending = kbo_foreign_injury_expected_end_pending(today, rec->expected_end_yyyymmdd);
        int runtime_injury_present = kbo_foreign_injury_runtime_injury_present(injured);
        int minimum_injury_basis = kbo_foreign_injury_record_has_minimum_injury_basis(rec);
        int stale_without_injury_basis = uses_slot && !runtime_injury_present && !minimum_injury_basis;
        int close_decision_allowed = rec->league_id != 0u
            && kbo_foreign_injury_replacement_close_decision_allowed(
                rec->league_id,
                today,
                source,
                "existing_close_slot");
        int detached_reserved_replacement = 0;
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
        if (uses_slot
                && rec->replacement_player_id == 0u
                && (!inactive_roster_present || stale_without_injury_basis)
                && (!expected_end_pending || stale_without_injury_basis)
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
            }
            rec->converted = 0u;
            rec->status = KBO_FOREIGN_INJURY_STATUS_CLOSED;
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
        if (uses_slot && runtime_injury_present) {
            kbo_foreign_injury_restore_active_replacement_player(rec, source);
        } else if (uses_slot && rec->replacement_player_id != 0u && expected_end_pending) {
            LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_return_wait_log_count);
            if (log_slot <= 80 || (log_slot % 100) == 0) {
                kbo_log_runtimef(
                    "foreign injury replacement: skipped active replacement restore without runtime injury source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d inactive_roster=%d today=%u expected_end=%u",
                    source != NULL ? source : "",
                    rec->team_id,
                    rec->injured_player_id,
                    rec->replacement_player_id,
                    *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                    *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                    *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                    rec->league_id,
                    (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
                    (uint32_t)injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET],
                    (int)*(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET),
                    inactive_roster_present,
                    today,
                    rec->expected_end_yyyymmdd);
            }
        }
        if (uses_slot && rec->replacement_player_id == 0u && expected_end_pending && runtime_injury_present) {
            uint32_t replacement_player_id = kbo_foreign_injury_resolve_replacement_for_record(rec);
            if (replacement_player_id != 0u) {
                rec->replacement_player_id = replacement_player_id;
                rec->status = KBO_FOREIGN_INJURY_STATUS_ACTIVE;
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

        int active_roster_present = top_team != NULL && memory_range_readable(top_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            ? kbo_foreign_injury_team_active_roster_contains_player(top_team, rec->injured_player_id)
            : 0;
        int returned_to_org_roster = (close_decision_allowed
            && kbo_foreign_injury_injured_player_returned_to_org_roster(rec, injured))
            || kbo_foreign_injury_return_state_allows_close(
                injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET],
                *(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET),
                injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
                active_roster_present,
                inactive_roster_present,
                close_decision_allowed);
        if (stale_without_injury_basis) {
            returned_to_org_roster = 1;
        }
        if (returned_to_org_roster && inactive_roster_present && expected_end_pending && !stale_without_injury_basis) {
            returned_to_org_roster = 0;
        }
        if (returned_to_org_roster && rec->replacement_player_id == 0u && expected_end_pending && !stale_without_injury_basis) {
            returned_to_org_roster = 0;
            LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_return_wait_log_count);
            if (log_slot <= 80 || (log_slot % 100) == 0) {
                kbo_log_runtimef(
                    "foreign injury replacement: suppressing early close before expected end source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d active_roster=%d inactive_roster=%d today=%u expected_end=%u",
                    source != NULL ? source : "",
                    rec->team_id,
                    rec->injured_player_id,
                    rec->replacement_player_id,
                    *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                    *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                    *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                    rec->league_id,
                    (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
                    (uint32_t)injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET],
                    (int)*(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET),
                    active_roster_present,
                    inactive_roster_present,
                    today,
                    rec->expected_end_yyyymmdd);
            }
        }
        if (stale_without_injury_basis) {
            LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_return_wait_log_count);
            if (log_slot <= 80 || (log_slot % 100) == 0) {
                kbo_log_runtimef(
                    "foreign injury replacement: closing stale active slot without runtime injury source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d active_roster=%d inactive_roster=%d today=%u expected_end=%u",
                    source != NULL ? source : "",
                    rec->team_id,
                    rec->injured_player_id,
                    rec->replacement_player_id,
                    *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                    *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                    *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                    rec->league_id,
                    (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
                    (uint32_t)injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET],
                    (int)*(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET),
                    active_roster_present,
                    inactive_roster_present,
                    today,
                    rec->expected_end_yyyymmdd);
            }
        }
        int expected_end_due = close_decision_allowed
            && !returned_to_org_roster
            && kbo_foreign_injury_expected_end_reached(today, rec->expected_end_yyyymmdd);
        if (!returned_to_org_roster && inactive_roster_present && !expected_end_due) {
            if (injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] == 0u) {
                LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_return_wait_log_count);
                if (log_slot <= 80 || (log_slot % 100) == 0) {
                    kbo_log_runtimef(
                        "foreign injury replacement: waiting inactive roster return source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d active_roster=%d inactive_roster=%d today=%u expected_end=%u",
                        source != NULL ? source : "",
                        rec->team_id,
                        rec->injured_player_id,
                        rec->replacement_player_id,
                        *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                        *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                        *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                        rec->league_id,
                        (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
                        (uint32_t)injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET],
                        (int)*(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET),
                        active_roster_present,
                        inactive_roster_present,
                        today,
                        rec->expected_end_yyyymmdd);
                }
            }
            continue;
        }
        if (!returned_to_org_roster && !expected_end_due) {
            if (injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] == 0u) {
                LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_return_wait_log_count);
                if (log_slot <= 80 || (log_slot % 100) == 0) {
                    kbo_log_runtimef(
                        "foreign injury replacement: waiting top-team return source=%s team=%u injured=%u replacement=%u current=%u active=%u league=%u slot_league=%u loan_active=%u injury=%u days_left=%d active_roster=%d inactive_roster=%d",
                        source != NULL ? source : "",
                        rec->team_id,
                        rec->injured_player_id,
                        rec->replacement_player_id,
                        *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                        *(uint32_t*)(injured + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                        *(uint32_t*)(injured + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                        rec->league_id,
                        (uint32_t)injured[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET],
                        (uint32_t)injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET],
                        (int)*(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET),
                        active_roster_present,
                        inactive_roster_present);
                }
            }
            continue;
        }
        if (expected_end_due) {
            kbo_log_runtimef(
                "foreign injury replacement: expected-end close fallback source=%s team=%u injured=%u replacement=%u today=%u expected_end=%u",
                source != NULL ? source : "",
                rec->team_id,
                rec->injured_player_id,
                rec->replacement_player_id,
                today,
                rec->expected_end_yyyymmdd);
        }

        uint32_t replacement_player_id = detached_reserved_replacement
            ? 0u
            : kbo_foreign_injury_resolve_replacement_for_record(rec);
        if (replacement_player_id != 0u) {
            rec->replacement_player_id = replacement_player_id;
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
                close_phase = "closed_keep_replacement";
            } else {
                kbo_foreign_injury_release_replacement_player(
                    rec->team_id,
                    rec->replacement_player_id,
                    source);
                rec->converted = 0u;
                close_phase = "closed_keep_injured";
            }
        } else {
            kbo_foreign_injury_choose_returning_player(rec, injured, NULL, &decision);
            rec->converted = 0u;
        }
        rec->status = KBO_FOREIGN_INJURY_STATUS_CLOSED;
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
    kbo_foreign_injury_emit_closed_news_batch(closed_news, closed_count, today, source);
    if (out_active_count != NULL) {
        *out_active_count = active_count;
    }
    if (out_closed_count != NULL) {
        *out_closed_count = closed_count + invalid_closed_count;
    }
}
