#include "../internal/foreign_roster_audit_internal.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/logging/rule_audit.h"
#include "../../quota/counts/foreign_quota_counts.h"

static int kbo_foreign_roster_audit_assignment_changed(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    return old_state != NULL
        && new_state != NULL
        && (old_state->current_team_id != new_state->current_team_id
            || old_state->active_team_id != new_state->active_team_id
            || old_state->loan_team_id != new_state->loan_team_id);
}

static void kbo_foreign_roster_audit_note_quota_assignment_change(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    if (!kbo_foreign_roster_audit_assignment_changed(old_state, new_state)
            || new_state->player_id == 0u) {
        return;
    }

    kbo_foreign_org_count_cache_note_observed_assignment_change(
        old_state->current_team_id,
        old_state->active_team_id,
        old_state->loan_team_id,
        new_state->current_team_id,
        new_state->active_team_id,
        new_state->loan_team_id);
}

void audit_foreign_roster_state(const char* source, int write_snapshot)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "foreign_roster_audit")) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        static volatile LONG no_vector_log_count = 0;
        LONG slot = InterlockedIncrement(&no_vector_log_count);
        if (slot <= 5) {
            kbo_log_runtime_line("foreign roster audit: no player vector");
            kbo_rule_audit_emit_fields(
                "foreign_roster.audit",
                "skip",
                "player_vector_unavailable",
                source != NULL ? source : "foreign_roster_audit",
                NULL);
        }
        return;
    }
    if (player_vector == 0u
            || player_count <= 0
            || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT) {
        kbo_log_runtimef(
            "foreign roster audit: invalid player vector source=%s vector=0x%llx count=%d",
            source != NULL ? source : "",
            (unsigned long long)player_vector,
            player_count);
        return;
    }
    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        kbo_log_runtimef(
            "foreign roster audit: unreadable player vector source=%s vector=0x%llx count=%d",
            source != NULL ? source : "",
            (unsigned long long)player_vector,
            player_count);
        return;
    }
    uintptr_t* player_snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (player_snapshot == NULL) {
        kbo_log_runtimef(
            "foreign roster audit: player snapshot alloc failed source=%s count=%d",
            source != NULL ? source : "",
            player_count);
        return;
    }
    SIZE_T bytes_read = 0u;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            player_snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        kbo_log_runtimef(
            "foreign roster audit: player snapshot copy failed source=%s vector=0x%llx count=%d bytes=%llu",
            source != NULL ? source : "",
            (unsigned long long)player_vector,
            player_count,
            (unsigned long long)bytes_read);
        HeapFree(GetProcessHeap(), 0, player_snapshot);
        return;
    }
    if (kbo_runtime_save_in_progress()) {
        kbo_log_runtimef(
            "foreign roster audit: aborted reason=save_in_progress stage=after_player_snapshot source=%s count=%d",
            source != NULL ? source : "",
            player_count);
        HeapFree(GetProcessHeap(), 0, player_snapshot);
        return;
    }

    char save_path[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        strcpy_s(save_path, sizeof(save_path), "unknown_save");
    }
    if (strcmp(g_kbo_foreign_roster_audit_save_path, save_path) != 0) {
        g_kbo_foreign_roster_audit_count = 0;
        g_kbo_foreign_roster_audit_generation = 0u;
        memset(g_kbo_foreign_roster_audit, 0, sizeof(g_kbo_foreign_roster_audit));
        snprintf(g_kbo_foreign_roster_audit_save_path, sizeof(g_kbo_foreign_roster_audit_save_path), "%s", save_path);
        kbo_log_runtimef("foreign roster audit: baseline reset save=%s source=%s", save_path, source != NULL ? source : "");
    }

    g_kbo_foreign_roster_audit_generation++;
    if (g_kbo_foreign_roster_audit_generation == 0u) {
        g_kbo_foreign_roster_audit_generation = 1u;
        for (int i = 0; i < g_kbo_foreign_roster_audit_count; i++) {
            g_kbo_foreign_roster_audit[i].seen_generation = 0u;
        }
    }

    int baseline_scan = g_kbo_foreign_roster_audit_count == 0 && g_kbo_foreign_roster_audit_generation == 1u;
    int write_snapshot_now = write_snapshot || baseline_scan;

    char date[16] = {0};
    if (!kbo_current_date_tick_latest_history_date(date, sizeof(date), 2000u)) {
        strcpy_s(date, sizeof(date), "00000000");
    }

    HANDLE audit_file = INVALID_HANDLE_VALUE;
    HANDLE snapshot_file = write_snapshot_now ? kbo_open_foreign_roster_snapshot_file() : INVALID_HANDLE_VALUE;

    int scanned = 0;
    int foreign = 0;
    int rostered = 0;
    int active_retained = 0;
    int free_or_unassigned = 0;
    int changed = 0;
    int new_foreign = 0;
    int current_cleared = 0;
    int active_cleared = 0;
    static volatile LONG release_detail_log_count = 0;

    for (int32_t i = 0; i < player_count; i++) {
        if ((i & 0x3ff) == 0 && kbo_runtime_save_in_progress()) {
            kbo_log_runtimef(
                "foreign roster audit: aborted reason=save_in_progress stage=scan_loop source=%s index=%d count=%d",
                source != NULL ? source : "",
                i,
                player_count);
            break;
        }

        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        scanned++;
        if (!kbo_player_is_active_for_roster_scan(player)) {
            continue;
        }
        if (!kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        KboForeignRosterAuditState current = {0};
        kbo_capture_foreign_roster_audit_state(player, &current);
        if (current.player_id == 0u) {
            continue;
        }

        foreign++;
        if (current.current_team_id != 0u) {
            rostered++;
        } else if (current.active_team_id != 0u) {
            active_retained++;
        } else {
            free_or_unassigned++;
        }

        if (snapshot_file != INVALID_HANDLE_VALUE) {
            kbo_write_foreign_roster_snapshot_row(snapshot_file, date, source, &current);
        }

        KboForeignRosterAuditState* previous = kbo_find_foreign_roster_audit_state(current.player_id);
        if (previous == NULL) {
            if (g_kbo_foreign_roster_audit_count < KBO_FOREIGN_ROSTER_AUDIT_MAX) {
                previous = &g_kbo_foreign_roster_audit[g_kbo_foreign_roster_audit_count++];
                *previous = current;
                previous->seen_generation = g_kbo_foreign_roster_audit_generation;
                if (!baseline_scan) {
                    if (audit_file == INVALID_HANDLE_VALUE) {
                        audit_file = kbo_open_foreign_roster_audit_append_file();
                    }
                    kbo_write_foreign_roster_audit_change(
                        audit_file,
                        date,
                        source,
                        "NEW_FOREIGN",
                        NULL,
                        &current);
                    changed++;
                    new_foreign++;
                }
            } else {
                static volatile LONG full_log_count = 0;
                if (InterlockedIncrement(&full_log_count) <= 5) {
                    kbo_log_runtimef(
                        "foreign roster audit: state table full max=%d player=%u",
                        KBO_FOREIGN_ROSTER_AUDIT_MAX,
                        current.player_id);
                }
            }
            continue;
        }

        KboForeignRosterAuditState old = *previous;
        previous->seen_generation = g_kbo_foreign_roster_audit_generation;
        if (!kbo_foreign_roster_audit_state_changed(&old, &current)) {
            continue;
        }

        kbo_foreign_roster_audit_note_quota_assignment_change(&old, &current);

        const char* change_type = kbo_foreign_roster_audit_change_type(&old, &current);
        if (audit_file == INVALID_HANDLE_VALUE) {
            audit_file = kbo_open_foreign_roster_audit_append_file();
        }
        kbo_write_foreign_roster_audit_change(audit_file, date, source, change_type, &old, &current);

        if (old.current_team_id != 0u && current.current_team_id == 0u) {
            current_cleared++;
            LONG slot = InterlockedIncrement(&release_detail_log_count);
            if (slot <= 240) {
                kbo_log_runtimef(
                    "foreign roster audit: %s player=%u nation=%u current=%u->%u active=%u->%u original=%u->%u restricted=%u->%u secondary=%u->%u dfa=%u->%u loan=%u->%u injury=%u->%u score=%d->%d date=%s source=%s",
                    change_type,
                    current.player_id,
                    current.nation_id,
                    old.current_team_id,
                    current.current_team_id,
                    old.active_team_id,
                    current.active_team_id,
                    old.original_team_id,
                    current.original_team_id,
                    (uint32_t)old.restricted,
                    (uint32_t)current.restricted,
                    (uint32_t)old.secondary_restricted,
                    (uint32_t)current.secondary_restricted,
                    (uint32_t)old.dfa,
                    (uint32_t)current.dfa,
                    (uint32_t)old.loan_active,
                    (uint32_t)current.loan_active,
                    (uint32_t)old.injury_active,
                    (uint32_t)current.injury_active,
                    old.score,
                    current.score,
                    date,
                    source != NULL ? source : "");
            }
        }
        if (old.active_team_id != 0u && current.active_team_id == 0u) {
            active_cleared++;
        }

        current.seen_generation = g_kbo_foreign_roster_audit_generation;
        *previous = current;
        changed++;
    }

    if (audit_file != INVALID_HANDLE_VALUE) {
        CloseHandle(audit_file);
    }
    if (snapshot_file != INVALID_HANDLE_VALUE) {
        kbo_close_foreign_roster_snapshot_file(snapshot_file);
    }
    HeapFree(GetProcessHeap(), 0, player_snapshot);

    if (baseline_scan || changed > 0) {
        kbo_log_runtimef(
            "foreign roster audit: source=%s date=%s scanned=%d foreign=%d rostered=%d active_retained=%d free=%d changed=%d new=%d current_cleared=%d active_cleared=%d snapshot=%d save=%s",
            source != NULL ? source : "",
            date,
            scanned,
            foreign,
            rostered,
            active_retained,
            free_or_unassigned,
            changed,
            new_foreign,
            current_cleared,
            active_cleared,
            write_snapshot_now,
            g_kbo_foreign_roster_audit_save_path);
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_i32(&audit_fields, "scanned", scanned);
            kbo_log_field_i32(&audit_fields, "foreign", foreign);
            kbo_log_field_i32(&audit_fields, "rostered", rostered);
            kbo_log_field_i32(&audit_fields, "active_retained", active_retained);
            kbo_log_field_i32(&audit_fields, "free", free_or_unassigned);
            kbo_log_field_i32(&audit_fields, "changed", changed);
            kbo_log_field_i32(&audit_fields, "new_foreign", new_foreign);
            kbo_log_field_i32(&audit_fields, "current_cleared", current_cleared);
            kbo_log_field_i32(&audit_fields, "active_cleared", active_cleared);
            kbo_log_field_i32(&audit_fields, "snapshot", write_snapshot_now);
            kbo_log_field_u32(&audit_fields, "generation", g_kbo_foreign_roster_audit_generation);
            kbo_rule_audit_emit_fields(
                "foreign_roster.audit",
                baseline_scan ? "baseline" : "record_changes",
                baseline_scan ? "baseline_scan" : "state_changes",
                source != NULL ? source : "foreign_roster_audit",
                &audit_fields);
        } while (0);
    }
}
