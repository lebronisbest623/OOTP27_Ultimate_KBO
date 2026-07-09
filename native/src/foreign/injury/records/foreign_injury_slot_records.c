#include "../internal/foreign_injury_internal.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../team/assignment/org_query/team_org_assignment_query.h"
#include "sql/foreign_injury_replacements_sql_store.h"

static uint64_t kbo_foreign_injury_replacement_fingerprint_mix(uint64_t hash, uint64_t value)
{
    hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    return hash;
}

static volatile LONG g_kbo_foreign_injury_replacement_fingerprint_generation = 1;
static volatile LONG g_kbo_foreign_injury_replacement_cached_fingerprint_generation = 0;
static volatile LONG64 g_kbo_foreign_injury_replacement_cached_fingerprint = 0;

static void kbo_foreign_injury_replacement_fingerprint_note_changed(void)
{
    LONG generation = InterlockedIncrement(&g_kbo_foreign_injury_replacement_fingerprint_generation);
    if (generation <= 0) {
        InterlockedExchange(&g_kbo_foreign_injury_replacement_fingerprint_generation, 1);
    }
}

uint64_t kbo_foreign_injury_replacement_fingerprint(void)
{
    if (g_kbo_foreign_injury_replacement_loaded_path[0] == '\0') {
        kbo_ensure_foreign_injury_replacements_loaded();
    }

    LONG generation = InterlockedCompareExchange(
        &g_kbo_foreign_injury_replacement_fingerprint_generation,
        0,
        0);
    LONG cached_generation = InterlockedCompareExchange(
        &g_kbo_foreign_injury_replacement_cached_fingerprint_generation,
        0,
        0);
    uint64_t cached_fingerprint = (uint64_t)InterlockedCompareExchange64(
        &g_kbo_foreign_injury_replacement_cached_fingerprint,
        0,
        0);
    if (cached_fingerprint != 0ull && cached_generation == generation) {
        return cached_fingerprint;
    }

    uint64_t hash = 1469598103934665603ull;
    kbo_lock_foreign_injury_replacements_shared();
    generation = InterlockedCompareExchange(
        &g_kbo_foreign_injury_replacement_fingerprint_generation,
        0,
        0);
    hash = kbo_foreign_injury_replacement_fingerprint_mix(
        hash,
        (uint64_t)(uint32_t)g_kbo_foreign_injury_replacement_count);
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        const KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->team_id);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->league_id);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->injured_player_id);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->replacement_player_id);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->opened_on_yyyymmdd);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->expected_end_yyyymmdd);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->injury_id);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->closed_on_yyyymmdd);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->slot_type);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->status);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->converted);
        hash = kbo_foreign_injury_replacement_fingerprint_mix(hash, rec->close_choice);
    }
    kbo_unlock_foreign_injury_replacements_shared();
    if (hash == 0ull) {
        hash = 1ull;
    }
    InterlockedExchange64(&g_kbo_foreign_injury_replacement_cached_fingerprint, (LONG64)hash);
    InterlockedExchange(&g_kbo_foreign_injury_replacement_cached_fingerprint_generation, generation);
    return hash;
}

int kbo_persist_foreign_injury_replacements_locked(void)
{
    kbo_foreign_injury_replacement_fingerprint_note_changed();

    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        return 0;
    }

    if (!kbo_foreign_injury_replacements_sql_replace_all(
            g_kbo_foreign_injury_replacements,
            g_kbo_foreign_injury_replacement_count)) {
        kbo_log_runtimef("foreign injury replacement: sqlite persist failed path=%s", path);
        return 0;
    }
    snprintf(g_kbo_foreign_injury_replacement_loaded_path, sizeof(g_kbo_foreign_injury_replacement_loaded_path), "%s", path);
    return 1;
}

