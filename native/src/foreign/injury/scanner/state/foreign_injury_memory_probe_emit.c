#include "foreign_injury_memory_probe_internal.h"

const char* kbo_foreign_injury_memory_probe_reason(
    const KboForeignInjuryMemoryProbeSnapshot* before,
    const KboForeignInjuryMemoryProbeSnapshot* after,
    int initial)
{
    if (initial) {
        return "initial_interesting_memory_state";
    }
    if (before == NULL || after == NULL) {
        return "memory_state_changed";
    }
    if (before->injury_active == 0u && after->injury_active != 0u) {
        return "injury_active_turned_on";
    }
    if (before->inactive_roster_present == 0u && after->inactive_roster_present != 0u) {
        return "inactive_roster_turned_on";
    }
    if (before->injury_aux_0876 != after->injury_aux_0876) {
        return "injury_aux_0876_changed";
    }
    if (before->restricted != after->restricted
            || before->secondary_restricted != after->secondary_restricted
            || before->dfa != after->dfa
            || before->loan_active != after->loan_active) {
        return "roster_hold_flags_changed";
    }
    if (before->current_team_id != after->current_team_id
            || before->active_team_id != after->active_team_id
            || before->loan_team_id != after->loan_team_id
            || before->original_team_id != after->original_team_id
            || before->default_team_id != after->default_team_id) {
        return "team_assignment_changed";
    }
    return "near_injury_memory_changed";
}

int kbo_foreign_injury_memory_probe_reason_is_anchor(const char* reason)
{
    return reason != NULL
        && (strcmp(reason, "injury_active_turned_on") == 0
            || strcmp(reason, "inactive_roster_turned_on") == 0
            || strcmp(reason, "injury_aux_0876_changed") == 0
            || strcmp(reason, "roster_hold_flags_changed") == 0
            || strcmp(reason, "initial_interesting_memory_state") == 0);
}

static int kbo_foreign_injury_memory_probe_count_changed_bytes(
    const uint8_t* before,
    const uint8_t* after)
{
    if (before == NULL || after == NULL) {
        return 0;
    }
    int changed = 0;
    for (uint32_t i = 0u; i < KBO_FOREIGN_INJURY_MEMORY_PROBE_BYTES; i++) {
        if (before[i] != after[i]) {
            changed++;
        }
    }
    return changed;
}

static void kbo_foreign_injury_memory_probe_append(
    char* out,
    size_t out_size,
    const char* text)
{
    if (out == NULL || out_size == 0u || text == NULL || text[0] == '\0') {
        return;
    }
    size_t used = strlen(out);
    if (used >= out_size - 1u) {
        return;
    }
    snprintf(out + used, out_size - used, "%s", text);
}

static void kbo_foreign_injury_memory_probe_format_word_diff(
    const uint8_t* before,
    const uint8_t* after,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (before == NULL || after == NULL) {
        return;
    }

    int emitted = 0;
    for (uint32_t rel = 0u;
            rel + sizeof(uint16_t) <= KBO_FOREIGN_INJURY_MEMORY_PROBE_BYTES && emitted < 28;
            rel += 2u) {
        uint16_t before_u16 = *(const uint16_t*)(before + rel);
        uint16_t after_u16 = *(const uint16_t*)(after + rel);
        if (before_u16 == after_u16) {
            continue;
        }
        char item[80] = {0};
        snprintf(
            item,
            sizeof(item),
            "%s0x%x:%u>%u",
            emitted == 0 ? "" : ";",
            KBO_FOREIGN_INJURY_MEMORY_PROBE_BASE + rel,
            (uint32_t)before_u16,
            (uint32_t)after_u16);
        kbo_foreign_injury_memory_probe_append(out, out_size, item);
        emitted++;
    }
    if (emitted >= 28) {
        kbo_foreign_injury_memory_probe_append(out, out_size, ";...");
    }
}

static void kbo_foreign_injury_memory_probe_format_focus_hex(
    const uint8_t* bytes,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (bytes == NULL) {
        return;
    }
    uint32_t rel = KBO_FOREIGN_INJURY_MEMORY_PROBE_FOCUS_BASE
        - KBO_FOREIGN_INJURY_MEMORY_PROBE_BASE;
    size_t used = 0u;
    for (uint32_t i = 0u;
            i < KBO_FOREIGN_INJURY_MEMORY_PROBE_FOCUS_BYTES && used + 4u < out_size;
            i++) {
        int written = snprintf(
            out + used,
            out_size - used,
            "%s%02X",
            i == 0u ? "" : " ",
            bytes[rel + i]);
        if (written <= 0) {
            break;
        }
        used += (size_t)written;
    }
}

