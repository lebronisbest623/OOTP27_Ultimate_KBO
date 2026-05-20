#include "../internal/intl_established_fa_postscan_internal.h"
#include "sql/intl_established_fa_postscan_state_sql_store.h"

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
    if (!kbo_intl_established_fa_postscan_state_sql_path(path, sizeof(path))) {
        kbo_log_runtimef(
            "international established FA postscan state persist skipped source=%s reason=path_unavailable date=%u expected=%d",
            source != NULL ? source : "",
            state->scheduled_date,
            state->expected_count);
        return 0;
    }
    (void)path;

    if (!kbo_intl_established_fa_postscan_state_sql_persist(state, source)) {
        kbo_log_runtimef(
            "international established FA postscan state persist failed source=%s date=%u expected=%d",
            source != NULL ? source : "",
            state->scheduled_date,
            state->expected_count);
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

    return kbo_intl_established_fa_postscan_state_sql_load(out, source);
}
