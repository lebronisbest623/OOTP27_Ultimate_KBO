#include "../foreign_injury_scanner_internal.h"

#include "../../../../core/dates/core_text_date.h"

#define KBO_FOREIGN_INJURY_MEMORY_PROBE_MAX 2048
#define KBO_FOREIGN_INJURY_MEMORY_PROBE_BASE 0x820u
#define KBO_FOREIGN_INJURY_MEMORY_PROBE_BYTES 0x200u
#define KBO_FOREIGN_INJURY_MEMORY_PROBE_FOCUS_BASE 0x840u
#define KBO_FOREIGN_INJURY_MEMORY_PROBE_FOCUS_BYTES 0x80u

typedef struct KboForeignInjuryMemoryProbeSnapshot {
    uint32_t player_id;
    uint32_t team_id;
    uint32_t league_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t loan_team_id;
    uint32_t original_team_id;
    uint32_t default_team_id;
    uint32_t date;
    int16_t injury_aux_0876;
    uint8_t slot_type;
    uint8_t injury_active;
    uint8_t inactive_roster_present;
    uint8_t has_assignment;
    uint8_t restricted;
    uint8_t secondary_restricted;
    uint8_t dfa;
    uint8_t loan_active;
    uint8_t bytes[KBO_FOREIGN_INJURY_MEMORY_PROBE_BYTES];
} KboForeignInjuryMemoryProbeSnapshot;

typedef struct KboForeignInjuryMemoryProbeRecord {
    uint8_t active;
    uint8_t has_transition;
    uint32_t first_seen_date;
    uint32_t last_seen_date;
    KboForeignInjuryMemoryProbeSnapshot last;
    KboForeignInjuryMemoryProbeSnapshot transition_before;
    KboForeignInjuryMemoryProbeSnapshot transition_after;
} KboForeignInjuryMemoryProbeRecord;

static KboLock g_kbo_foreign_injury_memory_probe_lock = KBO_LOCK_INIT;
static KboForeignInjuryMemoryProbeRecord
    g_kbo_foreign_injury_memory_probe_records[KBO_FOREIGN_INJURY_MEMORY_PROBE_MAX];

static uint32_t kbo_foreign_injury_memory_probe_read_u32(uint8_t* player, uint32_t offset)
{
    return player != NULL && memory_range_readable(player + offset, sizeof(uint32_t))
        ? *(uint32_t*)(player + offset)
        : 0u;
}

