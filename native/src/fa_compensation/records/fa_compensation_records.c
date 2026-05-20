#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../core/logging/core_log.h"
#include "fa_compensation_records.h"
#include "sql/fa_compensation_records_sql_store.h"

int kbo_load_fa_compensation_records(
    KboFaCompensationRecord* records,
    int max_records,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (records == NULL || max_records <= 0) {
        return 0;
    }
    memset(records, 0, (SIZE_T)max_records * sizeof(records[0]));

    if (out_path != NULL && out_path_size > 0) {
        (void)kbo_fa_compensation_records_sql_path(out_path, out_path_size);
    }

    int count = 0;
    if (!kbo_fa_compensation_records_sql_load(records, max_records, &count)) {
        return 0;
    }
    return count;
}

int kbo_persist_fa_compensation_records(const KboFaCompensationRecord* records, int record_count)
{
    char path[MAX_PATH] = {0};
    if (!kbo_fa_compensation_records_sql_path(path, sizeof(path))) {
        kbo_log_runtime_line("KBO FA compensation persist skipped reason=path_unavailable");
        return 0;
    }

    int ok = kbo_fa_compensation_records_sql_replace_all(records, record_count);
    if (!ok) {
        kbo_log_runtimef("KBO FA compensation sqlite persist failed path=%s", path);
        return 0;
    }
    kbo_log_runtimef("KBO FA compensation persisted records=%d path=%s", record_count, path);
    return 1;
}

int kbo_append_fa_compensation_record(const KboFaCompensationRecord* rec)
{
    if (rec == NULL || rec->player_id == 0u || rec->season == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_fa_compensation_records_sql_path(path, sizeof(path))) {
        kbo_log_runtime_line("KBO FA compensation append skipped reason=path_unavailable");
        return 0;
    }
    if (!kbo_fa_compensation_records_sql_append(rec)) {
        kbo_log_runtimef("KBO FA compensation append failed reason=sqlite player=%u path=%s", rec->player_id, path);
        return 0;
    }
    return 1;
}

int kbo_fa_compensation_find_existing(
    const KboFaCompensationRecord* records,
    int record_count,
    uint32_t player_id,
    uint32_t season,
    uint32_t original_team_id,
    uint32_t signing_team_id)
{
    if (records == NULL || player_id == 0u || season == 0u) {
        return -1;
    }
    for (int i = 0; i < record_count; i++) {
        const KboFaCompensationRecord* rec = &records[i];
        if (rec->player_id == player_id
                && rec->season == season
                && rec->original_team_id == original_team_id
                && rec->signing_team_id == signing_team_id) {
            return i;
        }
    }
    return -1;
}
