#include "../../runtime/common/custom_events_common.h"
#include "import_and_load.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/csv/core_csv.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/files/save_paths/platform/core_path_io.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

int kbo_import_asian_games_schedule_seed_file_locked(const char* path, const char* source)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    int imported = 0;
    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    while (kbo_csv_reader_next_row(reader)) {
        char fields[12][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 12);
        KboAsianGamesScheduleSeed seed;
        if (kbo_parse_asian_games_schedule_seed_fields(fields, field_count, &seed)) {
            kbo_add_asian_games_schedule_seed_locked(&seed);
            imported++;
        }
    }

    kbo_csv_reader_close(reader);
    if (imported > 0) {
        kbo_log_runtimef(
            "KBO Asian Games schedule seed import source=%s imported=%d path=%s",
            source != NULL ? source : "",
            imported,
            path);
    }
    return imported;
}

static void kbo_asian_games_schedule_seed_loaded_key_component(
    const char* path,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (path == NULL || path[0] == '\0') {
        snprintf(out, out_size, "none");
        return;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (kbo_get_file_attributes_ex_utf8(path, &attrs)
            && (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0u) {
        snprintf(
            out,
            out_size,
            "%s:csv:%08lx%08lx:%08lx%08lx",
            path,
            (unsigned long)attrs.ftLastWriteTime.dwHighDateTime,
            (unsigned long)attrs.ftLastWriteTime.dwLowDateTime,
            (unsigned long)attrs.nFileSizeHigh,
            (unsigned long)attrs.nFileSizeLow);
        return;
    }

    snprintf(out, out_size, "%s:missing", path);
}

void kbo_ensure_asian_games_schedule_seeds_loaded(void)
{
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    kbo_get_global_asian_games_schedule_seed_path(global_seed_path, sizeof(global_seed_path));
    kbo_get_save_asian_games_schedule_seed_path(save_seed_path, sizeof(save_seed_path));

    char save_seed_key[MAX_PATH * 2] = {0};
    char global_seed_key[MAX_PATH * 2] = {0};
    kbo_asian_games_schedule_seed_loaded_key_component(save_seed_path, save_seed_key, sizeof(save_seed_key));
    kbo_asian_games_schedule_seed_loaded_key_component(global_seed_path, global_seed_key, sizeof(global_seed_key));

    char loaded_key[MAX_PATH * 6] = {0};
    snprintf(loaded_key, sizeof(loaded_key), "%s|%s", global_seed_key, save_seed_key);

    kbo_lock_asian_games_schedule_seeds();
    if (InterlockedCompareExchange(&g_kbo_asian_games_schedule_seed_loaded, 1, 0) == 0
            || _stricmp(g_kbo_asian_games_schedule_seed_loaded_key, loaded_key) != 0) {
        memset(g_kbo_asian_games_schedule_seeds, 0, sizeof(g_kbo_asian_games_schedule_seeds));
        g_kbo_asian_games_schedule_seed_count = 0;
        snprintf(g_kbo_asian_games_schedule_seed_loaded_key, sizeof(g_kbo_asian_games_schedule_seed_loaded_key), "%s", loaded_key);
        kbo_import_asian_games_schedule_seed_file_locked(global_seed_path, "global_seed");
        kbo_import_asian_games_schedule_seed_file_locked(save_seed_path, "save_seed");
    }
    kbo_unlock_asian_games_schedule_seeds();
}
