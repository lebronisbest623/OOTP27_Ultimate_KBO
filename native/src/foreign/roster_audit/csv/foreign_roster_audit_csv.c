#include "../internal/foreign_roster_audit_internal.h"
#include "../../../core/files/atomic/core_atomic_file.h"

static char g_kbo_foreign_roster_snapshot_tmp_path[KBO_UTF8_PATH_BYTES];
static char g_kbo_foreign_roster_snapshot_dest_path[KBO_UTF8_PATH_BYTES];

int kbo_foreign_roster_audit_csv_empty(HANDLE file)
{
    DWORD high = 0;
    uint32_t low = GetFileSize(file, &high);
    if (high != 0) {
        return 0;
    }
    return low == 0;
}

uint32_t kbo_foreign_roster_audit_get_player_original_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
}

void kbo_capture_foreign_roster_audit_state(uint8_t* player, KboForeignRosterAuditState* out)
{
    memset(out, 0, sizeof(*out));
    out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    out->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    out->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    out->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    out->original_team_id = kbo_foreign_roster_audit_get_player_original_team_id(player);
    out->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    out->loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    out->restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    out->secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    out->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    out->loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
    out->injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    out->score = kbo_foreign_waiver_value_score(player);
}

int kbo_foreign_roster_audit_state_changed(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    return old_state->nation_id != new_state->nation_id
        || old_state->current_team_id != new_state->current_team_id
        || old_state->active_team_id != new_state->active_team_id
        || old_state->original_team_id != new_state->original_team_id
        || old_state->current_league_id != new_state->current_league_id
        || old_state->loan_team_id != new_state->loan_team_id
        || old_state->restricted != new_state->restricted
        || old_state->secondary_restricted != new_state->secondary_restricted
        || old_state->dfa != new_state->dfa
        || old_state->loan_active != new_state->loan_active
        || old_state->injury_active != new_state->injury_active
        || old_state->score != new_state->score;
}

int append_foreign_roster_audit_csv_header(HANDLE file)
{
    const char* header =
        "date,source,change_type,player_id,nation_id,"
        "old_current_team_id,new_current_team_id,"
        "old_active_team_id,new_active_team_id,"
        "old_original_team_id,new_original_team_id,"
        "old_current_league_id,new_current_league_id,"
        "old_loan_team_id,new_loan_team_id,"
        "old_restricted,new_restricted,"
        "old_secondary_restricted,new_secondary_restricted,"
        "old_dfa,new_dfa,"
        "old_loan_active,new_loan_active,"
        "old_injury_active,new_injury_active,"
        "old_foreign_value_score,new_foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

int append_foreign_roster_snapshot_csv_header(HANDLE file)
{
    const char* header =
        "date,source,player_id,nation_id,current_team_id,active_team_id,original_team_id,"
        "current_league_id,loan_team_id,restricted,secondary_restricted,dfa,loan_active,"
        "injury_active,foreign_value_score\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

HANDLE kbo_open_foreign_roster_audit_append_file(void)
{
    char path[KBO_UTF8_PATH_BYTES] = {0};
    if (!get_kbo_foreign_roster_audit_csv_path(path, sizeof(path))) {
        kbo_log_runtime_line("foreign roster audit: unable to resolve audit output path");
        return INVALID_HANDLE_VALUE;
    }

    DWORD attrs = kbo_get_file_attributes_utf8(path);
    int needs_header = (attrs == INVALID_FILE_ATTRIBUTES);
    HANDLE file = kbo_create_file_utf8(
        path,
        GENERIC_READ | FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("foreign roster audit: failed to open audit file path=%s gle=%lu", path, GetLastError());
        return INVALID_HANDLE_VALUE;
    }
    if (needs_header || kbo_foreign_roster_audit_csv_empty(file)) {
        append_foreign_roster_audit_csv_header(file);
    }
    return file;
}

void kbo_write_foreign_roster_audit_change(
    HANDLE file,
    const char* date,
    const char* source,
    const char* change_type,
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    if (file == INVALID_HANDLE_VALUE || new_state == NULL) {
        return;
    }

    KboForeignRosterAuditState zero = {0};
    if (old_state == NULL) {
        old_state = &zero;
    }

    char line[512] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%s,%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d\r\n",
        date != NULL ? date : "",
        source != NULL ? source : "",
        change_type != NULL ? change_type : "",
        new_state->player_id,
        new_state->nation_id,
        old_state->current_team_id,
        new_state->current_team_id,
        old_state->active_team_id,
        new_state->active_team_id,
        old_state->original_team_id,
        new_state->original_team_id,
        old_state->current_league_id,
        new_state->current_league_id,
        old_state->loan_team_id,
        new_state->loan_team_id,
        (uint32_t)old_state->restricted,
        (uint32_t)new_state->restricted,
        (uint32_t)old_state->secondary_restricted,
        (uint32_t)new_state->secondary_restricted,
        (uint32_t)old_state->dfa,
        (uint32_t)new_state->dfa,
        (uint32_t)old_state->loan_active,
        (uint32_t)new_state->loan_active,
        (uint32_t)old_state->injury_active,
        (uint32_t)new_state->injury_active,
        old_state->score,
        new_state->score);
    if (len <= 0 || len >= (int)sizeof(line)) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, (DWORD)len, &written, NULL);
}

