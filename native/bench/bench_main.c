#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/core/csv/core_csv.h"
#include "../src/core/dates/core_text_date.h"
#include "../src/fa_compensation/protection/fa_compensation_protection_score.h"
#include "../src/fa_compensation/protection/candidate_score/fa_compensation_candidate_score.h"
#include "../src/foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../src/foreign/common/policy/foreign_waiver_policy.h"
#include "../src/foreign/quota/candidates/foreign_quota_retention_opportunity_probe.h"
#include "../src/foreign/quota/counts/foreign_quota_counts.h"
#include "../src/perf_snapshot/perf_snapshot_format.h"

enum {
    BENCH_TEAM_ID_MAX = 4096,
    BENCH_READABLE_RANGE_MAX = 16,
    BENCH_PLAN_MAX = 128,
    BENCH_PLAN_SELECTOR_MAX = 128
};

typedef enum BenchReadabilityMode {
    BENCH_READABILITY_FAST = 0,
    BENCH_READABILITY_VIRTUAL_QUERY = 1
} BenchReadabilityMode;

static uint8_t* g_players = NULL;
static uintptr_t* g_player_vector = NULL;
static int32_t g_player_count = 0;
static uint8_t* g_teams = NULL;
static uintptr_t* g_team_vector = NULL;
static int32_t g_team_count = 0;
static uint32_t g_bench_team_ids[BENCH_TEAM_ID_MAX];
static int g_bench_team_count = 0;
static uint32_t g_bench_primary_team_id = 101u;
static uint32_t g_bench_primary_team_player_id = 1u;
static int32_t g_bench_allowed_candidate_index = -1;
static int32_t g_bench_blocked_candidate_index = -1;
static LARGE_INTEGER g_qpc_frequency;
static volatile int64_t g_bench_sink = 0;
static BenchReadabilityMode g_bench_readability_mode = BENCH_READABILITY_FAST;
volatile LONG g_kbo_custom_foreign_pending_offer_generation = 0;
volatile LONG g_kbo_foreign_waiver_rights_generation = 1;
int g_kbo_foreign_injury_replacement_count = 0;

LONG kbo_custom_foreign_pending_offer_generation_for_team(uint32_t team_id)
{
    (void)team_id;
    return InterlockedCompareExchange(&g_kbo_custom_foreign_pending_offer_generation, 0, 0);
}

int kbo_foreign_ai_controller_enabled(void)
{
    return 0;
}

int kbo_nation_is_asian_quota_candidate(uint32_t nation_id)
{
    return nation_id != 0u
        && nation_id != OOTP27_KBO_KOREA_NATION_ID
        && nation_id != 840u;
}

typedef struct BenchReadableRange {
    uintptr_t start;
    uintptr_t end;
} BenchReadableRange;

static BenchReadableRange g_bench_readable_ranges[BENCH_READABLE_RANGE_MAX];
static int g_bench_readable_range_count = 0;

static void bench_free_players(void);

typedef struct BenchConfig {
    int samples;
    int players;
} BenchConfig;

typedef struct BenchPlanEntry {
    char selector[BENCH_PLAN_SELECTOR_MAX];
    int is_prefix;
    long long iterations;
    int matched;
} BenchPlanEntry;

typedef struct BenchPlan {
    BenchPlanEntry entries[BENCH_PLAN_MAX];
    int count;
} BenchPlan;

typedef struct BenchCursor {
    uint32_t index;
} BenchCursor;

typedef struct BenchTeamContext {
    uint32_t team_id;
} BenchTeamContext;

typedef struct BenchFaProtectionContext {
    KboFaCompensationRecord record;
    KboFaProtectedCandidate candidates[KBO_FA_COMPENSATION_PROTECTED_LIST_MAX + 1];
    KboFaProtectedCandidate auto_protected[KBO_FA_COMPENSATION_PROTECTED_LIST_MAX];
    uint32_t cold_league_cursor;
} BenchFaProtectionContext;

typedef struct BenchForeignCandidateContext {
    uint32_t index;
    uint32_t team_id;
    int32_t candidate_index;
    int force_cold;
} BenchForeignCandidateContext;

typedef void (*BenchStep)(void* context);

int kbo_fast_block_fa_candidate_before_original(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    const char* context,
    uint32_t* out_player_id);
uint32_t kbo_custom_foreign_policy_extra_slots_for_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id);

static int bench_double_compare(const void* left, const void* right)
{
    double a = *(const double*)left;
    double b = *(const double*)right;
    if (a < b) { return -1; }
    if (a > b) { return 1; }
    return 0;
}

static double bench_ticks_to_us(LONGLONG ticks)
{
    return ((double)ticks * 1000000.0) / (double)g_qpc_frequency.QuadPart;
}

static void bench_register_readable_range(const void* data, size_t size)
{
    if (data == NULL || size == 0u || g_bench_readable_range_count >= BENCH_READABLE_RANGE_MAX) {
        return;
    }
    uintptr_t start = (uintptr_t)data;
    uintptr_t end = start + (uintptr_t)size;
    if (end <= start) {
        return;
    }
    BenchReadableRange* range = &g_bench_readable_ranges[g_bench_readable_range_count++];
    range->start = start;
    range->end = end;
}

static int bench_range_readable(const void* data, SIZE_T size)
{
    if (data == NULL || size == 0u) {
        return 0;
    }
    uintptr_t start = (uintptr_t)data;
    uintptr_t end = start + (uintptr_t)size;
    if (start < 0x10000u || end <= start) {
        return 0;
    }
    for (int i = 0; i < g_bench_readable_range_count; i++) {
        const BenchReadableRange* range = &g_bench_readable_ranges[i];
        if (start >= range->start && end <= range->end) {
            return 1;
        }
    }
    return 0;
}

static int bench_parse_int_arg(int argc, char** argv, const char* name, int fallback, int min_value, int max_value)
{
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], name) != 0) {
            continue;
        }
        char* tail = NULL;
        long value = strtol(argv[i + 1], &tail, 10);
        if (tail == argv[i + 1] || value < min_value || value > max_value) {
            return fallback;
        }
        return (int)value;
    }
    return fallback;
}

