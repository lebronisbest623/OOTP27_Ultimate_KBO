#include "../../runtime/common/custom_events_common.h"
#include "asian_games_roster_store.h"
#include "internal/asian_games_roster_store_internal.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/csv/core_csv.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../core/files/atomic/core_atomic_file.h"

int kbo_get_asian_games_roster_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    return kbo_get_save_scoped_data_file("asian_games_roster.csv", out, out_size);
}

int kbo_get_asian_games_roster_history_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    return kbo_get_save_scoped_data_file("asian_games_roster_history.csv", out, out_size);
}

int kbo_get_asian_games_tournament_history_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    return kbo_get_save_scoped_data_file("asian_games_tournament_history.csv", out, out_size);
}

void kbo_clear_asian_games_roster_memory(const char* source)
{
    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    g_kbo_asian_games_roster_count = 0;
    g_kbo_asian_games_roster_year = 0;
    g_kbo_asian_games_result = KBO_ASIAN_GAMES_RESULT_UNKNOWN;
    g_kbo_asian_games_roster_save_path[0] = '\0';
    kbo_log_runtimef("KBO Asian Games roster memory cleared source=%s", source != NULL ? source : "");
}

void kbo_clear_asian_games_roster_if_save_changed(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        if (g_kbo_asian_games_roster_count > 0 || g_kbo_asian_games_roster_save_path[0] != '\0') {
            kbo_clear_asian_games_roster_memory(source);
        }
        return;
    }

    if (g_kbo_asian_games_roster_save_path[0] != '\0'
            && _stricmp(g_kbo_asian_games_roster_save_path, path) != 0) {
        kbo_clear_asian_games_roster_memory(source);
    }
}

int kbo_save_asian_games_roster_csv(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        kbo_log_runtimef("KBO Asian Games roster csv save skipped source=%s reason=path_unavailable", source != NULL ? source : "");
        return 0;
    }

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("KBO Asian Games roster csv save skipped source=%s reason=create_failed gle=%lu path=%s", source != NULL ? source : "", GetLastError(), path);
        return 0;
    }

    const char* header = "year,index,player_id,original_team_id,original_league_id,departure_date,return_date,age,role,wildcard,military_unserved,old_restricted,old_secondary_restricted,old_injury_active,old_injury_days_left,departed,returned,exempted,score,tournament_result\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    int ok = 1;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        char line[512] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%ld,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%d,%u\r\n",
            g_kbo_asian_games_roster_year,
            i + 1,
            entry->player_id,
            entry->original_team_id,
            entry->original_league_id,
            entry->departure_date,
            entry->return_date,
            (uint32_t)entry->age,
            (uint32_t)entry->role,
            (uint32_t)entry->wildcard,
            (uint32_t)entry->military_unserved,
            (uint32_t)entry->old_restricted,
            (uint32_t)entry->old_secondary_restricted,
            (uint32_t)entry->old_injury_active,
            (int)entry->old_injury_days_left,
            (uint32_t)entry->departed,
            (uint32_t)entry->returned,
            (uint32_t)entry->exempted,
            entry->score,
            (uint32_t)g_kbo_asian_games_result);
        if (len <= 0 || !WriteFile(file, line, (DWORD)len, &written, NULL) || written != (DWORD)len) {
            ok = 0;
            break;
        }
    }

    if (!ok) {
        kbo_atomic_abort(file, tmp_path);
        return 0;
    }
    if (!kbo_atomic_commit(file, tmp_path, path)) {
        kbo_log_runtimef("KBO Asian Games roster csv save: atomic commit failed path=%s", path);
        return 0;
    }
    if (ok) {
        snprintf(g_kbo_asian_games_roster_save_path, sizeof(g_kbo_asian_games_roster_save_path), "%s", path);
    }
    int history_saved = ok ? kbo_save_asian_games_roster_history_csv(source) : 0;
    kbo_log_runtimef(
        "KBO Asian Games roster csv save source=%s ok=%d history=%d year=%u count=%ld path=%s",
        source != NULL ? source : "",
        ok,
        history_saved,
        g_kbo_asian_games_roster_year,
        roster_count,
        path);
    return ok;
}

