#include <stdio.h>
#include <string.h>

#include "../fa_declaration.h"
#include "../fa_declaration_internal.h"
#include "../sql/fa_declaration_sql_store.h"

int kbo_load_fa_declaration_report_rows(
    KboFaDeclarationReportRow* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0u) {
        out_path[0] = '\0';
    }
    if (rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));

    char path[260] = {0};
    if (!kbo_fa_declaration_sql_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0u) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    int count = 0;
    (void)kbo_fa_declaration_sql_load_report_rows(rows, max_rows, &count);
    return count;
}