static const char* bench_parse_string_arg(int argc, char** argv, const char* name)
{
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static BenchReadabilityMode bench_parse_readability_mode(int argc, char** argv)
{
    const char* value = bench_parse_string_arg(argc, argv, "--readability");
    if (value == NULL || value[0] == '\0' || strcmp(value, "fast") == 0) {
        return BENCH_READABILITY_FAST;
    }
    if (strcmp(value, "virtual") == 0 || strcmp(value, "virtualquery") == 0) {
        return BENCH_READABILITY_VIRTUAL_QUERY;
    }
    fprintf(stderr, "Unknown --readability value '%s'; using fast\n", value);
    return BENCH_READABILITY_FAST;
}

static void bench_strip_line(char* text)
{
    if (text == NULL) {
        return;
    }
    size_t length = strlen(text);
    while (length > 0u && (text[length - 1u] == '\n' || text[length - 1u] == '\r')) {
        text[--length] = '\0';
    }
}

static long long bench_parse_i64(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    char* tail = NULL;
    long long value = strtoll(text, &tail, 10);
    if (tail == text || value < 0) {
        return 0;
    }
    return value;
}

static int bench_load_plan(const char* path, BenchPlan* plan)
{
    if (path == NULL || path[0] == '\0' || plan == NULL) {
        return 0;
    }

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "failed to open bench replay plan: %s\n", path);
        return 0;
    }

    memset(plan, 0, sizeof(*plan));
    char line[2048];
    int line_number = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;
        bench_strip_line(line);
        if (line[0] == '\0') {
            continue;
        }
        if (line_number == 1 && strstr(line, "case_selector") == line) {
            continue;
        }

        char* selector = line;
        char* match = strchr(selector, ',');
        if (match == NULL) {
            continue;
        }
        *match++ = '\0';
        char* iterations_text = strchr(match, ',');
        if (iterations_text == NULL) {
            continue;
        }
        *iterations_text++ = '\0';
        char* extra = strchr(iterations_text, ',');
        if (extra != NULL) {
            *extra = '\0';
        }

        long long iterations = bench_parse_i64(iterations_text);
        if (selector[0] == '\0' || iterations <= 0 || plan->count >= BENCH_PLAN_MAX) {
            continue;
        }

        BenchPlanEntry* entry = &plan->entries[plan->count++];
        snprintf(entry->selector, sizeof(entry->selector), "%s", selector);
        entry->is_prefix = strcmp(match, "prefix") == 0;
        entry->iterations = iterations;
        entry->matched = 0;
    }

    fclose(file);
    fprintf(stderr, "Loaded replay plan entries=%d path=%s\n", plan->count, path);
    return plan->count > 0;
}

static BenchPlanEntry* bench_find_plan_entry(BenchPlan* plan, const char* case_name)
{
    if (plan == NULL || case_name == NULL) {
        return NULL;
    }

    for (int i = 0; i < plan->count; i++) {
        BenchPlanEntry* entry = &plan->entries[i];
        if (entry->is_prefix) {
            size_t prefix_length = strlen(entry->selector);
            if (prefix_length > 0u && strncmp(case_name, entry->selector, prefix_length) == 0) {
                return entry;
            }
            continue;
        }
        if (strcmp(case_name, entry->selector) == 0) {
            return entry;
        }
    }
    return NULL;
}

static void bench_run_case(
    FILE* out,
    const char* name,
    int samples,
    int inner_iterations,
    int warmup_iterations,
    BenchStep step,
    void* context)
{
    if (out == NULL || name == NULL || samples <= 0 || inner_iterations <= 0 || step == NULL) {
        return;
    }

    long long total_iterations = (long long)samples * (long long)inner_iterations;
    if (total_iterations <= 0) {
        return;
    }

    for (int i = 0; i < warmup_iterations; i++) {
        step(context);
    }

    double* sample_us = (double*)calloc((size_t)samples, sizeof(double));
    if (sample_us == NULL) {
        return;
    }

    double total_us = 0.0;
    double max_per_iteration_us = 0.0;
    for (int sample = 0; sample < samples; sample++) {
        LARGE_INTEGER start;
        LARGE_INTEGER end;
        QueryPerformanceCounter(&start);
        for (int i = 0; i < inner_iterations; i++) {
            step(context);
        }
        QueryPerformanceCounter(&end);

        double elapsed_us = bench_ticks_to_us(end.QuadPart - start.QuadPart);
        double per_iteration_us = elapsed_us / (double)inner_iterations;
        sample_us[sample] = per_iteration_us;
        total_us += elapsed_us;
        if (per_iteration_us > max_per_iteration_us) {
            max_per_iteration_us = per_iteration_us;
        }
    }

    qsort(sample_us, (size_t)samples, sizeof(sample_us[0]), bench_double_compare);
    int p50_index = samples / 2;
    int p95_index = (samples * 95) / 100;
    if (p95_index >= samples) {
        p95_index = samples - 1;
    }

    fprintf(
        out,
        "%s,%lld,%d,%.3f,%.6f,%.6f,%.6f,%.6f\n",
        name,
        total_iterations,
        inner_iterations,
        total_us / 1000.0,
        total_us / (double)total_iterations,
        sample_us[p50_index],
        sample_us[p95_index],
        max_per_iteration_us);
    fflush(out);

    free(sample_us);
}

static void bench_run_case_iterations(
    FILE* out,
    const char* name,
    long long total_iterations,
    int batch_iterations,
    int warmup_iterations,
    BenchStep step,
    void* context)
{
    if (out == NULL || name == NULL || total_iterations <= 0 || batch_iterations <= 0 || step == NULL) {
        return;
    }
    if ((long long)batch_iterations > total_iterations) {
        batch_iterations = (int)total_iterations;
    }

    int samples = (int)((total_iterations + (long long)batch_iterations - 1ll) / (long long)batch_iterations);
    if (samples <= 0) {
        return;
    }

    for (int i = 0; i < warmup_iterations; i++) {
        step(context);
    }

    double* sample_us = (double*)calloc((size_t)samples, sizeof(double));
    if (sample_us == NULL) {
        return;
    }

    double total_us = 0.0;
    double max_per_iteration_us = 0.0;
    long long remaining = total_iterations;
    for (int sample = 0; sample < samples; sample++) {
        int iterations_this_batch = remaining < (long long)batch_iterations
            ? (int)remaining
            : batch_iterations;
        if (iterations_this_batch <= 0) {
            break;
        }

        LARGE_INTEGER start;
        LARGE_INTEGER end;
        QueryPerformanceCounter(&start);
        for (int i = 0; i < iterations_this_batch; i++) {
            step(context);
        }
        QueryPerformanceCounter(&end);

        double elapsed_us = bench_ticks_to_us(end.QuadPart - start.QuadPart);
        double per_iteration_us = elapsed_us / (double)iterations_this_batch;
        sample_us[sample] = per_iteration_us;
        total_us += elapsed_us;
        if (per_iteration_us > max_per_iteration_us) {
            max_per_iteration_us = per_iteration_us;
        }
        remaining -= (long long)iterations_this_batch;
    }

    qsort(sample_us, (size_t)samples, sizeof(sample_us[0]), bench_double_compare);
    int p50_index = samples / 2;
    int p95_index = (samples * 95) / 100;
    if (p95_index >= samples) {
        p95_index = samples - 1;
    }

    fprintf(
        out,
        "%s,%lld,%d,%.3f,%.6f,%.6f,%.6f,%.6f\n",
        name,
        total_iterations,
        batch_iterations,
        total_us / 1000.0,
        total_us / (double)total_iterations,
        sample_us[p50_index],
        sample_us[p95_index],
        max_per_iteration_us);
    fflush(out);

    free(sample_us);
}