int kbo_load_asian_games_roster_csv(const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        kbo_clear_asian_games_roster_memory(source);
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        DWORD scoped_error = GetLastError();
        kbo_clear_asian_games_roster_memory(source);
        kbo_log_runtimef("KBO Asian Games roster csv load skipped source=%s reason=open_failed gle=%lu path=%s", source != NULL ? source : "", scoped_error, path);
        return 0;
    }

    KboAsianGamesRosterEntry loaded[KBO_ASIAN_GAMES_ROSTER_SIZE];
    memset(loaded, 0, sizeof(loaded));
    int loaded_count = 0;
    uint32_t loaded_year = 0;
    uint8_t loaded_result = KBO_ASIAN_GAMES_RESULT_UNKNOWN;

    while (loaded_count < KBO_ASIAN_GAMES_ROSTER_SIZE && kbo_csv_reader_next_row(reader)) {
        char fields[20][64];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 20);
        if (field_count < 18
                || fields[0][0] == '\0'
                || _stricmp(fields[0], "year") == 0) {
            continue;
        }

        uint32_t year = kbo_csv_parse_u32_text(fields[0], 10);
        uint32_t player_id = kbo_csv_parse_u32_text(fields[2], 10);
        if (player_id == 0u) {
            continue;
        }

        KboAsianGamesRosterEntry* entry = &loaded[loaded_count++];
        int has_military_unserved = field_count >= 20;
        int old_restricted_field = has_military_unserved ? 11 : 10;
        int tournament_result_field = has_military_unserved ? 19 : 18;
        uint32_t tournament_result = field_count > tournament_result_field
            ? kbo_csv_parse_u32_text(fields[tournament_result_field], 10)
            : 0u;
        loaded_year = year;
        if (field_count > tournament_result_field
                && (tournament_result == KBO_ASIAN_GAMES_RESULT_GOLD
                    || tournament_result == KBO_ASIAN_GAMES_RESULT_NO_GOLD)) {
            loaded_result = (uint8_t)tournament_result;
        }
        entry->player_id = player_id;
        entry->original_team_id = kbo_csv_parse_u32_text(fields[3], 10);
        entry->original_league_id = kbo_csv_parse_u32_text(fields[4], 10);
        entry->departure_date = kbo_csv_parse_u32_text(fields[5], 10);
        entry->return_date = kbo_csv_parse_u32_text(fields[6], 10);
        entry->age = (uint16_t)kbo_csv_parse_u32_text(fields[7], 10);
        entry->role = (uint8_t)kbo_csv_parse_u32_text(fields[8], 10);
        entry->wildcard = (uint8_t)kbo_csv_parse_u32_text(fields[9], 10);
        entry->military_unserved = has_military_unserved
            ? (uint8_t)kbo_csv_parse_u32_text(fields[10], 10)
            : 1u;
        entry->old_restricted = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field], 10);
        entry->old_secondary_restricted = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field + 1], 10);
        entry->old_injury_active = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field + 2], 10);
        entry->old_injury_days_left = (int16_t)strtol(fields[old_restricted_field + 3], NULL, 10);
        entry->departed = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field + 4], 10);
        entry->returned = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field + 5], 10);
        entry->exempted = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field + 6], 10);
        entry->score = (int32_t)strtol(fields[old_restricted_field + 7], NULL, 10);
        entry->player_ptr = 0;
    }

    kbo_csv_reader_close(reader);
    if (loaded_count <= 0) {
        return 0;
    }

    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    memcpy(g_kbo_asian_games_roster, loaded, sizeof(KboAsianGamesRosterEntry) * (size_t)loaded_count);
    g_kbo_asian_games_roster_count = loaded_count;
    g_kbo_asian_games_roster_year = loaded_year;
    g_kbo_asian_games_result = loaded_result;
    snprintf(g_kbo_asian_games_roster_save_path, sizeof(g_kbo_asian_games_roster_save_path), "%s", path);
    kbo_log_runtimef("KBO Asian Games roster csv load source=%s year=%u count=%d result=%u path=%s", source != NULL ? source : "", loaded_year, loaded_count, (uint32_t)g_kbo_asian_games_result, path);
    return loaded_count;
}

