#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "team_classification.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../lookup/team_lookup.h"
#include "parse/team_classification_seed_parse.h"

#define KBO_TEAM_CLASSIFICATION_SEED_FILE "team_classification_seed.csv"
#define KBO_TEAM_CLASSIFICATION_MAX 32
#define KBO_TEAM_CLASSIFICATION_REFRESH_MS 1000u

typedef struct KboTeamClassificationEntry {
    char team_csv_id[16];
    char display_name[96];
    volatile LONG independent_kind;
    volatile LONG team_id;
    volatile LONG league_id;
} KboTeamClassificationEntry;

static KboTeamClassificationEntry g_kbo_independent_teams[KBO_TEAM_CLASSIFICATION_MAX];
static volatile LONG g_kbo_independent_team_count = 0;
static volatile LONG g_kbo_team_classification_loaded_state = 0;
static volatile LONG g_kbo_team_classification_refresh_tick = 0;
static volatile LONG64 g_kbo_team_classification_seed_deferred_log_ms = 0;

static void kbo_team_classification_copy_text(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (text == NULL) {
        return;
    }

    size_t len = strlen(text);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, text, len);
    out[len] = '\0';
}

static const char* kbo_team_classification_independent_kind_label(int kind)
{
    if (kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES) {
        return "futures";
    }
    if (kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE) {
        return "league";
    }
    return "none";
}

static int kbo_team_classification_should_log_seed_deferred(void)
{
    ULONGLONG now = GetTickCount64();
    LONG64 last = InterlockedCompareExchange64(&g_kbo_team_classification_seed_deferred_log_ms, 0, 0);
    if (last > 0 && now >= (ULONGLONG)last && now - (ULONGLONG)last < 30000ull) {
        return 0;
    }
    return InterlockedCompareExchange64(&g_kbo_team_classification_seed_deferred_log_ms, (LONG64)now, last) == last;
}

static int kbo_team_classification_add_independent_row(
    const KboTeamClassificationSeedRow* row)
{
    if (row == NULL || row->team_csv_id[0] == '\0') {
        return 0;
    }
    int independent_kind = kbo_team_classification_seed_row_independent_kind(row);
    if (independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE) {
        return 0;
    }

    LONG count = InterlockedCompareExchange(&g_kbo_independent_team_count, 0, 0);
    for (LONG i = 0; i < count; i++) {
        if (_stricmp(g_kbo_independent_teams[i].team_csv_id, row->team_csv_id) == 0) {
            return 1;
        }
    }
    if (count >= KBO_TEAM_CLASSIFICATION_MAX) {
        kbo_log_runtimef(
            "KBO team classification seed ignored extra independent row team=%s kind=%s max=%d",
            row->team_csv_id,
            kbo_team_classification_independent_kind_label(independent_kind),
            KBO_TEAM_CLASSIFICATION_MAX);
        return 0;
    }

    KboTeamClassificationEntry* entry = &g_kbo_independent_teams[count];
    kbo_team_classification_copy_text(entry->team_csv_id, sizeof(entry->team_csv_id), row->team_csv_id);
    kbo_team_classification_copy_text(entry->display_name, sizeof(entry->display_name), row->display_name);
    InterlockedExchange(&entry->independent_kind, independent_kind);
    InterlockedExchange(&entry->team_id, 0);
    InterlockedExchange(&entry->league_id, 0);
    InterlockedExchange(&g_kbo_independent_team_count, count + 1);
    return 1;
}

static int kbo_team_classification_load_seed_file(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_global_data_file(KBO_TEAM_CLASSIFICATION_SEED_FILE, path, sizeof(path))) {
        return -1;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    int rows = 0;
    int loaded = 0;
    int futures = 0;
    int leagues = 0;
    char line[512];
    while (fgets(line, sizeof(line), file) != NULL) {
        KboTeamClassificationSeedRow row;
        if (!kbo_parse_team_classification_seed_line(line, &row)) {
            continue;
        }
        rows++;
        int independent_kind = kbo_team_classification_seed_row_independent_kind(&row);
        if (independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE) {
            continue;
        }
        if (kbo_team_classification_add_independent_row(&row)) {
            loaded++;
            if (independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES) {
                futures++;
            } else if (independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE) {
                leagues++;
            }
        }
    }
    fclose(file);

    kbo_log_runtimef(
        "KBO team classification seed loaded rows=%d independent=%d futures=%d true_league=%d source=%s",
        rows,
        loaded,
        futures,
        leagues,
        path);
    return loaded;
}