static void bench_run_case_from_plan_or_default(
    FILE* out,
    const char* name,
    int default_samples,
    int default_inner_iterations,
    BenchStep step,
    void* context,
    BenchPlan* plan)
{
    if (plan != NULL && plan->count > 0) {
        BenchPlanEntry* entry = bench_find_plan_entry(plan, name);
        if (entry == NULL) {
            return;
        }
        entry->matched++;
        bench_run_case_iterations(out, name, entry->iterations, default_inner_iterations, 0, step, context);
        return;
    }

    bench_run_case(out, name, default_samples, default_inner_iterations, 8, step, context);
}

static void bench_fill_player(uint8_t* player, int index)
{
    memset(player, 0, OOTP27_PLAYER_SCAN_BYTES);
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = (uint32_t)(index + 1);
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = 1u;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 101u + (uint32_t)(index % 10);
    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 101u + (uint32_t)(index % 10);
    *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) =
        (index % 5 == 0) ? 840u : OOTP27_KBO_KOREA_NATION_ID;
    *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET) = (uint16_t)(18 + (index % 23));
    player[OOTP27_PLAYER_POSITION_ROLE_OFFSET] = (uint8_t)(1 + (index % 9));
    player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] =
        player[OOTP27_PLAYER_POSITION_ROLE_OFFSET] == 1u ? 1u : 2u;
    *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET) =
        (int32_t)(30000000 + ((index % 30) * 25000000));
    *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET) = (int16_t)(35 + (index % 40));
    *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET) = (int16_t)(38 + ((index * 3) % 42));
    *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET) = (int16_t)(34 + ((index * 5) % 44));
    *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET) = (int16_t)(20 + ((index * 7) % 58));
}

static void bench_add_team_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return;
    }
    for (int i = 0; i < g_bench_team_count; i++) {
        if (g_bench_team_ids[i] == team_id) {
            return;
        }
    }
    if (g_bench_team_count < BENCH_TEAM_ID_MAX) {
        g_bench_team_ids[g_bench_team_count++] = team_id;
    }
}

static int bench_player_is_foreign_index(int32_t index);
static int bench_player_matches_team_index(int32_t index, uint32_t team_id);

static void bench_collect_team_ids(void)
{
    memset(g_bench_team_ids, 0, sizeof(g_bench_team_ids));
    g_bench_team_count = 0;
    g_bench_primary_team_id = 101u;
    g_bench_primary_team_player_id = 1u;

    for (int32_t i = 0; i < g_player_count; i++) {
        uint8_t* player = g_players + ((size_t)i * OOTP27_PLAYER_SCAN_BYTES);
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        bench_add_team_id(current_team_id);
        bench_add_team_id(active_team_id);
        bench_add_team_id(loan_team_id);
        if (g_bench_primary_team_player_id == 1u && player_id != 0u && current_team_id != 0u) {
            g_bench_primary_team_player_id = player_id;
        }
    }

    if (g_bench_team_count > 0) {
        g_bench_primary_team_id = g_bench_team_ids[0];
    }

    for (int32_t i = 0; i < g_player_count; i++) {
        if (!bench_player_is_foreign_index(i)) {
            continue;
        }
        uint32_t current_team_id = *(uint32_t*)(g_players + ((size_t)i * OOTP27_PLAYER_SCAN_BYTES) + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (current_team_id != 0u) {
            g_bench_primary_team_id = current_team_id;
            break;
        }
    }

    for (int32_t i = 0; i < g_player_count; i++) {
        uint8_t* player = g_players + ((size_t)i * OOTP27_PLAYER_SCAN_BYTES);
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        if (current_team_id == g_bench_primary_team_id || active_team_id == g_bench_primary_team_id) {
            g_bench_primary_team_player_id = player_id;
            break;
        }
    }
}

static int bench_player_is_foreign_index(int32_t index)
{
    if (index < 0 || index >= g_player_count || g_players == NULL) {
        return 0;
    }
    uint8_t* player = g_players + ((size_t)index * OOTP27_PLAYER_SCAN_BYTES);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    return player_id != 0u && nation_id != 0u && nation_id != OOTP27_KBO_KOREA_NATION_ID;
}

static int bench_player_matches_team_index(int32_t index, uint32_t team_id)
{
    if (index < 0 || index >= g_player_count || team_id == 0u || g_players == NULL) {
        return 0;
    }
    uint8_t* player = g_players + ((size_t)index * OOTP27_PLAYER_SCAN_BYTES);
    return *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == team_id
        || *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == team_id
        || *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == team_id;
}

static void bench_select_foreign_candidates(void)
{
    g_bench_allowed_candidate_index = -1;
    g_bench_blocked_candidate_index = -1;
    for (int32_t i = 0; i < g_player_count; i++) {
        if (!bench_player_is_foreign_index(i)) {
            continue;
        }
        if (g_bench_allowed_candidate_index < 0
                && bench_player_matches_team_index(i, g_bench_primary_team_id)) {
            g_bench_allowed_candidate_index = i;
        }
        if (g_bench_blocked_candidate_index < 0
                && !bench_player_matches_team_index(i, g_bench_primary_team_id)) {
            g_bench_blocked_candidate_index = i;
        }
    }
    for (int32_t i = 0; i < g_player_count; i++) {
        if (!bench_player_is_foreign_index(i)) {
            continue;
        }
        if (g_bench_allowed_candidate_index < 0) {
            g_bench_allowed_candidate_index = i;
        }
        if (g_bench_blocked_candidate_index < 0) {
            g_bench_blocked_candidate_index = i;
        }
    }
}

static int bench_init_teams_from_collected_ids(void)
{
    if (g_bench_team_count <= 0) {
        return 1;
    }

    g_teams = (uint8_t*)calloc((size_t)g_bench_team_count, OOTP27_KBO_TEAM_READABLE_BYTES);
    g_team_vector = (uintptr_t*)calloc((size_t)g_bench_team_count, sizeof(uintptr_t));
    if (g_teams == NULL || g_team_vector == NULL) {
        free(g_team_vector);
        free(g_teams);
        g_team_vector = NULL;
        g_teams = NULL;
        g_team_count = 0;
        return 0;
    }

    for (int i = 0; i < g_bench_team_count; i++) {
        uint8_t* team = g_teams + ((size_t)i * OOTP27_KBO_TEAM_READABLE_BYTES);
        memset(team, 0, OOTP27_KBO_TEAM_READABLE_BYTES);
        *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) = g_bench_team_ids[i];
        *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) = 1u;
        team[OOTP27_KBO_TEAM_ACTIVE_OFFSET] = 1u;
        team[OOTP27_KBO_TEAM_DELETED_OFFSET] = 0u;
        g_team_vector[i] = (uintptr_t)team;
    }
    g_team_count = g_bench_team_count;
    bench_register_readable_range(g_teams, (size_t)g_team_count * OOTP27_KBO_TEAM_READABLE_BYTES);
    bench_register_readable_range(g_team_vector, (size_t)g_team_count * sizeof(uintptr_t));
    return 1;
}

