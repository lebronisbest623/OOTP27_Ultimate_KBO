#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cbt_records.h"
#include "sql/cbt_records_sql_store.h"

int kbo_cbt_load_records(KboCbtRecord* records, int max, char* path_out, size_t path_size)
{
    if (records == NULL || max <= 0) {
        return 0;
    }
    memset(records, 0, (SIZE_T)max * sizeof(records[0]));
    if (path_out != NULL && path_size > 0) {
        path_out[0] = '\0';
    }

    if (path_out != NULL && path_size > 0) {
        (void)kbo_cbt_records_sql_path(path_out, path_size);
    }

    int count = 0;
    if (!kbo_cbt_records_sql_load(records, max, &count)) {
        return 0;
    }
    return count;
}

int kbo_cbt_save_records(const KboCbtRecord* records, int count)
{
    if (records == NULL || count <= 0) {
        return 0;
    }
    return kbo_cbt_records_sql_replace_all(records, count);
}

uint32_t kbo_cbt_get_consecutive_count(const KboCbtRecord* records, int count,
    uint32_t team_id, uint32_t before_season)
{
    if (records == NULL || count <= 0 || team_id == 0u || before_season == 0u) {
        return 0u;
    }

    /* Walk backwards from before_season counting consecutive prior violations */
    uint32_t streak = 0u;
    for (uint32_t s = before_season - 1u; s >= 1900u; s--) {
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (records[i].season == s && records[i].team_id == team_id) {
                if (records[i].overage > 0) {
                    streak++;
                    found = 1;
                }
                break;
            }
        }
        if (!found) {
            break;
        }
    }
    return streak;
}

int kbo_cbt_find_record(const KboCbtRecord* records, int count, uint32_t season, uint32_t team_id)
{
    if (records == NULL || count <= 0) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (records[i].season == season && records[i].team_id == team_id) {
            return i;
        }
    }
    return -1;
}
