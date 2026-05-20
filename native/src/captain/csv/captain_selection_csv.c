#include "../internal/captain_selection_internal.h"
#include "../../core/dates/constants/kbo_date_constants.h"
#include "../sql/captain_selection_sql_store.h"

int kbo_captain_selection_csv_path(uint32_t season, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0 || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    return kbo_captain_selection_sql_path(out, out_size);
}

int kbo_captain_selection_csv_exists(uint32_t season)
{
    return kbo_captain_selection_sql_exists(season);
}

int kbo_captain_load_selection_csv(
    uint32_t season,
    KboCaptainSelectionRow* rows,
    int max_rows)
{
    if (rows == NULL || max_rows <= 0 || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    int count = 0;
    if (!kbo_captain_selection_sql_load(season, rows, max_rows, &count)) {
        return 0;
    }
    return count;
}

int kbo_captain_write_selection_csv(
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (rows == NULL || row_count <= 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_captain_selection_csv_path(rows[0].season, path, sizeof(path))) {
        kbo_log_runtimef(
            "KBO captain selection csv skipped source=%s reason=path_unavailable season=%u",
            source != NULL ? source : "",
            rows[0].season);
        return 0;
    }

    if (!kbo_captain_selection_sql_replace_season(rows, row_count, source)) {
        kbo_log_runtimef("KBO captain selection csv write failed path=%s", path);
        return 0;
    }

    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return 1;
}