static void bench_prepare_fa_protection_context(BenchFaProtectionContext* context)
{
    if (context == NULL) {
        return;
    }
    memset(context, 0, sizeof(*context));
    context->record.player_id = g_bench_primary_team_player_id;
    context->record.signed_on_yyyymmdd = 20270115u;
    context->record.season = 2027u;
    context->record.league_id = 1u;
    context->record.original_team_id = g_bench_primary_team_id;
    context->record.signing_team_id = g_bench_primary_team_id;
    context->record.previous_salary = 100000000;
    context->record.cash_with_player = 100000000u;
    context->record.cash_only = 200000000u;
    context->record.protect_count = 20u;
    context->record.requires_player_compensation = 1u;
    snprintf(context->record.grade, sizeof(context->record.grade), "%s", "B");
    snprintf(context->record.case_label, sizeof(context->record.case_label), "%s", "bench");
    snprintf(context->record.player_name, sizeof(context->record.player_name), "Player #%u", context->record.player_id);
    snprintf(context->record.source, sizeof(context->record.source), "%s", "bench");
    context->cold_league_cursor = 1000u;
}

static void bench_seed_replay_foreign_org_counts(uint32_t team_id)
{
    kbo_foreign_org_count_seed_benchmark_snapshot(team_id, 0u, 0u, 0u);
}

static int bench_init_players(int player_count)
{
    if (player_count <= 0) {
        return 1;
    }

    g_players = (uint8_t*)calloc((size_t)player_count, OOTP27_PLAYER_SCAN_BYTES);
    g_player_vector = (uintptr_t*)calloc((size_t)player_count, sizeof(uintptr_t));
    if (g_players == NULL || g_player_vector == NULL) {
        free(g_player_vector);
        free(g_players);
        g_players = NULL;
        g_player_vector = NULL;
        return 0;
    }

    for (int i = 0; i < player_count; i++) {
        uint8_t* player = g_players + ((size_t)i * OOTP27_PLAYER_SCAN_BYTES);
        bench_fill_player(player, i);
        g_player_vector[i] = (uintptr_t)player;
    }
    g_player_count = player_count;
    bench_register_readable_range(g_players, (size_t)player_count * OOTP27_PLAYER_SCAN_BYTES);
    bench_register_readable_range(g_player_vector, (size_t)player_count * sizeof(uintptr_t));
    return 1;
}

static int bench_load_snapshot(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "failed to open snapshot: %s\n", path);
        return 0;
    }

    KboPerfSnapshotHeader header;
    memset(&header, 0, sizeof(header));
    if (fread(&header, 1u, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        fprintf(stderr, "snapshot header read failed: %s\n", path);
        return 0;
    }
    if (memcmp(header.magic, KBO_PERF_SNAPSHOT_MAGIC, KBO_PERF_SNAPSHOT_MAGIC_BYTES) != 0
            || header.schema != KBO_PERF_SNAPSHOT_SCHEMA
            || header.header_bytes != sizeof(KboPerfSnapshotHeader)
            || header.record_header_bytes != sizeof(KboPerfSnapshotPlayerRecordHeader)
            || header.player_scan_bytes != OOTP27_PLAYER_SCAN_BYTES
            || header.copied_player_count == 0u
            || header.copied_player_count > 200000u) {
        fclose(file);
        fprintf(stderr, "snapshot header invalid: %s\n", path);
        return 0;
    }

    int player_count = (int)header.copied_player_count;
    g_players = (uint8_t*)calloc((size_t)player_count, OOTP27_PLAYER_SCAN_BYTES);
    g_player_vector = (uintptr_t*)calloc((size_t)player_count, sizeof(uintptr_t));
    if (g_players == NULL || g_player_vector == NULL) {
        fclose(file);
        free(g_player_vector);
        free(g_players);
        g_player_vector = NULL;
        g_players = NULL;
        fprintf(stderr, "snapshot allocation failed players=%d\n", player_count);
        return 0;
    }

    for (int i = 0; i < player_count; i++) {
        KboPerfSnapshotPlayerRecordHeader record;
        if (fread(&record, 1u, sizeof(record), file) != sizeof(record)) {
            fclose(file);
            bench_free_players();
            fprintf(stderr, "snapshot record header read failed index=%d\n", i);
            return 0;
        }
        uint8_t* player = g_players + ((size_t)i * OOTP27_PLAYER_SCAN_BYTES);
        if (fread(player, 1u, OOTP27_PLAYER_SCAN_BYTES, file) != OOTP27_PLAYER_SCAN_BYTES) {
            fclose(file);
            bench_free_players();
            fprintf(stderr, "snapshot player bytes read failed index=%d\n", i);
            return 0;
        }
        (void)record;
        g_player_vector[i] = (uintptr_t)player;
    }

    fclose(file);
    g_player_count = player_count;
    bench_register_readable_range(g_players, (size_t)player_count * OOTP27_PLAYER_SCAN_BYTES);
    bench_register_readable_range(g_player_vector, (size_t)player_count * sizeof(uintptr_t));
    fprintf(
        stderr,
        "Loaded snapshot players=%d source_count=%u skipped=%u date=%u path=%s\n",
        g_player_count,
        header.source_player_count,
        header.skipped_player_count,
        header.yyyymmdd,
        path);
    return 1;
}