void kbo_ensure_foreign_injury_replacements_loaded(void)
{
    KBO_PROFILE_BEGIN(profile_foreign_injury_ensure);
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        KBO_PROFILE_END(profile_foreign_injury_ensure, "foreign_injury.ensure.no_path");
        return;
    }

    /* Fast path: shared lock for the common case where data is already loaded
       for the current save.  This allows concurrent readers to proceed without
       blocking each other. */
    kbo_lock_foreign_injury_replacements_shared();
    if (g_kbo_foreign_injury_replacement_loaded_path[0] != '\0'
            && strcmp(g_kbo_foreign_injury_replacement_loaded_path, path) == 0
            && g_kbo_foreign_injury_replacement_count > 0) {
        kbo_unlock_foreign_injury_replacements_shared();
        KBO_PROFILE_END(profile_foreign_injury_ensure, "foreign_injury.ensure.cached");
        return;
    }
    kbo_unlock_foreign_injury_replacements_shared();

    /* Slow path: exclusive lock for initial load or seed import.
       Double-check after acquiring exclusive lock. */
    kbo_lock_foreign_injury_replacements();
    if (g_kbo_foreign_injury_replacement_loaded_path[0] != '\0'
            && strcmp(g_kbo_foreign_injury_replacement_loaded_path, path) == 0
            && g_kbo_foreign_injury_replacement_count > 0) {
        kbo_unlock_foreign_injury_replacements();
        KBO_PROFILE_END(profile_foreign_injury_ensure, "foreign_injury.ensure.cached");
        return;
    }

    static DWORD last_empty_import_attempt_tick = 0u;
    DWORD now = GetTickCount();
    int should_import_seed = 0;
    int path_changed = strcmp(g_kbo_foreign_injury_replacement_loaded_path, path) != 0;
    if (path_changed) {
        last_empty_import_attempt_tick = 0u;
        kbo_load_foreign_injury_replacements_locked(path);
        kbo_foreign_injury_replacement_fingerprint_note_changed();
    }
    should_import_seed = path_changed;
    if (!should_import_seed
            && g_kbo_foreign_injury_replacement_count == 0
            && (last_empty_import_attempt_tick == 0u
                || now < last_empty_import_attempt_tick
                || now - last_empty_import_attempt_tick > 60000u)) {
        should_import_seed = 1;
    }
    if (should_import_seed) {
        last_empty_import_attempt_tick = now;
        uint32_t today = 0u;
        kbo_current_date_tick_latest_published_date(&today);
        int imported = 0;
        char save_seed_path[MAX_PATH] = {0};
        char global_seed_path[MAX_PATH] = {0};
        if (kbo_get_save_foreign_injury_replacement_seed_path(save_seed_path, sizeof(save_seed_path))) {
            imported += kbo_import_foreign_injury_replacement_seed_file_locked(save_seed_path, today, "save_seed");
        }
        if (kbo_get_global_foreign_injury_replacement_seed_path(global_seed_path, sizeof(global_seed_path))) {
            imported += kbo_import_foreign_injury_replacement_seed_file_locked(global_seed_path, today, "global_seed");
        }
        if (imported > 0) {
            kbo_persist_foreign_injury_replacements_locked();
        }
    }
    kbo_unlock_foreign_injury_replacements();
    KBO_PROFILE_END(profile_foreign_injury_ensure, should_import_seed
        ? "foreign_injury.ensure.import_checked"
        : "foreign_injury.ensure.cached");
}

int kbo_foreign_injury_replacements_loaded_for_current_save(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        return 0;
    }

    kbo_lock_foreign_injury_replacements_shared();
    int loaded = strcmp(g_kbo_foreign_injury_replacement_loaded_path, path) == 0;
    kbo_unlock_foreign_injury_replacements_shared();
    return loaded;
}

int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed)
{
    if (injured_player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->injured_player_id == injured_player_id
                && (include_closed || rec->status != KBO_FOREIGN_INJURY_STATUS_CLOSED)) {
            return i;
        }
    }
    return -1;
}

