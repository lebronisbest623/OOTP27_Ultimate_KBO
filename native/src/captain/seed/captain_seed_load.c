#include "../internal/captain_selection_internal.h"
#include "../../core/csv/core_csv.h"

static int kbo_captain_seed_source_rank(const char* source)
{
    return source != NULL && _stricmp(source, "save_seed") == 0 ? 2 : 1;
}

int kbo_get_save_captain_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("config\\captain_seed.csv", out, out_size);
}

int kbo_get_global_captain_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_global_data_file("captain_seed.csv", out, out_size);
}

static void kbo_captain_seed_file_loaded_key_component(const char* path, char* out, size_t out_size)
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
    if (kbo_get_file_attributes_ex_utf8(path, &attrs)) {
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

static int kbo_add_captain_seed_locked(const KboCaptainSeed* seed)
{
    if (seed == NULL
            || (seed->team_id == 0u && seed->team_code[0] == '\0')
            || (seed->player_id == 0u && seed->player_key[0] == '\0')) {
        return 0;
    }

    if (g_kbo_captain_seed_count >= KBO_CAPTAIN_SEED_MAX) {
        return 0;
    }
    g_kbo_captain_seeds[g_kbo_captain_seed_count++] = *seed;
    return 1;
}

static int kbo_captain_seed_fields_parse_failure_is_reportable(char fields[][128], int field_count)
{
    if (fields == NULL || field_count <= 0) {
        return 0;
    }
    if (fields[0][0] == '\0' || fields[0][0] == '#' || fields[0][0] == ';') {
        return 0;
    }
    return _stricmp(fields[0], "season") != 0
        && _stricmp(fields[0], "team_id") != 0
        && _stricmp(fields[0], "team_code") != 0;
}

int kbo_import_captain_seed_file_locked(const char* path, const char* source)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int imported = 0;
    int parse_failed = 0;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[10][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 10);

        KboCaptainSeed seed;
        if (kbo_parse_captain_seed_fields(fields, field_count, &seed)) {
            seed.source_rank = (uint8_t)kbo_captain_seed_source_rank(source);
            snprintf(seed.source, sizeof(seed.source), "%s", source != NULL ? source : "");
            if (kbo_add_captain_seed_locked(&seed)) {
                imported++;
            }
        } else if (kbo_captain_seed_fields_parse_failure_is_reportable(fields, field_count)) {
            parse_failed++;
        }
    }

    kbo_csv_reader_close(reader);
    if (imported > 0 || parse_failed > 0) {
        kbo_log_runtimef(
            "KBO captain seed import source=%s imported=%d parse_failed=%d path=%s",
            source != NULL ? source : "",
            imported,
            parse_failed,
            path);
    }
    return imported;
}

void kbo_ensure_captain_seeds_loaded(void)
{
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    if (!kbo_get_save_captain_seed_path(save_seed_path, sizeof(save_seed_path))) {
        save_seed_path[0] = '\0';
    }
    if (!kbo_get_global_captain_seed_path(global_seed_path, sizeof(global_seed_path))) {
        global_seed_path[0] = '\0';
    }

    char save_seed_key[MAX_PATH * 2] = {0};
    char global_seed_key[MAX_PATH * 2] = {0};
    kbo_captain_seed_file_loaded_key_component(save_seed_path, save_seed_key, sizeof(save_seed_key));
    kbo_captain_seed_file_loaded_key_component(global_seed_path, global_seed_key, sizeof(global_seed_key));

    char loaded_key[MAX_PATH * 6] = {0};
    snprintf(loaded_key, sizeof(loaded_key), "%s|%s", save_seed_key, global_seed_key);
    if (InterlockedCompareExchange(&g_kbo_captain_seed_loaded, 0, 0) != 0
            && strcmp(g_kbo_captain_seed_loaded_key, loaded_key) == 0) {
        return;
    }

    kbo_lock_captain_seeds();
    if (InterlockedCompareExchange(&g_kbo_captain_seed_loaded, 0, 0) != 0
            && strcmp(g_kbo_captain_seed_loaded_key, loaded_key) == 0) {
        kbo_unlock_captain_seeds();
        return;
    }

    memset(g_kbo_captain_seeds, 0, sizeof(g_kbo_captain_seeds));
    g_kbo_captain_seed_count = 0;
    if (global_seed_path[0] != '\0') {
        kbo_import_captain_seed_file_locked(global_seed_path, "global_seed");
    }
    if (save_seed_path[0] != '\0') {
        kbo_import_captain_seed_file_locked(save_seed_path, "save_seed");
    }
    snprintf(g_kbo_captain_seed_loaded_key, sizeof(g_kbo_captain_seed_loaded_key), "%s", loaded_key);
    InterlockedExchange(&g_kbo_captain_seed_loaded, 1);
    int count = g_kbo_captain_seed_count;
    kbo_unlock_captain_seeds();
    kbo_log_runtimef("KBO captain seeds ready count=%d", count);
}