static void bench_free_players(void)
{
    free(g_team_vector);
    free(g_teams);
    free(g_player_vector);
    free(g_players);
    g_team_vector = NULL;
    g_teams = NULL;
    g_team_count = 0;
    g_player_vector = NULL;
    g_players = NULL;
    g_player_count = 0;
    g_bench_readable_range_count = 0;
}

static void bench_step_date_serial(void* context)
{
    BenchCursor* cursor = (BenchCursor*)context;
    uint32_t day = 1u + (cursor->index++ % 28u);
    g_bench_sink += (int64_t)kbo_date_serial(2027u, 3u, day);
}

static void bench_step_csv_line_fields(void* context)
{
    BenchCursor* cursor = (BenchCursor*)context;
    char fields[8][64];
    const char* line = (cursor->index++ & 1u)
        ? "123,\"Kim, Min\",456,starter,\"quoted field\",20270315"
        : "987,\"Lee, Ji\",654,reliever,\"plain field\",20271102";
    g_bench_sink += kbo_csv_read_line_fields(line, fields[0], sizeof(fields[0]), 6);
}

static void bench_step_foreign_value_score(void* context)
{
    BenchCursor* cursor = (BenchCursor*)context;
    uint32_t index = cursor->index++ % (uint32_t)g_player_count;
    g_bench_sink += kbo_foreign_waiver_value_score(g_players + ((size_t)index * OOTP27_PLAYER_SCAN_BYTES));
}

static void bench_step_fa_role_count_cache_hit(void* context)
{
    BenchTeamContext* team = (BenchTeamContext*)context;
    g_bench_sink += kbo_fa_team_role_count(team != NULL ? team->team_id : g_bench_primary_team_id, 0);
}

static void bench_step_fa_role_count_cold_scan(void* context)
{
    BenchCursor* cursor = (BenchCursor*)context;
    uint32_t team_id = 100000u + cursor->index++;
    g_bench_sink += kbo_fa_team_role_count(team_id, 0);
}

static void bench_step_fa_candidate_score_cached_roles(void* context)
{
    BenchCursor* cursor = (BenchCursor*)context;
    uint32_t index = cursor->index++ % (uint32_t)g_player_count;
    char reason[96];
    g_bench_sink += kbo_fa_protection_candidate_score(
        g_players + ((size_t)index * OOTP27_PLAYER_SCAN_BYTES),
        g_bench_primary_team_id,
        reason,
        sizeof(reason));
}

static void bench_step_foreign_org_count_cache_hit(void* context)
{
    BenchTeamContext* team = (BenchTeamContext*)context;
    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(
        team != NULL ? team->team_id : g_bench_primary_team_id,
        &foreign_count,
        &asian_count,
        &non_asian_count);
    g_bench_sink += (int64_t)foreign_count + (int64_t)asian_count + (int64_t)non_asian_count;
}

static void bench_step_foreign_org_count_snapshot_hit(void* context)
{
    BenchTeamContext* team = (BenchTeamContext*)context;
    uint32_t team_id = team != NULL ? team->team_id : g_bench_primary_team_id;
    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_foreign_org_count_cache_note_player_assignment_change(0u, 0u, 0u, team_id, 0u, 0u, 0u, 0);
    kbo_foreign_org_count_refresh_benchmark_snapshot_tick();
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
    g_bench_sink += (int64_t)foreign_count + (int64_t)asian_count + (int64_t)non_asian_count;
}

static void bench_step_foreign_org_count_snapshot_rebuild(void* context)
{
    BenchTeamContext* team = (BenchTeamContext*)context;
    uint32_t team_id = team != NULL ? team->team_id : g_bench_primary_team_id;
    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_foreign_org_count_cache_note_roster_mutation();
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
    g_bench_sink += (int64_t)foreign_count + (int64_t)asian_count + (int64_t)non_asian_count;
}

static void bench_step_foreign_org_count_fresh_scan(void* context)
{
    BenchTeamContext* team = (BenchTeamContext*)context;
    uint32_t team_id = team != NULL ? team->team_id : g_bench_primary_team_id;
    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe_fresh(team_id, &foreign_count, &asian_count, &non_asian_count);
    g_bench_sink += (int64_t)foreign_count + (int64_t)asian_count + (int64_t)non_asian_count;
}

static uint8_t* bench_candidate_player(BenchForeignCandidateContext* context)
{
    if (context == NULL || context->candidate_index < 0 || context->candidate_index >= g_player_count) {
        return NULL;
    }
    return g_players + ((size_t)context->candidate_index * OOTP27_PLAYER_SCAN_BYTES);
}

static void bench_step_foreign_candidate_policy(void* context)
{
    BenchForeignCandidateContext* candidate = (BenchForeignCandidateContext*)context;
    uint8_t* player = bench_candidate_player(candidate);
    if (candidate == NULL || player == NULL) {
        return;
    }
    if (candidate->force_cold) {
        InterlockedIncrement(&g_kbo_custom_foreign_pending_offer_generation);
    }
    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = 0u;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    int allowed = kbo_custom_foreign_policy_team_allows_candidate(
        candidate->team_id,
        player,
        &effective_before,
        &effective_after,
        &effective_limit,
        &slot_type,
        &injured_player_id);
    g_bench_sink += allowed
        + (int64_t)effective_before
        + (int64_t)effective_after
        + (int64_t)effective_limit
        + (int64_t)slot_type
        + (int64_t)injured_player_id;
}

static void bench_step_foreign_fast_block(void* context)
{
    BenchForeignCandidateContext* candidate = (BenchForeignCandidateContext*)context;
    uint8_t* player = bench_candidate_player(candidate);
    if (candidate == NULL || player == NULL) {
        return;
    }
    if (candidate->force_cold) {
        InterlockedIncrement(&g_kbo_custom_foreign_pending_offer_generation);
    }
    uint32_t player_id = 0u;
    int blocked = kbo_fast_block_fa_candidate_before_original(
        (uintptr_t)player,
        (int32_t)candidate->team_id,
        "bench",
        &player_id);
    g_bench_sink += blocked + (int64_t)player_id;
}