int kbo_foreign_injury_replacement_player_reserved_locked(
    uint32_t replacement_player_id,
    const KboForeignInjuryReplacement* owner_rec)
{
    if (replacement_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        const KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec == owner_rec || rec->replacement_player_id != replacement_player_id) {
            continue;
        }
        if (kbo_foreign_injury_status_uses_slot(rec->status)
                || rec->status == KBO_FOREIGN_INJURY_STATUS_PENDING) {
            return 1;
        }
    }
    return 0;
}

int kbo_foreign_injury_player_excluded_from_foreign_count(uint32_t team_id, uint32_t player_id)
{
    int result = 0;
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements_shared();
    result = kbo_foreign_injury_player_excluded_from_foreign_count_locked(team_id, player_id);
    kbo_unlock_foreign_injury_replacements_shared();
    return result;
}

int kbo_team_has_foreign_injury_slot_locked(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id)
{
    if (out_injured_player_id != NULL) {
        *out_injured_player_id = 0u;
    }
    if (team_id == 0u || slot_type == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (kbo_team_ids_share_org(rec->team_id, team_id)
                && rec->slot_type == slot_type
                && kbo_foreign_injury_status_uses_slot(rec->status)
                && kbo_foreign_injury_record_has_minimum_injury_basis(rec)) {
            if (out_injured_player_id != NULL) {
                *out_injured_player_id = rec->injured_player_id;
            }
            return 1;
        }
    }
    return 0;
}

int kbo_foreign_injury_record_has_minimum_injury_basis_on_date(
    const KboForeignInjuryReplacement* rec,
    uint32_t today)
{
    if (rec == NULL || rec->injured_player_id == 0u) {
        return 0;
    }
    if (today == 0u) {
        kbo_current_date_tick_latest_published_date(&today);
    }

    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    uint8_t* injured = kbo_find_player_by_id(rec->injured_player_id, &team_id, &league_id);
    if (injured == NULL || !memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    if (!kbo_player_current_assignment_matches_team_or_affiliate(injured, rec->team_id)) {
        return 0;
    }
    KboForeignInjuryLiveMemory live_injury;
    memset(&live_injury, 0, sizeof(live_injury));
    int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
    if (kbo_foreign_injury_read_live_memory(injured, &live_injury)
            && kbo_foreign_injury_live_memory_has_long_term_basis(&live_injury, min_days)
            && kbo_foreign_injury_live_memory_matches_record_episode(rec, &live_injury)) {
        return 1;
    }
    if (kbo_foreign_injury_live_memory_has_record_continuation_basis(rec, &live_injury, today)) {
        return 1;
    }

    int inactive_roster_present = kbo_foreign_injury_player_on_inactive_replacement_roster(
        injured,
        rec->injured_player_id,
        rec->team_id,
        today);
    int roster_hold_flag_present = injured[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
        || injured[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u
        || injured[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u;
    if ((inactive_roster_present || roster_hold_flag_present)
            && rec->expected_end_yyyymmdd != 0u) {
        return 1;
    }
    if (inactive_roster_present
            && kbo_foreign_injury_expected_end_pending(today, rec->expected_end_yyyymmdd)) {
        return 1;
    }
    if (live_injury.active != 0u || live_injury.days_left > 0) {
        return 0;
    }
    return 0;
}

int kbo_foreign_injury_record_has_minimum_injury_basis(const KboForeignInjuryReplacement* rec)
{
    uint32_t today = 0u;
    kbo_current_date_tick_latest_published_date(&today);
    return kbo_foreign_injury_record_has_minimum_injury_basis_on_date(rec, today);
}

int kbo_team_has_foreign_injury_slot(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id)
{
    int result = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements_shared();
    result = kbo_team_has_foreign_injury_slot_locked(team_id, slot_type, out_injured_player_id);
    kbo_unlock_foreign_injury_replacements_shared();
    return result;
}


