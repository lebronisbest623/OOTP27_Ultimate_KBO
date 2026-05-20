#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/csv/core_csv.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/logging/rule_audit.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../../waiver_core/api/foreign_waiver_core.h"
#include "../api/foreign_waiver_decisions.h"
#include "../internal/foreign_waiver_decisions_state_internal.h"
#include "../internal/foreign_waiver_decisions_team_internal.h"

#define KBO_FOREIGN_WAIVER_COMMAND_FILE "config\\foreign_waiver_commands.txt"

static void kbo_audit_foreign_waiver_command(
    const char* decision,
    const char* reason,
    const char* action,
    int line_no,
    uint32_t team_id,
    uint32_t player_id,
    uint32_t fallback_league)
{
    KboLogFields fields;
    kbo_log_fields_init(&fields);
    kbo_log_field_i32(&fields, "line", line_no);
    if (action != NULL && action[0] != '\0') {
        kbo_log_field_str(&fields, "action", action);
    }
    kbo_log_field_u32(&fields, "team_id", team_id);
    kbo_log_field_u32(&fields, "player_id", player_id);
    if (fallback_league != 0u) {
        kbo_log_field_u32(&fields, "fallback_league", fallback_league);
    }
    kbo_rule_audit_emit_fields(
        "foreign_waiver.command",
        decision,
        reason,
        "user",
        &fields);
}

