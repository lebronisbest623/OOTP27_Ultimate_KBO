#include "../internal/intl_established_fa_postscan_internal.h"

#include "../../../core/dates/core_text_date.h"

#define KBO_INTL_ESTABLISHED_FA_POSTSCAN_STATE_FILE "intl_established_fa_postscan_state.txt"

static int kbo_intl_established_fa_postscan_state_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INTL_ESTABLISHED_FA_POSTSCAN_STATE_FILE,
        out,
        out_size);
}

int kbo_intl_established_fa_postscan_persist_state(
    const KboIntlEstablishedFaPostscanState* state,
    const char* source)
{
    if (state == NULL
            || state->scheduled_date == 0u
            || state->expected_count <= 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_intl_established_fa_postscan_state_path(path, sizeof(path))) {
        kbo_log_runtimef(
            "international established FA postscan state persist skipped source=%s reason=path_unavailable date=%u expected=%d",
            source != NULL ? source : "",
            state->scheduled_date,
            state->expected_count);
        return 0;
    }

    FILE* file = fopen(path, "w");
    if (file == NULL) {
        kbo_log_runtimef(
            "international established FA postscan state persist skipped source=%s reason=open_failed date=%u expected=%d path=%s",
            source != NULL ? source : "",
            state->scheduled_date,
            state->expected_count,
            path);
        return 0;
    }

    int written = fprintf(
        file,
        "1 %ld %d %u %d %d %d %u %u %u\n",
        (long)state->batch_id,
        state->before_count,
        state->before_max_player_id,
        state->original_count,
        state->expected_count,
        state->multiplier,
        state->primary_league_id,
        state->fallback_league_id,
        state->scheduled_date);
    fclose(file);
    if (written <= 0) {
        kbo_log_runtimef(
            "international established FA postscan state persist failed source=%s date=%u expected=%d path=%s",
            source != NULL ? source : "",
            state->scheduled_date,
            state->expected_count,
            path);
        return 0;
    }
    return 1;
}

int kbo_intl_established_fa_postscan_load_state(
    KboIntlEstablishedFaPostscanState* out,
    const char* source)
{
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    char path[MAX_PATH] = {0};
    if (!kbo_intl_established_fa_postscan_state_path(path, sizeof(path))) {
        return 0;
    }

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    long batch_id = 0;
    int version = 0;
    int before_count = 0;
    unsigned int before_max_player_id = 0u;
    int original_count = 0;
    int expected_count = 0;
    int multiplier = 0;
    unsigned int primary_league_id = 0u;
    unsigned int fallback_league_id = 0u;
    unsigned int scheduled_date = 0u;
    int matched = fscanf(
        file,
        "%d %ld %d %u %d %d %d %u %u %u",
        &version,
        &batch_id,
        &before_count,
        &before_max_player_id,
        &original_count,
        &expected_count,
        &multiplier,
        &primary_league_id,
        &fallback_league_id,
        &scheduled_date);
    fclose(file);

    if (matched != 10
            || version != 1
            || expected_count <= 0
            || scheduled_date == 0u
            || kbo_date_serial(scheduled_date / 10000u, (scheduled_date / 100u) % 100u, scheduled_date % 100u) == 0u) {
        kbo_log_runtimef(
            "international established FA postscan state load skipped source=%s reason=invalid path=%s matched=%d version=%d date=%u expected=%d",
            source != NULL ? source : "",
            path,
            matched,
            version,
            scheduled_date,
            expected_count);
        return 0;
    }

    out->pending = 0;
    out->batch_id = (LONG)batch_id;
    out->before_count = before_count;
    out->before_max_player_id = (uint32_t)before_max_player_id;
    out->original_count = original_count;
    out->expected_count = expected_count;
    out->multiplier = multiplier;
    out->primary_league_id = (uint32_t)primary_league_id;
    out->fallback_league_id = (uint32_t)fallback_league_id;
    out->scheduled_date = (uint32_t)scheduled_date;
    out->due_tick = 0ull;
    out->attempts = 0;
    return 1;
}
