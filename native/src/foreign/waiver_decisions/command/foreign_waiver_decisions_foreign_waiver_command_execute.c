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