static void bench_step_foreign_candidate_extra_slots(void* context)
{
    BenchForeignCandidateContext* candidate = (BenchForeignCandidateContext*)context;
    uint8_t* player = bench_candidate_player(candidate);
    if (candidate == NULL || player == NULL) {
        return;
    }
    if (candidate->force_cold) {
        InterlockedIncrement(&g_kbo_custom_foreign_pending_offer_generation);
    }
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t extra_slots = kbo_custom_foreign_policy_extra_slots_for_candidate(
        candidate->team_id,
        player,
        &slot_type,
        &injured_player_id);
    g_bench_sink += (int64_t)extra_slots + (int64_t)slot_type + (int64_t)injured_player_id;
}

static void bench_seed_foreign_candidate_caches(
    BenchForeignCandidateContext* allowed,
    BenchForeignCandidateContext* blocked)
{
    BenchForeignCandidateContext seed_allowed = *allowed;
    BenchForeignCandidateContext seed_blocked = *blocked;
    seed_allowed.force_cold = 1;
    seed_blocked.force_cold = 1;
    bench_step_foreign_candidate_policy(&seed_allowed);
    bench_step_foreign_candidate_policy(&seed_blocked);
}

static void bench_step_fa_protection_build_cache_hit(void* context)
{
    BenchFaProtectionContext* fa = (BenchFaProtectionContext*)context;
    if (fa == NULL) {
        return;
    }
    int count = kbo_build_fa_compensation_protected_candidates(
        &fa->record,
        fa->candidates,
        (int)(sizeof(fa->candidates) / sizeof(fa->candidates[0])),
        fa->auto_protected,
        (int)(sizeof(fa->auto_protected) / sizeof(fa->auto_protected[0])));
    g_bench_sink += count;
}

static void bench_step_fa_protection_build_cold(void* context)
{
    BenchFaProtectionContext* fa = (BenchFaProtectionContext*)context;
    if (fa == NULL) {
        return;
    }
    KboFaCompensationRecord cold = fa->record;
    cold.league_id = fa->cold_league_cursor++;
    int count = kbo_build_fa_compensation_protected_candidates(
        &cold,
        fa->candidates,
        (int)(sizeof(fa->candidates) / sizeof(fa->candidates[0])),
        fa->auto_protected,
        (int)(sizeof(fa->auto_protected) / sizeof(fa->auto_protected[0])));
    g_bench_sink += count;
}

static void bench_print_header(FILE* out)
{
    fputs("case,iterations,batch,total_ms,avg_us,p50_us,p95_us,max_us\n", out);
    fflush(out);
}

