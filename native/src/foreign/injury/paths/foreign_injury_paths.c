#include "foreign_injury_paths.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

/* Foreign injury replacement file paths. Included from native/KBOFix.c. */

int kbo_get_foreign_injury_replacement_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_save_state_db_path(out, out_size);
}

int kbo_get_save_foreign_injury_replacement_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("config\\foreign_injury_replacements_seed.csv", out, out_size);
}

int kbo_get_global_foreign_injury_replacement_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_global_data_file("foreign_injury_replacements_seed.csv", out, out_size);
}