static int kbo_execute_foreign_waiver_claim(const char* line, int line_no)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }

    if (line[0] == '#' || line[0] == ';') {
        return 0;
    }

    char raw[512];
    size_t raw_len = 0;
    for (; raw_len < sizeof(raw) - 1 && line[raw_len] != '\0' && line[raw_len] != '\r' && line[raw_len] != '\n'; raw_len++) {
        raw[raw_len] = line[raw_len];
    }
    raw[raw_len] = '\0';

    const char* p = raw;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    char action_name[16] = {0};
    size_t idx = 0;
    while (*p != '\0' && *p != ',' && *p != ' ' && *p != '\t' && idx + 1 < sizeof(action_name)) {
        action_name[idx++] = *p++;
    }
    action_name[idx] = '\0';

    int retain_rights =
        strcmp(action_name, "RETAIN") == 0 || strcmp(action_name, "retain") == 0
        || strcmp(action_name, "RIGHTS") == 0 || strcmp(action_name, "rights") == 0
        || strcmp(action_name, "RESERVE") == 0 || strcmp(action_name, "reserve") == 0;
    int skip_rights =
        strcmp(action_name, "SKIP") == 0 || strcmp(action_name, "skip") == 0;

    if (strcmp(action_name, "CLAIM") != 0 && strcmp(action_name, "claim") != 0
            && strcmp(action_name, "MOVE") != 0 && strcmp(action_name, "move") != 0
            && !retain_rights && !skip_rights) {
        return 0;
    }

    if (*p == ',') {
        p++;
    } else if (*p != '\0') {
        while (*p != '\0' && *p != ',') {
            p++;
        }
        if (*p == ',') {
            p++;
        }
    }

    char fields[3][64];
    int field_count = kbo_csv_read_trimmed_line_fields(raw, (char*)fields, sizeof(fields[0]), 3);
    uint32_t team_id = field_count > 1 ? kbo_csv_parse_u32_text(fields[1], 10) : 0u;
    uint32_t player_id = field_count > 2 ? kbo_csv_parse_u32_text(fields[2], 10) : 0u;
    if (team_id == 0u || player_id == 0u) {
        kbo_audit_foreign_waiver_command("skip", "malformed_command", action_name, line_no, team_id, player_id, 0u);
        kbo_log_runtimef("foreign waiver command line %d malformed: %s", line_no, raw);
        return 0;
    }

    if (team_id == 0 || player_id == 0) {
        kbo_log_runtimef("foreign waiver command line %d ignored (zero id): %s", line_no, raw);
        return 0;
    }

    uint8_t* destination_team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (destination_team == NULL) {
        kbo_audit_foreign_waiver_command("fail", "team_not_found", action_name, line_no, team_id, player_id, 0u);
        kbo_log_runtimef("foreign waiver command line %d failed: team=%u not found", line_no, team_id);
        return 0;
    }

    uint32_t player_current_team = 0;
    uint32_t player_current_league = 0;
    uint8_t* player = kbo_find_player_by_id(player_id, &player_current_team, &player_current_league);
    if (player == NULL) {
        kbo_audit_foreign_waiver_command("fail", "player_not_found", action_name, line_no, team_id, player_id, 0u);
        kbo_log_runtimef("foreign waiver command line %d failed: player=%u not found", line_no, player_id);
        return 0;
    }

    if (!kbo_original_club_priority_window_allows(player, team_id, action_name)) {
        kbo_audit_foreign_waiver_command("skip", "priority_window_blocked", action_name, line_no, team_id, player_id, 0u);
        return 0;
    }

    if (retain_rights) {
        uint32_t destination_league = *(uint32_t*)(destination_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        uint32_t fallback_league = player_current_league != 0 ? player_current_league : destination_league;
        if (!kbo_retain_foreign_player_rights(player, destination_team, fallback_league, player_id, team_id)) {
            kbo_audit_foreign_waiver_command("fail", "retain_failed", action_name, line_no, team_id, player_id, fallback_league);
            kbo_log_runtimef("foreign waiver command line %d failed: action=%s team=%u player=%u",
                       line_no, action_name, team_id, player_id);
            return 0;
        }

        kbo_log_runtimef("foreign waiver command line %d executed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        kbo_append_foreign_waiver_decision_record("user", "RETAIN", team_id, player_id, 0, 0, 1);
        kbo_audit_foreign_waiver_command("retain", "user_command", action_name, line_no, team_id, player_id, fallback_league);
        return 1;
    }

    if (skip_rights) {
        kbo_clear_foreign_waiver_right(team_id, player_id);
        kbo_log_runtimef("foreign waiver command line %d executed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        kbo_append_foreign_waiver_decision_record("user", "SKIP", team_id, player_id, 0, 0, 1);
        kbo_audit_foreign_waiver_command("skip_rights", "user_command", action_name, line_no, team_id, player_id, 0u);
        return 1;
    }

    if (player_current_team == team_id) {
        kbo_audit_foreign_waiver_command("noop", "player_already_on_team", action_name, line_no, team_id, player_id, 0u);
        kbo_log_runtimef("foreign waiver command line %d no-op: player=%u already on team=%u", line_no, player_id, team_id);
        return 1;
    }

    uint32_t destination_league = *(uint32_t*)(destination_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t fallback_league = player_current_league != 0 ? player_current_league : destination_league;
    if (fallback_league == 0) {
        kbo_audit_foreign_waiver_command("fail", "fallback_league_unavailable", action_name, line_no, team_id, player_id, 0u);
        return 0;
    }

    if (!kbo_retain_foreign_player_rights(player, destination_team, fallback_league, player_id, team_id)) {
        kbo_audit_foreign_waiver_command("fail", "claim_retain_failed", action_name, line_no, team_id, player_id, fallback_league);
        kbo_log_runtimef("foreign waiver command line %d failed: action=%s team=%u player=%u",
                   line_no, action_name, team_id, player_id);
        return 0;
    }
    kbo_log_runtimef("foreign waiver command line %d executed: action=%s team=%u player=%u",
               line_no, action_name, team_id, player_id);
    kbo_append_foreign_waiver_decision_record("user", "RETAIN", team_id, player_id, 0, 0, 1);
    kbo_audit_foreign_waiver_command("claim", "user_command", action_name, line_no, team_id, player_id, fallback_league);
    return 1;
}

int kbo_get_foreign_waiver_command_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file(KBO_FOREIGN_WAIVER_COMMAND_FILE, out, out_size);
}

int kbo_foreign_waiver_command_file_ready(void)
{
    char path[MAX_PATH] = {0};
    return kbo_get_foreign_waiver_command_path(path, sizeof(path));
}

static int kbo_append_foreign_waiver_cmd_line(const char* line)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_waiver_command_path(path, sizeof(path))) {
        return 0;
    }
    kbo_lock_enter(&g_kbo_foreign_waiver_decision_lock);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_lock_leave(&g_kbo_foreign_waiver_decision_lock);
        return 0;
    }
    DWORD wrote = 0;
    char out[128] = {0};
    int len = snprintf(out, sizeof(out), "%s\r\n", line);
    WriteFile(file, out, (DWORD)len, &wrote, NULL);
    CloseHandle(file);
    kbo_lock_leave(&g_kbo_foreign_waiver_decision_lock);
    return wrote == (DWORD)len;
}

