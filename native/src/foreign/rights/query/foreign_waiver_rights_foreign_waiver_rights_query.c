#include "../internal/foreign_waiver_rights_internal.h"

/* Foreign reserve-right lookup helpers. */

static volatile LONG g_kbo_foreign_waiver_rights_lookup_loaded = 0;
static char g_kbo_foreign_waiver_rights_lookup_loaded_path[MAX_PATH] = {0};

static int kbo_current_foreign_waiver_rights_lookup_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (!kbo_get_foreign_waiver_rights_path(out, out_size)) {
        out[0] = '\0';
        return 0;
    }
    return out[0] != '\0';
}

int kbo_foreign_waiver_rights_lookup_context_ready(void)
{
    char current_path[MAX_PATH] = {0};
    if (!kbo_current_foreign_waiver_rights_lookup_path(current_path, sizeof(current_path))) {
        return 0;
    }
    return InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lookup_loaded, 0, 0) == 1
        && strcmp(g_kbo_foreign_waiver_rights_lookup_loaded_path, current_path) == 0;
}

void kbo_ensure_foreign_waiver_rights_loaded_for_lookup(void)
{
    static volatile LONG load_in_progress = 0;
    static volatile LONG last_attempt_tick = 0;
    static char last_attempt_path[MAX_PATH] = {0};

    char current_path[MAX_PATH] = {0};
    int current_path_ready = kbo_current_foreign_waiver_rights_lookup_path(
        current_path,
        sizeof(current_path));

    if (current_path_ready && kbo_foreign_waiver_rights_lookup_context_ready()) {
        return;
    }

    DWORD now = GetTickCount();
    LONG last = InterlockedCompareExchange(&last_attempt_tick, 0, 0);
    if (last != 0
            && now - (DWORD)last < 1000u
            && ((!current_path_ready && last_attempt_path[0] == '\0')
                || (current_path_ready && strcmp(last_attempt_path, current_path) == 0))) {
        return;
    }

    if (InterlockedCompareExchange(&load_in_progress, 1, 0) != 0) {
        return;
    }

    InterlockedExchange(&last_attempt_tick, (LONG)now);
    snprintf(last_attempt_path, sizeof(last_attempt_path), "%s", current_path_ready ? current_path : "");
    if (kbo_load_foreign_waiver_rights()) {
        if (current_path_ready) {
            snprintf(
                g_kbo_foreign_waiver_rights_lookup_loaded_path,
                sizeof(g_kbo_foreign_waiver_rights_lookup_loaded_path),
                "%s",
                current_path);
            InterlockedExchange(&g_kbo_foreign_waiver_rights_lookup_loaded, 1);
        } else {
            g_kbo_foreign_waiver_rights_lookup_loaded_path[0] = '\0';
            InterlockedExchange(&g_kbo_foreign_waiver_rights_lookup_loaded, 0);
        }
    }
    InterlockedExchange(&load_in_progress, 0);
}

int kbo_has_active_foreign_waiver_right(uint32_t team_id, uint32_t player_id, uint32_t today_yyyymmdd)
{
    if (team_id == 0u || player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
    int result = 0;
    kbo_lock_enter(&g_kbo_foreign_waiver_rights_lock);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == team_id && rec->player_id == player_id && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            result = 1;
            break;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_waiver_rights_lock);
    return result;
}

int kbo_get_active_foreign_waiver_right_dates(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t today_yyyymmdd,
    uint32_t* out_retained_on,
    uint32_t* out_expires_on)
{
    if (out_retained_on != NULL) {
        *out_retained_on = 0u;
    }
    if (out_expires_on != NULL) {
        *out_expires_on = 0u;
    }
    if (team_id == 0u || player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }

    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();

    int result = 0;
    kbo_lock_enter(&g_kbo_foreign_waiver_rights_lock);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == team_id
                && rec->player_id == player_id
                && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            if (out_retained_on != NULL) {
                *out_retained_on = rec->retained_on_yyyymmdd;
            }
            if (out_expires_on != NULL) {
                *out_expires_on = rec->expires_on_yyyymmdd;
            }
            result = 1;
            break;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_waiver_rights_lock);
    return result;
}

int kbo_find_active_foreign_waiver_holder(uint32_t player_id, uint32_t today_yyyymmdd, uint32_t* out_team_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0;
    }
    if (player_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }

    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();

    enum {
        KBO_FOREIGN_WAIVER_HOLDER_CACHE_SIZE = 8192
    };
    typedef struct KboForeignWaiverHolderCacheEntry {
        uint32_t player_id;
        uint32_t today_yyyymmdd;
        LONG generation;
        uint32_t holder_team_id;
        uint8_t found;
        uint8_t valid;
    } KboForeignWaiverHolderCacheEntry;
    static KboForeignWaiverHolderCacheEntry holder_cache[KBO_FOREIGN_WAIVER_HOLDER_CACHE_SIZE] = {{0}};

    LONG generation = InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_generation, 0, 0);
    uint32_t slot_index = ((player_id * 2654435761u) ^ (today_yyyymmdd * 2246822519u))
        & (KBO_FOREIGN_WAIVER_HOLDER_CACHE_SIZE - 1u);
    KboForeignWaiverHolderCacheEntry* cached = &holder_cache[slot_index];
    if (cached->valid
            && cached->player_id == player_id
            && cached->today_yyyymmdd == today_yyyymmdd
            && cached->generation == generation) {
        if (cached->found && out_team_id != NULL) {
            *out_team_id = cached->holder_team_id;
        }
        return cached->found ? 1 : 0;
    }

    int found = 0;
    uint32_t holder_team_id = 0;
    kbo_lock_enter(&g_kbo_foreign_waiver_rights_lock);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == player_id && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            holder_team_id = rec->team_id;
            found = holder_team_id != 0u;
            break;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_waiver_rights_lock);

    if (found && out_team_id != NULL) {
        *out_team_id = holder_team_id;
    }
    cached->player_id = player_id;
    cached->today_yyyymmdd = today_yyyymmdd;
    cached->generation = generation;
    cached->holder_team_id = holder_team_id;
    cached->found = found ? 1u : 0u;
    cached->valid = 1u;
    return found;
}

