#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#include "../fa_declaration_internal.h"
#include "../sql/fa_declaration_sql_store.h"

int kbo_fa_declaration_append_csv(
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    const char* source,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0u) {
        out_path[0] = '\0';
    }
    char path[MAX_PATH] = {0};
    if (!kbo_fa_declaration_sql_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0u) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return kbo_fa_declaration_sql_append_candidates(candidates, candidate_count, source);
}
