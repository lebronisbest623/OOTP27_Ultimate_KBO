#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/logging/core_log.h"
#include "../../core/csv/core_csv.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../api/fa_market_classification.h"
#include "fa_market_seed_cases.h"

static int kbo_get_fa_market_cases_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_market_cases_seed.csv", out, out_size);
}

static void kbo_fa_market_copy_token(const char* value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }

    while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') {
        value++;
    }

    size_t used = 0;
    for (const char* p = value; *p != '\0' && used + 1u < out_size; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\r' || ch == '\n') {
            break;
        }
        out[used++] = (char)ch;
    }
    while (used > 0u && (out[used - 1u] == ' ' || out[used - 1u] == '\t')) {
        used--;
    }
    out[used] = '\0';
}

static int kbo_fa_market_seed_case_allowed(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    return strcmp(case_label, "KBO_FA_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_DEFERRED") == 0
        || strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
        || strcmp(case_label, "KBO_FA_CARRYOVER_UNSIGNED") == 0
        || strcmp(case_label, "DOMESTIC_RELEASED_NON_FA") == 0
        || strcmp(case_label, "DOMESTIC_UNDRAFTED_FREE_AGENT") == 0
        || strcmp(case_label, "DOMESTIC_INDEPENDENT_FUTURES_FA") == 0
        || strcmp(case_label, "DOMESTIC_INDEPENDENT_LEAGUE_FA") == 0
        || strcmp(case_label, "DOMESTIC_TEAMLESS_UNVERIFIED") == 0
        || strcmp(case_label, "DOMESTIC_MARKET_UNVERIFIED") == 0
        || strcmp(case_label, "FOREIGN_RESERVED_RIGHT") == 0
        || strcmp(case_label, "FOREIGN_FREE") == 0;
}

const char* kbo_fa_market_canonical_case_label(const char* case_label)
{
    if (case_label != NULL && strcmp(case_label, "DOMESTIC_MARKET_UNVERIFIED") == 0) {
        return "DOMESTIC_TEAMLESS_UNVERIFIED";
    }
    return case_label;
}

int kbo_fa_market_case_is_seeded_official(const char* case_label)
{
    return case_label != NULL
        && (strcmp(case_label, "KBO_FA_APPROVED") == 0
            || strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
            || strcmp(case_label, "KBO_FA_DEFERRED") == 0);
}

static void kbo_ensure_fa_market_cases_seed_template(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_market_cases_seed_path(path, sizeof(path))) {
        return;
    }

    DWORD attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    const char* header =
        "player_id,kbo_case,kbo_grade,note\r\n"
        "# Optional manual truth table for official KBO FA statuses, releases, and domestic teamless cases.\r\n"
        "# kbo_case: KBO_FA_APPROVED,KBO_FA_ELIGIBLE_NOT_APPROVED,KBO_FA_DEFERRED,KBO_FA_BY_HISTORY_UNGRADED,KBO_FA_CARRYOVER_UNSIGNED,DOMESTIC_RELEASED_NON_FA,DOMESTIC_UNDRAFTED_FREE_AGENT,DOMESTIC_INDEPENDENT_FUTURES_FA,DOMESTIC_INDEPENDENT_LEAGUE_FA,DOMESTIC_TEAMLESS_UNVERIFIED\r\n"
        "# kbo_grade: A,B,C,UNKNOWN\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    CloseHandle(file);
    kbo_log_runtimef("FA market seed template created path=%s", path);
}

int kbo_load_fa_market_seed_cases(KboFaMarketSeedCase* seeds, int max_seeds, char* out_path, size_t out_path_size)
{
    if (seeds == NULL || max_seeds <= 0) {
        return 0;
    }
    memset(seeds, 0, (SIZE_T)max_seeds * sizeof(seeds[0]));
    kbo_ensure_fa_market_cases_seed_template();

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_market_cases_seed_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max_seeds && kbo_csv_reader_next_row(reader)) {
        char fields[4][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 4);
        if (field_count < 3
                || fields[0][0] == '\0'
                || fields[0][0] == '#'
                || fields[0][0] == ';') {
            continue;
        }

        char* tail = NULL;
        unsigned long player_id = strtoul(fields[0], &tail, 10);
        if (tail != fields[0]
                && player_id > 0ul
                && player_id <= 0xfffffffful
                && kbo_fa_market_seed_case_allowed(fields[1])) {
            seeds[count].player_id = (uint32_t)player_id;
            kbo_fa_market_copy_token(fields[1], seeds[count].case_label, sizeof(seeds[count].case_label));
            kbo_fa_market_copy_token(fields[2][0] != '\0' ? fields[2] : "UNKNOWN", seeds[count].grade, sizeof(seeds[count].grade));
            kbo_fa_market_copy_token(field_count > 3 ? fields[3] : "", seeds[count].note, sizeof(seeds[count].note));
            count++;
        }
    }

    kbo_csv_reader_close(reader);
    return count;
}

const KboFaMarketSeedCase* kbo_find_fa_market_seed_case(
    const KboFaMarketSeedCase* seeds,
    int seed_count,
    uint32_t player_id)
{
    if (seeds == NULL || seed_count <= 0 || player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < seed_count; i++) {
        if (seeds[i].player_id == player_id) {
            return &seeds[i];
        }
    }
    return NULL;
}