int kbo_append_foreign_waiver_user_decision(uint32_t team_id, uint32_t player_id, int retain)
{
    if (team_id == 0u || player_id == 0u) {
        return 0;
    }
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "team_id", team_id);
            kbo_log_field_u32(&audit_fields, "player_id", player_id);
            kbo_log_field_i32(&audit_fields, "retain", retain ? 1 : 0);
            kbo_rule_audit_emit_fields(
                "foreign_waiver.user_decision",
                "block",
                "window_closed",
                "user",
                &audit_fields);
        } while (0);
        kbo_log_runtimef("foreign waiver decision: blocked by window state team=%u player=%u action=%s", team_id, player_id, retain ? "RETAIN" : "SKIP");
        return 0;
    }
    char line[128] = {0};
    int len = snprintf(line, sizeof(line), "%s,%u,%u", retain ? "RETAIN" : "SKIP", team_id, player_id);
    if (len <= 0) {
        return 0;
    }
    return kbo_append_foreign_waiver_cmd_line(line);
}
void process_foreign_waiver_commands(void)
{
    if (!kbo_foreign_waiver_ai_enabled() || !kbo_fix_enabled()) {
        return;
    }
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_waiver_command_path(path, sizeof(path))) {
        kbo_log_runtime_line("foreign waiver command: unable to resolve command path");
        return;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD file_size_low = GetFileSize(file, NULL);
    if (file_size_low == INVALID_FILE_SIZE || file_size_low == 0) {
        CloseHandle(file);
        return;
    }
    if (file_size_low >= 30000) {
        CloseHandle(file);
        kbo_log_runtime_line("foreign waiver command: command file too large; skip to avoid blocking");
        return;
    }

    char input[30000] = {0};
    DWORD read = 0;
    if (!ReadFile(file, input, file_size_low, &read, NULL) || read == 0) {
        CloseHandle(file);
        return;
    }
    CloseHandle(file);
    input[read] = '\0';

    char remaining[30000] = {0};
    char* keep = remaining;
    DWORD remain_len = 0;
    DWORD used_commands = 0;
    DWORD executed_commands = 0;

    const char* cursor = input;
    int line_no = 1;
    while (cursor < input + read) {
        const char* line_end = strchr(cursor, '\n');
        if (line_end == NULL) {
            line_end = input + read;
        }

        size_t len = (size_t)(line_end - cursor);
        while (len > 0 && (cursor[len - 1] == '\r' || cursor[len - 1] == '\n')) {
            len--;
        }

        char line[512] = {0};
        if (len >= sizeof(line)) {
            len = sizeof(line) - 1;
        }
        memcpy(line, cursor, len);
        line[len] = '\0';

        const char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n') {
            trimmed++;
        }

        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
            if (remain_len + len + 1 < sizeof(remaining)) {
                size_t add_len = strlen(line);
                memcpy(keep + remain_len, line, add_len);
                keep[remain_len + add_len] = '\r';
                keep[remain_len + add_len + 1] = '\n';
                remain_len += (DWORD)(add_len + 2);
            }
        } else {
            used_commands++;
            if (!kbo_execute_foreign_waiver_claim(trimmed, line_no)) {
                if (remain_len + len + 2 < sizeof(remaining)) {
                    size_t add_len = strlen(trimmed);
                    memcpy(keep + remain_len, trimmed, add_len);
                    keep[remain_len + add_len] = '\r';
                    keep[remain_len + add_len + 1] = '\n';
                    remain_len += (DWORD)(add_len + 2);
                }
            } else {
                executed_commands++;
            }
        }

        if (*line_end == '\0') {
            break;
        }
        cursor = line_end + 1;
        line_no++;
    }

    if (used_commands > 0) {
        file = CreateFileA(
            path,
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (file != INVALID_HANDLE_VALUE) {
            if (remain_len > 0) {
                DWORD written = 0;
                WriteFile(file, remaining, remain_len, &written, NULL);
            }
            CloseHandle(file);
        }
        kbo_log_runtimef("foreign waiver command: processed=%lu executed=%lu keep_len=%lu", used_commands, executed_commands, remain_len);
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u64(&audit_fields, "used", used_commands);
            kbo_log_field_u64(&audit_fields, "executed", executed_commands);
            kbo_log_field_u64(&audit_fields, "remaining_bytes", remain_len);
            kbo_rule_audit_emit_fields(
                "foreign_waiver.command_file",
                "process",
                "commands_consumed",
                "user",
                &audit_fields);
        } while (0);
        return;
    }
}