int main(int argc, char** argv)
{
    BenchConfig config;
    config.samples = bench_parse_int_arg(argc, argv, "--samples", 60, 10, 100000);
    config.players = bench_parse_int_arg(argc, argv, "--players", 5000, 100, 100000);
    const char* snapshot_path = bench_parse_string_arg(argc, argv, "--snapshot");
    const char* plan_path = bench_parse_string_arg(argc, argv, "--plan");
    const char* dataset_label = snapshot_path != NULL ? "snapshot" : "synthetic";
    g_bench_readability_mode = bench_parse_readability_mode(argc, argv);

    if (!QueryPerformanceFrequency(&g_qpc_frequency) || g_qpc_frequency.QuadPart <= 0) {
        fprintf(stderr, "QueryPerformanceFrequency failed\n");
        return 1;
    }
    if (snapshot_path != NULL ? !bench_load_snapshot(snapshot_path) : !bench_init_players(config.players)) {
        fprintf(stderr, "failed to initialize benchmark players\n");
        return 1;
    }
    bench_collect_team_ids();
    bench_select_foreign_candidates();
    if (!bench_init_teams_from_collected_ids()) {
        fprintf(stderr, "failed to initialize benchmark teams\n");
        bench_free_players();
        return 1;
    }

    BenchCursor date_cursor = {0};
    BenchCursor csv_cursor = {0};
    BenchCursor value_cursor = {0};
    BenchCursor cold_role_cursor = {0};
    BenchCursor candidate_cursor = {0};
    BenchTeamContext primary_team;
    primary_team.team_id = g_bench_primary_team_id;
    BenchForeignCandidateContext candidate_allowed_cache;
    candidate_allowed_cache.index = 0u;
    candidate_allowed_cache.team_id = g_bench_primary_team_id;
    candidate_allowed_cache.candidate_index = g_bench_allowed_candidate_index;
    candidate_allowed_cache.force_cold = 0;
    BenchForeignCandidateContext candidate_blocked_cache;
    candidate_blocked_cache.index = 0u;
    candidate_blocked_cache.team_id = g_bench_primary_team_id;
    candidate_blocked_cache.candidate_index = g_bench_blocked_candidate_index;
    candidate_blocked_cache.force_cold = 0;
    BenchForeignCandidateContext candidate_allowed_cold = candidate_allowed_cache;
    candidate_allowed_cold.force_cold = 1;
    BenchForeignCandidateContext candidate_blocked_cold = candidate_blocked_cache;
    candidate_blocked_cold.force_cold = 1;
    BenchFaProtectionContext fa_protection;
    bench_prepare_fa_protection_context(&fa_protection);
    BenchPlan replay_plan;
    memset(&replay_plan, 0, sizeof(replay_plan));
    BenchPlan* active_plan = NULL;
    if (plan_path != NULL) {
        if (!bench_load_plan(plan_path, &replay_plan)) {
            bench_free_players();
            return 1;
        }
        active_plan = &replay_plan;
    }

    if (active_plan != NULL) {
        bench_seed_replay_foreign_org_counts(g_bench_primary_team_id);
        bench_seed_foreign_candidate_caches(&candidate_allowed_cache, &candidate_blocked_cache);
    } else {
        kbo_fa_team_role_count(g_bench_primary_team_id, 0);
        kbo_foreign_org_count_cache_note_roster_mutation();
        bench_step_foreign_org_count_cache_hit(&primary_team);
        bench_seed_foreign_candidate_caches(&candidate_allowed_cache, &candidate_blocked_cache);
        bench_step_fa_protection_build_cache_hit(&fa_protection);
    }

    fprintf(
        stderr,
        "Bench dataset=%s players=%d teams=%d primary_team=%u primary_player=%u allowed_candidate=%d blocked_candidate=%d readability=%s\n",
        dataset_label,
        g_player_count,
        g_team_count,
        g_bench_primary_team_id,
        g_bench_primary_team_player_id,
        g_bench_allowed_candidate_index,
        g_bench_blocked_candidate_index,
        g_bench_readability_mode == BENCH_READABILITY_FAST ? "fast" : "virtual");

    char case_name[128] = {0};
    bench_print_header(stdout);
    bench_run_case_from_plan_or_default(stdout, "core.date_serial", config.samples, 10000, bench_step_date_serial, &date_cursor, active_plan);
    bench_run_case_from_plan_or_default(stdout, "core.csv_read_line_fields", config.samples, 2000, bench_step_csv_line_fields, &csv_cursor, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.value_score.%s_player", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 200, bench_step_foreign_value_score, &value_cursor, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.org_count.cache_hit.%s_vector", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 2000, bench_step_foreign_org_count_cache_hit, &primary_team, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.org_count.snapshot_hit_plus_invalidate.%s_vector", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 2000, bench_step_foreign_org_count_snapshot_hit, &primary_team, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.org_count.snapshot_rebuild.%s_vector", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples < 20 ? config.samples : 20, 1, bench_step_foreign_org_count_snapshot_rebuild, &primary_team, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.org_count.fresh_scan.%s_vector", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples < 30 ? config.samples : 30, 1, bench_step_foreign_org_count_fresh_scan, &primary_team, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.candidate.cache_allowed.%s_player", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 1000, bench_step_foreign_candidate_policy, &candidate_allowed_cache, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.candidate.cache_blocked.%s_player", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 1000, bench_step_foreign_candidate_policy, &candidate_blocked_cache, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.candidate.allowed.%s_player", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 100, bench_step_foreign_candidate_policy, &candidate_allowed_cold, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.candidate.blocked.%s_player", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 100, bench_step_foreign_candidate_policy, &candidate_blocked_cold, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.candidate.extra_slots.%s_player", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 100, bench_step_foreign_candidate_extra_slots, &candidate_blocked_cold, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.fast_block.allowed.%s_player", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 1000, bench_step_foreign_fast_block, &candidate_allowed_cache, active_plan);
    snprintf(case_name, sizeof(case_name), "foreign.fast_block.custom_blocked.%s_player", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 100, bench_step_foreign_fast_block, &candidate_blocked_cold, active_plan);
    snprintf(case_name, sizeof(case_name), "fa.role_count.cache_hit.%s_vector", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 2000, bench_step_fa_role_count_cache_hit, &primary_team, active_plan);
    snprintf(case_name, sizeof(case_name), "fa.candidate_score.cached_roles.%s_player", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 100, bench_step_fa_candidate_score_cached_roles, &candidate_cursor, active_plan);
    snprintf(case_name, sizeof(case_name), "fa.protection_build.cache_hit.%s_vector", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples, 100, bench_step_fa_protection_build_cache_hit, &fa_protection, active_plan);
    snprintf(case_name, sizeof(case_name), "fa.protection_build.cold.%s_vector", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples < 30 ? config.samples : 30, 1, bench_step_fa_protection_build_cold, &fa_protection, active_plan);
    snprintf(case_name, sizeof(case_name), "fa.role_count.cold_scan.%s_vector", dataset_label);
    bench_run_case_from_plan_or_default(stdout, case_name, config.samples < 30 ? config.samples : 30, 1, bench_step_fa_role_count_cold_scan, &cold_role_cursor, active_plan);

    if (active_plan != NULL) {
        for (int i = 0; i < active_plan->count; i++) {
            BenchPlanEntry* entry = &active_plan->entries[i];
            if (entry->matched == 0) {
                fprintf(stderr, "Replay plan selector did not match a bench case: %s\n", entry->selector);
            }
        }
    }

    if (g_bench_sink == INT64_MIN) {
        fputs("", stderr);
    }
    bench_free_players();
    return 0;
}

void kbo_log_runtimef_at(const char* file, int line, const char* fmt, ...)
{
    (void)file;
    (void)line;
    (void)fmt;
}

void kbo_log_runtime_line_at(const char* file, int source_line, const char* line)
{
    (void)file;
    (void)source_line;
    (void)line;
}

int memory_range_readable(const void* ptr, SIZE_T size)
{
    if (ptr == NULL || size == 0u) {
        return 0;
    }
    if (g_bench_readability_mode == BENCH_READABILITY_FAST && bench_range_readable(ptr, size)) {
        return 1;
    }

    uintptr_t start = (uintptr_t)ptr;
    uintptr_t end = start + size;
    if (start < 0x10000u || end <= start) {
        return 0;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) {
        return 0;
    }

    DWORD protect = mbi.Protect & 0xffu;
    int readable = protect == PAGE_READONLY
        || protect == PAGE_READWRITE
        || protect == PAGE_WRITECOPY
        || protect == PAGE_EXECUTE_READ
        || protect == PAGE_EXECUTE_READWRITE
        || protect == PAGE_EXECUTE_WRITECOPY;
    uintptr_t region_end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return readable && end <= region_end;
}

int kbo_player_pointer_plausible(uintptr_t player_ptr)
{
    return memory_range_readable((const void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
}

int find_kbo_global_player_vector(uintptr_t* out_vector, int32_t* out_count, uint32_t* out_offset)
{
    if (out_vector != NULL) {
        *out_vector = (uintptr_t)g_player_vector;
    }
    if (out_count != NULL) {
        *out_count = g_player_count;
    }
    if (out_offset != NULL) {
        *out_offset = 0u;
    }
    return g_player_vector != NULL && g_player_count > 0;
}

uint8_t* find_kbo_team_by_numeric_id_any_league(uint32_t team_id, int allow_deleted)
{
    if (team_id == 0u || g_team_vector == NULL || g_team_count <= 0) {
        return NULL;
    }
    for (int32_t i = 0; i < g_team_count; i++) {
        uintptr_t team_ptr = g_team_vector[i];
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (!allow_deleted && team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0u) {
            continue;
        }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) == team_id) {
            return team;
        }
    }
    return NULL;
}

uint8_t* find_kbo_team_by_csv_id_any_league(const char* team_id, int allow_deleted)
{
    (void)team_id;
    (void)allow_deleted;
    return NULL;
}

int kbo_player_current_assignment_matches_team_or_affiliate(uint8_t* player, uint32_t team_id)
{
    if (player == NULL || team_id == 0u || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == team_id
        || *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == team_id
        || *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == team_id;
}

void kbo_ensure_foreign_replacement_player_seeds_loaded(void)
{
}

int kbo_foreign_replacement_player_seed_matches_loaded(uint8_t* player, uint8_t* out_slot_type)
{
    (void)player;
    if (out_slot_type != NULL) {
        *out_slot_type = 0u;
    }
    return 0;
}

int kbo_foreign_injury_player_excluded_from_foreign_count(uint32_t team_id, uint32_t player_id)
{
    (void)team_id;
    (void)player_id;
    return 0;
}

void kbo_foreign_injury_build_exclusion_snapshot(KboForeignInjuryExclusionSnapshot* out)
{
    if (out != NULL) {
        out->count = 0;
    }
}

int kbo_foreign_injury_player_excluded_from_foreign_count_snapshot(
    const KboForeignInjuryExclusionSnapshot* snapshot,
    uint32_t team_id,
    uint32_t player_id)
{
    (void)snapshot;
    return kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id);
}

void kbo_copy_player_display_name(uint8_t* player, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    uint32_t player_id = 0u;
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    }
    snprintf(out, out_size, "Player #%u", player_id);
}

