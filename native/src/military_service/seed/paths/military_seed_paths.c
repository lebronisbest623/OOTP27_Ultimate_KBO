#include "military_seed_paths.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

/* Military service seed and players.dat path resolution. Included from native/KBOFix.c. */

int kbo_get_save_military_service_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("config\\military_service_seed.csv", out, out_size);
}

int kbo_get_global_military_service_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_global_data_file("military_service_seed.csv", out, out_size);
}

int kbo_get_save_military_service_resolved_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_save_state_db_path(out, out_size);
}

int kbo_get_current_players_dat_path_for_military_seed(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    snprintf(out, out_size, "%s\\players.dat", save_path);
    DWORD attrs = GetFileAttributesA(out);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return 1;
    }

    snprintf(out, out_size, "%s\\data\\players.dat", save_path);
    attrs = GetFileAttributesA(out);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
