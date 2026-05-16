#include "../internal/foreign_injury_internal.h"
#include "../../../team/assignment/org_query/team_org_assignment_query.h"

int kbo_persist_foreign_injury_replacements_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_injury_replacement_path(path, sizeof(path))) {
        return 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("foreign injury replacement: persist failed path=%s gle=%lu", path, (unsigned long)GetLastError());
        return 0;
    }

    DWORD written = 0;
    const char* header = "team_id,league_id,injured_player_id,replacement_player_id,opened_on,expected_end,slot_type,status,converted\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        char line[256] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
            rec->team_id,
            rec->league_id,
            rec->injured_player_id,
            rec->replacement_player_id,
            rec->opened_on_yyyymmdd,
            rec->expected_end_yyyymmdd,
            (uint32_t)rec->slot_type,
            (uint32_t)rec->status,
            (uint32_t)rec->converted);
        if (len > 0 && len < (int)sizeof(line)) {
            written = 0;
            WriteFile(file, line, (DWORD)len, &written, NULL);
        }
    }

    if (!kbo_atomic_commit(file, tmp_path, path)) {
        kbo_log_runtimef("foreign injury replacement: atomic commit failed path=%s", path);
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

    static DWORD last_empty_import_attempt_tick = 0u;
    kbo_lock_foreign_injury_replacements();
    int path_changed = strcmp(g_kbo_foreign_injury_replacement_loaded_path, path) != 0;
    if (path_changed) {
        last_empty_import_attempt_tick = 0u;
        kbo_load_foreign_injury_replacements_locked(path);
    }
    DWORD now = GetTickCount();
    int should_import_seed = path_changed;
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
        kbo_get_current_yyyymmdd(&today);
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

    kbo_lock_foreign_injury_replacements();
    int loaded = strcmp(g_kbo_foreign_injury_replacement_loaded_path, path) == 0;
    kbo_unlock_foreign_injury_replacements();
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
                || rec->status == KBO_FOREIGN_INJURY_STATUS_PENDING
                || (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED && rec->converted != 0u)) {
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
    kbo_lock_foreign_injury_replacements();
    result = kbo_foreign_injury_player_excluded_from_foreign_count_locked(team_id, player_id);
    kbo_unlock_foreign_injury_replacements();
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
        if (rec->team_id == team_id
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
        kbo_get_current_yyyymmdd(&today);
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
    uint8_t injury_active = injured[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    int16_t days_left = *(int16_t*)(injured + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
    int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
    if (kbo_foreign_injury_duration_meets_minimum(days_left, min_days)) {
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
    int sql_days = 0;
    uint32_t sql_date = 0u;
    if (inactive_roster_present
            && kbo_foreign_injury_recent_sql_has_long_term_injury_date_on_date(
                rec->injured_player_id,
                min_days,
                today,
                &sql_days,
                &sql_date)) {
        uint32_t sql_expected_end = sql_date != 0u
            ? kbo_foreign_injury_expected_end_from_duration(sql_date, sql_days)
            : 0u;
        if (sql_expected_end == 0u || !kbo_foreign_injury_expected_end_reached(today, sql_expected_end)) {
            return 1;
        }
    }
    if (rec->expected_end_yyyymmdd != 0u && (injury_active != 0u || days_left > 0)) {
        return 1;
    }
    if (days_left > 0) {
        return 0;
    }
    return kbo_foreign_injury_inactive_roster_has_long_term_injury_basis(
        injury_active,
        days_left,
        min_days,
        inactive_roster_present);
}

int kbo_foreign_injury_record_has_minimum_injury_basis(const KboForeignInjuryReplacement* rec)
{
    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);
    return kbo_foreign_injury_record_has_minimum_injury_basis_on_date(rec, today);
}

int kbo_team_has_foreign_injury_slot(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id)
{
    int result = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    result = kbo_team_has_foreign_injury_slot_locked(team_id, slot_type, out_injured_player_id);
    kbo_unlock_foreign_injury_replacements();
    return result;
}

int kbo_team_has_foreign_injury_slot_for_candidate_locked(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (out_replacement_player_id != NULL) { *out_replacement_player_id = 0u; }
    if (team_id == 0u || slot_type == 0u || candidate_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->team_id != team_id
                || rec->slot_type != slot_type
                || !kbo_foreign_injury_status_uses_slot(rec->status)
                || !kbo_foreign_injury_record_has_minimum_injury_basis(rec)
                || rec->injured_player_id == candidate_player_id) {
            continue;
        }
        if (rec->replacement_player_id != 0u && rec->replacement_player_id != candidate_player_id) {
            continue;
        }
        if (kbo_foreign_injury_replacement_player_reserved_locked(candidate_player_id, rec)) {
            continue;
        }
        if (out_injured_player_id != NULL) {
            *out_injured_player_id = rec->injured_player_id;
        }
        if (out_replacement_player_id != NULL) {
            *out_replacement_player_id = rec->replacement_player_id;
        }
        return 1;
    }
    return 0;
}

int kbo_team_has_foreign_injury_slot_for_candidate(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    KBO_PROFILE_BEGIN(profile_foreign_injury_slot_candidate);
    int result = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    result = kbo_team_has_foreign_injury_slot_for_candidate_locked(
        team_id,
        slot_type,
        candidate_player_id,
        out_injured_player_id,
        out_replacement_player_id);
    kbo_unlock_foreign_injury_replacements();
    KBO_PROFILE_END(profile_foreign_injury_slot_candidate, result
        ? "foreign_injury.slot_candidate.hit"
        : "foreign_injury.slot_candidate.miss");
    return result;
}

int kbo_team_has_foreign_injury_slot_for_candidate_any(
    uint32_t team_id,
    int allow_asian_slot,
    uint32_t candidate_player_id,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (out_replacement_player_id != NULL) { *out_replacement_player_id = 0u; }
    if (team_id == 0u || candidate_player_id == 0u) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_foreign_injury_slot_candidate_any);
    int result = 0;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t replacement_player_id = 0u;

    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    if (allow_asian_slot
            && kbo_team_has_foreign_injury_slot_for_candidate_locked(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA,
                candidate_player_id,
                &injured_player_id,
                &replacement_player_id)) {
        slot_type = KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA;
        result = 1;
    }
    if (!result
            && kbo_team_has_foreign_injury_slot_for_candidate_locked(
                team_id,
                KBO_FOREIGN_INJURY_SLOT_REGULAR,
                candidate_player_id,
                &injured_player_id,
                &replacement_player_id)) {
        slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR;
        result = 1;
    }
    kbo_unlock_foreign_injury_replacements();

    if (result) {
        if (out_slot_type != NULL) { *out_slot_type = slot_type; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        if (out_replacement_player_id != NULL) { *out_replacement_player_id = replacement_player_id; }
    }
    KBO_PROFILE_END(profile_foreign_injury_slot_candidate_any, result
        ? "foreign_injury.slot_candidate_any.hit"
        : "foreign_injury.slot_candidate_any.miss");
    return result;
}

void kbo_count_foreign_injury_replacements_for_team(
    uint32_t team_id,
    int* out_open,
    int* out_pending,
    int* out_closed)
{
    if (out_open != NULL) { *out_open = 0; }
    if (out_pending != NULL) { *out_pending = 0; }
    if (out_closed != NULL) { *out_closed = 0; }
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (team_id != 0u && rec->team_id != team_id) {
            continue;
        }
        if (kbo_foreign_injury_status_uses_slot(rec->status)
                && kbo_foreign_injury_record_has_minimum_injury_basis(rec)) {
            if (out_open != NULL) { (*out_open)++; }
        } else if (rec->status == KBO_FOREIGN_INJURY_STATUS_PENDING) {
            if (out_pending != NULL) { (*out_pending)++; }
        } else if (rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            if (out_closed != NULL) { (*out_closed)++; }
        }
    }
    kbo_unlock_foreign_injury_replacements();
}