int kbo_team_classification_seed_source_available(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_global_data_file(KBO_TEAM_CLASSIFICATION_SEED_FILE, path, sizeof(path))) {
        return 0;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static void kbo_team_classification_load_once(void)
{
    for (;;) {
        LONG state = InterlockedCompareExchange(&g_kbo_team_classification_loaded_state, 0, 0);
        if (state == 2) {
            return;
        }
        if (state == 1) {
            SwitchToThread();
            continue;
        }
        if (InterlockedCompareExchange(&g_kbo_team_classification_loaded_state, 1, 0) == 0) {
            break;
        }
    }

    if (kbo_team_classification_load_seed_file() < 0) {
        if (kbo_team_classification_should_log_seed_deferred()) {
            kbo_log_runtimef(
                "KBO team classification seed deferred; source unavailable file=%s",
                KBO_TEAM_CLASSIFICATION_SEED_FILE);
        }
        InterlockedExchange(&g_kbo_team_classification_loaded_state, 0);
        return;
    }
    InterlockedExchange(&g_kbo_team_classification_loaded_state, 2);
}

static void kbo_team_classification_resolve_ids(int force)
{
    LONG count = InterlockedCompareExchange(&g_kbo_independent_team_count, 0, 0);
    if (count <= 0) {
        return;
    }

    DWORD now = GetTickCount();
    DWORD last = (DWORD)InterlockedCompareExchange(&g_kbo_team_classification_refresh_tick, 0, 0);
    if (!force && last != 0u && now - last < KBO_TEAM_CLASSIFICATION_REFRESH_MS) {
        return;
    }
    InterlockedExchange(&g_kbo_team_classification_refresh_tick, (LONG)now);

    for (LONG i = 0; i < count; i++) {
        KboTeamClassificationEntry* entry = &g_kbo_independent_teams[i];
        if (entry->team_csv_id[0] == '\0') {
            continue;
        }

        uint8_t* team = find_kbo_team_by_csv_id_any_league(entry->team_csv_id, 1);
        if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (team_id == 0u || league_id == 0u) {
            continue;
        }

        LONG old_team = InterlockedExchange(&entry->team_id, (LONG)team_id);
        LONG old_league = InterlockedExchange(&entry->league_id, (LONG)league_id);
        if (force || (uint32_t)old_team != team_id || (uint32_t)old_league != league_id) {
            kbo_log_runtimef(
                "KBO team classification resolved independent team csv=%s kind=%s team=%u league=%u name=%s",
                entry->team_csv_id,
                kbo_team_classification_independent_kind_label((int)InterlockedCompareExchange(&entry->independent_kind, 0, 0)),
                team_id,
                league_id,
                entry->display_name);
        }
    }
}

int kbo_collect_independent_futures_team_leagues(
    KboIndependentFuturesTeamLeague* out,
    int max_count,
    int* out_seed_rows,
    int* out_unresolved_rows)
{
    if (out_seed_rows != NULL) {
        *out_seed_rows = 0;
    }
    if (out_unresolved_rows != NULL) {
        *out_unresolved_rows = 0;
    }
    if (out != NULL && max_count > 0) {
        memset(out, 0, sizeof(out[0]) * (size_t)max_count);
    }

    kbo_team_classification_load_once();
    kbo_team_classification_resolve_ids(0);

    int written = 0;
    int unresolved = 0;
    int futures_rows = 0;
    LONG count = InterlockedCompareExchange(&g_kbo_independent_team_count, 0, 0);
    for (LONG i = 0; i < count; i++) {
        KboTeamClassificationEntry* entry = &g_kbo_independent_teams[i];
        if ((int)InterlockedCompareExchange(&entry->independent_kind, 0, 0)
                != KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES) {
            continue;
        }
        futures_rows++;
        uint32_t team_id = (uint32_t)InterlockedCompareExchange(&entry->team_id, 0, 0);
        uint32_t league_id = (uint32_t)InterlockedCompareExchange(&entry->league_id, 0, 0);
        if (team_id == 0u || league_id == 0u) {
            unresolved++;
            continue;
        }

        int duplicate_league = 0;
        for (int j = 0; j < written; j++) {
            if (out != NULL && out[j].league_id == league_id) {
                duplicate_league = 1;
                break;
            }
        }
        if (duplicate_league) {
            continue;
        }

        if (out != NULL && written < max_count) {
            KboIndependentFuturesTeamLeague* item = &out[written];
            item->team_id = team_id;
            item->league_id = league_id;
            kbo_team_classification_copy_text(item->team_csv_id, sizeof(item->team_csv_id), entry->team_csv_id);
            kbo_team_classification_copy_text(item->display_name, sizeof(item->display_name), entry->display_name);
            written++;
        }
    }

    if (out_seed_rows != NULL) {
        *out_seed_rows = futures_rows;
    }
    if (out_unresolved_rows != NULL) {
        *out_unresolved_rows = unresolved;
    }
    return written;
}

int kbo_team_classification_independent_kind_for_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE;
    }

    kbo_team_classification_load_once();
    kbo_team_classification_resolve_ids(0);

    LONG count = InterlockedCompareExchange(&g_kbo_independent_team_count, 0, 0);
    for (LONG i = 0; i < count; i++) {
        KboTeamClassificationEntry* entry = &g_kbo_independent_teams[i];
        uint32_t entry_team_id = (uint32_t)InterlockedCompareExchange(&entry->team_id, 0, 0);
        if (entry_team_id == team_id) {
            return (int)InterlockedCompareExchange(&entry->independent_kind, 0, 0);
        }
    }
    return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE;
}

int kbo_team_classification_league_has_independent_futures_team(uint32_t league_id)
{
    if (league_id == 0u) {
        return 0;
    }

    kbo_team_classification_load_once();
    kbo_team_classification_resolve_ids(0);

    LONG count = InterlockedCompareExchange(&g_kbo_independent_team_count, 0, 0);
    for (LONG i = 0; i < count; i++) {
        if ((int)InterlockedCompareExchange(&g_kbo_independent_teams[i].independent_kind, 0, 0)
                != KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES) {
            continue;
        }
        uint32_t entry_league_id = (uint32_t)InterlockedCompareExchange(
            &g_kbo_independent_teams[i].league_id,
            0,
            0);
        if (entry_league_id == league_id) {
            return 1;
        }
    }
    return 0;
}