void kbo_foreign_injury_memory_probe_emit_transition(
    const KboForeignInjuryMemoryProbeSnapshot* before,
    const KboForeignInjuryMemoryProbeSnapshot* after,
    int initial,
    const char* source)
{
    if (after == NULL) {
        return;
    }

    char changed_offsets[512] = {0};
    char focus_before[512] = {0};
    char focus_after[512] = {0};
    int changed_bytes = before != NULL
        ? kbo_foreign_injury_memory_probe_count_changed_bytes(before->bytes, after->bytes)
        : 0;
    if (before != NULL) {
        kbo_foreign_injury_memory_probe_format_word_diff(
            before->bytes,
            after->bytes,
            changed_offsets,
            sizeof(changed_offsets));
        kbo_foreign_injury_memory_probe_format_focus_hex(
            before->bytes,
            focus_before,
            sizeof(focus_before));
    }
    kbo_foreign_injury_memory_probe_format_focus_hex(
        after->bytes,
        focus_after,
        sizeof(focus_after));

    const char* reason = kbo_foreign_injury_memory_probe_reason(before, after, initial);
    do {
        KboLogFields fields;
        kbo_log_fields_init(&fields);
        kbo_log_field_u32(&fields, "date", after->date);
        kbo_log_field_u32(&fields, "player_id", after->player_id);
        kbo_log_field_u32(&fields, "team_id", after->team_id);
        kbo_log_field_u32(&fields, "league_id", after->league_id);
        kbo_log_field_u32(&fields, "current_team_id", after->current_team_id);
        kbo_log_field_u32(&fields, "active_team_id", after->active_team_id);
        kbo_log_field_u32(&fields, "loan_team_id", after->loan_team_id);
        kbo_log_field_u32(&fields, "original_team_id", after->original_team_id);
        kbo_log_field_u32(&fields, "default_team_id", after->default_team_id);
        kbo_log_field_u32(&fields, "injury_active_before", before != NULL ? before->injury_active : 0u);
        kbo_log_field_u32(&fields, "injury_active_after", after->injury_active);
        kbo_log_field_i32(&fields, "injury_aux_0876_before", before != NULL ? before->injury_aux_0876 : 0);
        kbo_log_field_i32(&fields, "injury_aux_0876_after", after->injury_aux_0876);
        kbo_log_field_u32(&fields, "inactive_roster_before", before != NULL ? before->inactive_roster_present : 0u);
        kbo_log_field_u32(&fields, "inactive_roster_after", after->inactive_roster_present);
        kbo_log_field_u32(&fields, "restricted_before", before != NULL ? before->restricted : 0u);
        kbo_log_field_u32(&fields, "restricted_after", after->restricted);
        kbo_log_field_u32(&fields, "secondary_restricted_before", before != NULL ? before->secondary_restricted : 0u);
        kbo_log_field_u32(&fields, "secondary_restricted_after", after->secondary_restricted);
        kbo_log_field_u32(&fields, "dfa_before", before != NULL ? before->dfa : 0u);
        kbo_log_field_u32(&fields, "dfa_after", after->dfa);
        kbo_log_field_u32(&fields, "loan_active_before", before != NULL ? before->loan_active : 0u);
        kbo_log_field_u32(&fields, "loan_active_after", after->loan_active);
        kbo_log_field_i32(&fields, "changed_bytes", changed_bytes);
        kbo_log_field_str(&fields, "changed_offsets", changed_offsets);
        kbo_log_field_str(&fields, "focus_before_0x840_0x8bf", focus_before);
        kbo_log_field_str(&fields, "focus_after_0x840_0x8bf", focus_after);
        kbo_rule_audit_emit_fields(
            "foreign_injury.memory_probe",
            "observe",
            reason,
            source,
            &fields);
    } while (0);

    kbo_log_runtimef(
        "foreign injury memory probe: %s source=%s date=%u player=%u team=%u league=%u injury=%u->%u aux0876=%d->%d inactive=%u->%u changed_bytes=%d offsets=%s",
        reason,
        source != NULL ? source : "",
        after->date,
        after->player_id,
        after->team_id,
        after->league_id,
        before != NULL ? (uint32_t)before->injury_active : 0u,
        (uint32_t)after->injury_active,
        before != NULL ? (int)before->injury_aux_0876 : 0,
        (int)after->injury_aux_0876,
        before != NULL ? (uint32_t)before->inactive_roster_present : 0u,
        (uint32_t)after->inactive_roster_present,
        changed_bytes,
        changed_offsets);
}
