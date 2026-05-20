#include "foreign_injury_memory_probe_internal.h"

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