void kbo_write_foreign_roster_snapshot_row(
    HANDLE file,
    const char* date,
    const char* source,
    const KboForeignRosterAuditState* state)
{
    if (file == INVALID_HANDLE_VALUE || state == NULL) {
        return;
    }

    char line[320] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d\r\n",
        date != NULL ? date : "",
        source != NULL ? source : "",
        state->player_id,
        state->nation_id,
        state->current_team_id,
        state->active_team_id,
        state->original_team_id,
        state->current_league_id,
        state->loan_team_id,
        (uint32_t)state->restricted,
        (uint32_t)state->secondary_restricted,
        (uint32_t)state->dfa,
        (uint32_t)state->loan_active,
        (uint32_t)state->injury_active,
        state->score);
    if (len <= 0 || len >= (int)sizeof(line)) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, (DWORD)len, &written, NULL);
}

HANDLE kbo_open_foreign_roster_snapshot_file(void)
{
    char path[KBO_UTF8_PATH_BYTES] = {0};
    if (!get_kbo_foreign_roster_snapshot_csv_path(path, sizeof(path))) {
        kbo_log_runtime_line("foreign roster audit: unable to resolve snapshot output path");
        return INVALID_HANDLE_VALUE;
    }

    char tmp_path[KBO_UTF8_PATH_BYTES] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("foreign roster audit: failed to open snapshot file path=%s gle=%lu", path, GetLastError());
        return INVALID_HANDLE_VALUE;
    }
    snprintf(g_kbo_foreign_roster_snapshot_tmp_path, sizeof(g_kbo_foreign_roster_snapshot_tmp_path), "%s", tmp_path);
    snprintf(g_kbo_foreign_roster_snapshot_dest_path, sizeof(g_kbo_foreign_roster_snapshot_dest_path), "%s", path);
    append_foreign_roster_snapshot_csv_header(file);
    return file;
}

void kbo_close_foreign_roster_snapshot_file(HANDLE file)
{
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    if (!kbo_atomic_commit(file, g_kbo_foreign_roster_snapshot_tmp_path, g_kbo_foreign_roster_snapshot_dest_path)) {
        kbo_log_runtimef(
            "foreign roster audit: failed to commit snapshot file path=%s gle=%lu",
            g_kbo_foreign_roster_snapshot_dest_path,
            GetLastError());
    }
    g_kbo_foreign_roster_snapshot_tmp_path[0] = '\0';
    g_kbo_foreign_roster_snapshot_dest_path[0] = '\0';
}

