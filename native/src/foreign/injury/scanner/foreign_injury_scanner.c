#include "foreign_injury_scanner_internal.h"
#include "loop/foreign_injury_scanner_player_loop.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"

static void kbo_foreign_injury_replacement_scan_for_date_mode(
    const char* source,
    uint32_t today,
    int process_existing_replacements,
    int captured_live_date);

void kbo_foreign_injury_replacement_scan_captured_date(const char* source, uint32_t today)
{
    kbo_foreign_injury_replacement_scan_for_date_mode(source, today, 1, 1);
}

void kbo_foreign_injury_replacement_scan_discovery_for_date(const char* source, uint32_t today)
{
    kbo_foreign_injury_replacement_scan_for_date_mode(source, today, 0, 0);
}

static void kbo_foreign_injury_replacement_scan_for_date_mode(
    const char* source,
    uint32_t today,
    int process_existing_replacements,
    int captured_live_date)
{
    KBO_PROFILE_BEGIN(profile_foreign_injury_scan);
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "foreign_injury_replacement_scan")) {
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.save_pause_abort");
        return;
    }
    if (!kbo_foreign_injury_replacement_enabled()) {
        kbo_rule_audit_emit_fields("foreign_injury.replacement.scan", "skip", "disabled", source, NULL);
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.disabled");
        return;
    }
    if (kbo_foreign_injury_replacement_scan_source_is_read_only(source)) {
        KBO_PROFILE_BEGIN(profile_foreign_injury_load_readonly);
        kbo_ensure_foreign_injury_replacements_loaded();
        KBO_PROFILE_END(profile_foreign_injury_load_readonly, "foreign_injury.scan.readonly_load");
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.readonly");
        return;
    }
    if (today == 0u) {
        kbo_rule_audit_emit_fields("foreign_injury.replacement.scan", "skip", "date_unavailable", source, NULL);
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.no_date");
        return;
    }
    uint32_t live_date = 0u;
    (void)kbo_current_date_tick_latest_published_date(&live_date);
    int live_injury_fields_available = captured_live_date || live_date == today;
    if (captured_live_date && live_date != 0u && live_date != today) {
        kbo_log_runtimef(
            "foreign injury replacement: date hook scan source=%s event_date=%u live_date=%u reason=using_hook_date_for_live_memory",
            source != NULL ? source : "",
            today,
            live_date);
        if (today < live_date) {
            static volatile LONG stale_date_skip_log_count = 0;
            LONG stale_slot = InterlockedIncrement(&stale_date_skip_log_count);
            if (stale_slot <= 80) {
                kbo_log_runtimef(
                    "foreign injury replacement: stale date hook scan skipped source=%s event_date=%u live_date=%u reason=coalesced_to_live_date",
                    source != NULL ? source : "",
                    today,
                    live_date);
            }
            kbo_profiler_record_us("foreign_injury.scan.stale_date_hook_skipped", 0);
            KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.stale_date_hook_skipped");
            return;
        }
    }
    if (kbo_foreign_injury_same_date_idle_scan_cached(today, source)) {
        kbo_profiler_record_us("foreign_injury.scan.same_date_idle_cached", 0);
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.same_date_idle_cached");
        return;
    }
    if (!process_existing_replacements) {
        kbo_profiler_record_us("foreign_injury.scan.discovery_only_opening_disabled_memory_ssot", 0);
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.discovery_only_disabled");
        return;
    }
    KBO_PROFILE_BEGIN(profile_foreign_injury_load);
    kbo_ensure_foreign_injury_replacements_loaded();
    KBO_PROFILE_END(profile_foreign_injury_load, "foreign_injury.scan.load_records");
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.scan",
                "skip",
                "player_vector_unavailable",
                source,
                &audit_fields);
        } while (0);
        KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.no_player_vector");
        return;
    }
    uint32_t configured_league_id = kbo_resolve_kbo_league_id();
    int slot_opening_allowed = kbo_foreign_injury_replacement_in_season_window(
        configured_league_id,
        today,
        source,
        "scan_open_slot");
    KboForeignInjuryScannerPlayerLoopResult loop_result = {0};
    if (!slot_opening_allowed) {
        kbo_profiler_record_us("foreign_injury.scan.player_loop_skipped_closed_window", 0);
    } else {
        loop_result = kbo_foreign_injury_scan_player_loop(
            player_vector,
            player_count,
            configured_league_id,
            today,
            live_date,
            live_injury_fields_available,
            process_existing_replacements,
            captured_live_date,
            source);
    }
    int scanned = loop_result.scanned;
    int opened = loop_result.opened;
    int active_count = 0;
    int closed_count = 0;
    if (process_existing_replacements) {
        uint64_t existing_fingerprint = kbo_foreign_injury_replacement_fingerprint();
        if (opened == 0
                && kbo_foreign_injury_same_date_existing_idle_cached(today, existing_fingerprint)) {
            kbo_profiler_record_us("foreign_injury.scan.existing_replacements_cached", 0);
        } else {
            KBO_PROFILE_BEGIN(profile_foreign_injury_existing);
            kbo_foreign_injury_process_existing_replacements(today, source, &active_count, &closed_count);
            KBO_PROFILE_END(profile_foreign_injury_existing, "foreign_injury.scan.existing_replacements");
            kbo_foreign_injury_note_same_date_existing_idle(
                today,
                existing_fingerprint,
                active_count == 0 && closed_count == 0);
        }
    } else {
        kbo_profiler_record_us("foreign_injury.scan.discovery_only_existing_skipped", 0);
    }
    if (opened > 0 || active_count > 0 || closed_count > 0) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today);
            kbo_log_field_i32(&audit_fields, "scanned_foreign", scanned);
            kbo_log_field_i32(&audit_fields, "opened", opened);
            kbo_log_field_i32(&audit_fields, "activated", active_count);
            kbo_log_field_i32(&audit_fields, "closed", closed_count);
            kbo_log_field_u32(&audit_fields, "discovery_only", process_existing_replacements ? 0u : 1u);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.scan",
                "process",
                "lifecycle_changes",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign injury replacement: scan source=%s scanned_foreign=%d opened=%d active=%d pending=%d closed=%d discovery_only=%d",
            source != NULL ? source : "",
            scanned,
            opened,
            active_count,
            0,
            closed_count,
            process_existing_replacements ? 0 : 1);
    }
    kbo_foreign_injury_note_same_date_idle_scan(
        today,
        source,
        opened == 0 && active_count == 0 && closed_count == 0);
    KBO_PROFILE_END(profile_foreign_injury_scan, "foreign_injury.scan.total");
}