static int kbo_foreign_injury_memory_probe_capture(
    uint8_t* player,
    uint32_t player_id,
    uint32_t team_id,
    uint32_t league_id,
    uint8_t slot_type,
    uint32_t today,
    uint8_t injury_active,
    int16_t injury_aux_0876,
    int inactive_roster_present,
    int has_assignment,
    KboForeignInjuryMemoryProbeSnapshot* out)
{
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (player == NULL
            || player_id == 0u
            || !memory_range_readable(
                player + KBO_FOREIGN_INJURY_MEMORY_PROBE_BASE,
                KBO_FOREIGN_INJURY_MEMORY_PROBE_BYTES)) {
        return 0;
    }

    out->player_id = player_id;
    out->team_id = team_id;
    out->league_id = league_id;
    out->current_team_id = kbo_foreign_injury_memory_probe_read_u32(
        player,
        OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    out->active_team_id = kbo_foreign_injury_memory_probe_read_u32(
        player,
        OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    out->loan_team_id = kbo_foreign_injury_memory_probe_read_u32(
        player,
        OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    out->original_team_id = kbo_foreign_injury_memory_probe_read_u32(
        player,
        OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    out->default_team_id = kbo_foreign_injury_memory_probe_read_u32(
        player,
        OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    out->date = today;
    out->injury_aux_0876 = injury_aux_0876;
    out->slot_type = slot_type;
    out->injury_active = injury_active;
    out->inactive_roster_present = inactive_roster_present ? 1u : 0u;
    out->has_assignment = has_assignment ? 1u : 0u;
    out->restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    out->secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    out->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    out->loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
    memcpy(
        out->bytes,
        player + KBO_FOREIGN_INJURY_MEMORY_PROBE_BASE,
        KBO_FOREIGN_INJURY_MEMORY_PROBE_BYTES);
    return 1;
}

static int kbo_foreign_injury_memory_probe_interesting_initial(
    const KboForeignInjuryMemoryProbeSnapshot* snap)
{
    return snap != NULL
        && (snap->injury_active != 0u
            || snap->injury_aux_0876 > 0
            || snap->inactive_roster_present != 0u
            || snap->restricted != 0u
            || snap->secondary_restricted != 0u
            || snap->dfa != 0u);
}

static int kbo_foreign_injury_memory_probe_changed(
    const KboForeignInjuryMemoryProbeSnapshot* before,
    const KboForeignInjuryMemoryProbeSnapshot* after)
{
    if (before == NULL || after == NULL) {
        return 0;
    }
    if (before->injury_active != after->injury_active
        || before->injury_aux_0876 != after->injury_aux_0876
        || before->inactive_roster_present != after->inactive_roster_present
        || before->restricted != after->restricted
        || before->secondary_restricted != after->secondary_restricted
        || before->dfa != after->dfa
        || before->loan_active != after->loan_active
        || before->current_team_id != after->current_team_id
        || before->active_team_id != after->active_team_id
        || before->loan_team_id != after->loan_team_id
        || before->original_team_id != after->original_team_id
        || before->default_team_id != after->default_team_id) {
        return 1;
    }

    if (!kbo_foreign_injury_memory_probe_interesting_initial(before)
            && !kbo_foreign_injury_memory_probe_interesting_initial(after)) {
        return 0;
    }
    return memcmp(
        before->bytes,
        after->bytes,
        KBO_FOREIGN_INJURY_MEMORY_PROBE_BYTES) != 0;
}

static const char* kbo_foreign_injury_memory_probe_reason(
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

static int kbo_foreign_injury_memory_probe_reason_is_anchor(const char* reason)
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

static void kbo_foreign_injury_memory_probe_emit_transition(
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

void kbo_foreign_injury_memory_probe_observe(
    uint8_t* player,
    uint32_t player_id,
    uint32_t team_id,
    uint32_t league_id,
    uint8_t slot_type,
    uint32_t today,
    uint8_t injury_active,
    int16_t injury_aux_0876,
    int inactive_roster_present,
    int has_assignment,
    const char* source)
{
    if (today == 0u || player_id == 0u) {
        return;
    }

    KboForeignInjuryMemoryProbeSnapshot current;
    if (!kbo_foreign_injury_memory_probe_capture(
            player,
            player_id,
            team_id,
            league_id,
            slot_type,
            today,
            injury_active,
            injury_aux_0876,
            inactive_roster_present,
            has_assignment,
            &current)) {
        return;
    }

    KboForeignInjuryMemoryProbeSnapshot before;
    memset(&before, 0, sizeof(before));
    KboForeignInjuryMemoryProbeSnapshot after;
    memset(&after, 0, sizeof(after));
    int emit = 0;
    int initial = 0;

    kbo_lock_enter(&g_kbo_foreign_injury_memory_probe_lock);
    int slot = -1;
    int free_slot = -1;
    for (int i = 0; i < KBO_FOREIGN_INJURY_MEMORY_PROBE_MAX; i++) {
        if (g_kbo_foreign_injury_memory_probe_records[i].active
                && g_kbo_foreign_injury_memory_probe_records[i].last.player_id == player_id) {
            slot = i;
            break;
        }
        if (!g_kbo_foreign_injury_memory_probe_records[i].active && free_slot < 0) {
            free_slot = i;
        }
    }

    if (slot < 0) {
        slot = free_slot >= 0 ? free_slot : 0;
        KboForeignInjuryMemoryProbeRecord* rec = &g_kbo_foreign_injury_memory_probe_records[slot];
        memset(rec, 0, sizeof(*rec));
        rec->active = 1u;
        rec->first_seen_date = today;
        rec->last_seen_date = today;
        rec->last = current;
        if (kbo_foreign_injury_memory_probe_interesting_initial(&current)) {
            rec->has_transition = 1u;
            rec->transition_after = current;
            after = current;
            emit = 1;
            initial = 1;
        }
    } else {
        KboForeignInjuryMemoryProbeRecord* rec = &g_kbo_foreign_injury_memory_probe_records[slot];
        if (kbo_foreign_injury_memory_probe_changed(&rec->last, &current)) {
            const char* reason = kbo_foreign_injury_memory_probe_reason(&rec->last, &current, 0);
            before = rec->last;
            after = current;
            if (!rec->has_transition
                    || kbo_foreign_injury_memory_probe_reason_is_anchor(reason)) {
                rec->transition_before = rec->last;
                rec->transition_after = current;
                rec->has_transition = 1u;
            }
            emit = 1;
        }
        rec->last = current;
        rec->last_seen_date = today;
    }
    kbo_lock_leave(&g_kbo_foreign_injury_memory_probe_lock);

    if (emit) {
        kbo_foreign_injury_memory_probe_emit_transition(
            initial ? NULL : &before,
            &after,
            initial,
            source);
    }
}