int kbo_profiler_begin(LARGE_INTEGER* out_start)
{
    if (out_start != NULL) {
        out_start->QuadPart = 0;
    }
    return 0;
}

void kbo_profiler_end(const char* name, const LARGE_INTEGER* start)
{
    (void)name;
    (void)start;
}

int kbo_profiler_is_enabled(void)
{
    return 0;
}

void kbo_profiler_record_us(const char* name, unsigned long long elapsed_us)
{
    (void)name;
    (void)elapsed_us;
}

void kbo_profiler_reset_enabled_cache(void)
{
}

int kbo_fix_enabled(void)
{
    return 1;
}

uint32_t kbo_resolve_kbo_league_id(void)
{
    return 1u;
}

int kbo_foreign_waiver_ai_enabled(void)
{
    return 0;
}

int kbo_custom_foreign_policy_enabled(void)
{
    return 1;
}

int kbo_get_foreign_waiver_current_yyyymmdd(uint32_t* out_today)
{
    if (out_today != NULL) {
        *out_today = 20260310u;
    }
    return 1;
}

void kbo_custom_foreign_count_pending_offers(
    uint32_t team_id,
    uint32_t today,
    uint32_t candidate_id,
    uint32_t* out_asian_pending,
    uint32_t* out_non_asian_pending,
    int* out_candidate_pending)
{
    (void)team_id;
    (void)today;
    (void)candidate_id;
    if (out_asian_pending != NULL) { *out_asian_pending = 0u; }
    if (out_non_asian_pending != NULL) { *out_non_asian_pending = 0u; }
    if (out_candidate_pending != NULL) { *out_candidate_pending = 0; }
}

int kbo_foreign_injury_replacement_enabled(void)
{
    return 1;
}

int kbo_foreign_injury_replacement_in_season_window(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    const char* source,
    const char* context)
{
    (void)league_id;
    (void)today_yyyymmdd;
    (void)source;
    (void)context;
    return 0;
}

int kbo_team_has_foreign_injury_slot_for_candidate_any(
    uint32_t team_id,
    int allow_asian_slot,
    uint32_t candidate_player_id,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id)
{
    (void)team_id;
    (void)allow_asian_slot;
    (void)candidate_player_id;
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }
    if (out_replacement_player_id != NULL) { *out_replacement_player_id = 0u; }
    return 0;
}

const char* kbo_foreign_injury_slot_label(uint8_t slot_type)
{
    switch (slot_type) {
        case 1u: return "Regular";
        case 2u: return "Asian quota";
        default: return "none";
    }
}

int kbo_retention_opportunity_get_summary(
    uint32_t team_id,
    uint32_t today,
    KboForeignRetentionOpportunitySummary* out_summary)
{
    (void)team_id;
    (void)today;
    if (out_summary != NULL) {
        memset(out_summary, 0, sizeof(*out_summary));
    }
    return 0;
}

int32_t kbo_retention_opportunity_score_margin_for_best(int32_t best_score)
{
    return best_score / 10;
}

int kbo_retention_opportunity_probe_should_block(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t today,
    uint32_t asian_count,
    uint32_t non_asian_count,
    uint32_t pending_asian_count,
    uint32_t pending_non_asian_count,
    uint32_t asian_after,
    uint32_t non_asian_after,
    uint32_t effective_before,
    uint32_t effective_after,
    uint32_t effective_limit,
    int candidate_pending,
    int already_in_org,
    int allowed)
{
    (void)team_id;
    (void)candidate;
    (void)today;
    (void)asian_count;
    (void)non_asian_count;
    (void)pending_asian_count;
    (void)pending_non_asian_count;
    (void)asian_after;
    (void)non_asian_after;
    (void)effective_before;
    (void)effective_after;
    (void)effective_limit;
    (void)candidate_pending;
    (void)already_in_org;
    (void)allowed;
    return 0;
}

int kbo_find_active_foreign_waiver_holder(
    uint32_t player_id,
    uint32_t today_yyyymmdd,
    uint32_t* out_team_id)
{
    (void)player_id;
    (void)today_yyyymmdd;
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    return 0;
}

void kbo_prune_expired_foreign_waiver_rights(uint32_t today_yyyymmdd)
{
    (void)today_yyyymmdd;
}

int kbo_has_active_foreign_waiver_right(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t today_yyyymmdd)
{
    (void)team_id;
    (void)player_id;
    (void)today_yyyymmdd;
    return 0;
}

int kbo_sync_active_foreign_waiver_right_to_memory(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    uint32_t today_yyyymmdd)
{
    (void)player;
    (void)player_id;
    (void)holder_team_id;
    (void)today_yyyymmdd;
    return 0;
}

void kbo_record_recent_foreign_offer_block(
    uint32_t player_id,
    uint32_t requester_team_id,
    uint32_t holder_team_id,
    uint32_t today)
{
    (void)player_id;
    (void)requester_team_id;
    (void)holder_team_id;
    (void)today;
}

void kbo_record_recent_custom_foreign_policy_block(
    uint32_t player_id,
    uint32_t requester_team_id,
    uint32_t today)
{
    (void)player_id;
    (void)requester_team_id;
    (void)today;
}

int32_t kbo_read_clamped_policy_int(
    const char* file_name,
    const char* key,
    int32_t fallback,
    int32_t min_value,
    int32_t max_value)
{
    (void)file_name;
    (void)key;
    (void)min_value;
    (void)max_value;
    return fallback;
}

uint32_t kbo_read_u32_leading_number_from_foreign_policy_file(const char* file_name)
{
    (void)file_name;
    return 0u;
}

int read_kbo_localappdata_flag_file(const char* file_name)
{
    (void)file_name;
    return 0;
}

int get_kbo_asian_quota_nation_ids_path(char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    return 0;
}

int32_t kbo_get_asian_quota_salary_limit(void)
{
    return 200000;
}
