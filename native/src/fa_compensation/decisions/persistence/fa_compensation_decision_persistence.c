#include "../fa_compensation_decisions_internal.h"
#include "../../../core/csv/core_csv.h"
#include "../sql/fa_compensation_decisions_sql_store.h"

int kbo_load_fa_compensation_protection_debug_rows(
    KboFaCompensationProtectionDebugRow* rows,
    int max_rows)
{
    if (rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_protection_debug_path(path, sizeof(path))) {
        return 0;
    }
    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max_rows && kbo_csv_reader_next_row(reader)) {
        char fields[16][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 16);
        if (field_count < 16 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }

        KboFaCompensationProtectionDebugRow* row = &rows[count];
        row->fa_player_id = kbo_fa_compensation_parse_u32(fields[0]);
        row->season = kbo_fa_compensation_parse_u32(fields[1]);
        kbo_fa_compensation_copy_token(fields[2], row->grade, sizeof(row->grade));
        row->original_team_id = kbo_fa_compensation_parse_u32(fields[3]);
        row->signing_team_id = kbo_fa_compensation_parse_u32(fields[4]);
        row->due_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[5]);
        row->generated_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[6]);
        kbo_fa_compensation_copy_token(fields[7], row->list_type, sizeof(row->list_type));
        row->rank = kbo_fa_compensation_parse_u32(fields[8]);
        row->player_id = kbo_fa_compensation_parse_u32(fields[9]);
        kbo_fa_compensation_copy_token(fields[10], row->player_name, sizeof(row->player_name));
        row->score = kbo_fa_compensation_parse_i32(fields[11]);
        row->age = (uint16_t)kbo_fa_compensation_parse_u32(fields[12]);
        row->role = (uint8_t)kbo_fa_compensation_parse_u32(fields[13]);
        kbo_fa_compensation_copy_token(fields[14], row->reason, sizeof(row->reason));
        kbo_fa_compensation_copy_token(fields[15], row->source, sizeof(row->source));
        if (row->fa_player_id != 0u && row->player_id != 0u) {
            count++;
        } else {
            memset(row, 0, sizeof(*row));
        }
    }

    kbo_csv_reader_close(reader);
    return count;
}

int kbo_persist_fa_compensation_player_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const KboFaProtectedCandidate* selected,
    int unprotected_candidate_count,
    const char* source)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_fa_compensation_decisions_sql_path(path, sizeof(path))) {
        return 0;
    }
    if (!kbo_fa_compensation_decisions_sql_append_player_decision(
            rec,
            due_yyyymmdd,
            decided_yyyymmdd,
            selected,
            unprotected_candidate_count,
            source)) {
        kbo_log_runtimef(
            "KBO FA compensation decision persist failed reason=sqlite player=%u path=%s",
            rec->player_id,
            path);
        return 0;
    }

    kbo_log_runtimef(
        "KBO FA compensation player selected fa_player=%u original_team=%u signing_team=%u selected=%u name=%s score=%d unprotected=%d decision=%s",
        rec->player_id,
        rec->original_team_id,
        rec->signing_team_id,
        selected->player_id,
        selected->player_name,
        selected->score,
        unprotected_candidate_count,
        path);
    return 1;
}

int kbo_persist_fa_compensation_cash_only_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const char* source)
{
    if (rec == NULL || rec->player_id == 0u || rec->cash_only == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_fa_compensation_decisions_sql_path(path, sizeof(path))) {
        return 0;
    }
    if (!kbo_fa_compensation_decisions_sql_append_cash_only_decision(
            rec,
            due_yyyymmdd,
            decided_yyyymmdd,
            source)) {
        kbo_log_runtimef(
            "KBO FA compensation cash-only decision persist failed reason=sqlite player=%u path=%s",
            rec->player_id,
            path);
        return 0;
    }

    kbo_log_runtimef(
        "KBO FA compensation cash-only selected fa_player=%u original_team=%u signing_team=%u cash_only=%u decision=%s",
        rec->player_id,
        rec->original_team_id,
        rec->signing_team_id,
        rec->cash_only,
        path);
    return 1;
}

