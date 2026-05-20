#include "../../runtime/common/custom_events_common.h"
#include "builtin_and_projected.h"
#include "policy/projected_policy.h"
#include <stdio.h>
#include <string.h>
#include "../../../core/dates/core_text_date.h"
#include "../../../core/csv/core_csv.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/core_flags/api/flags_api.h"

typedef struct KboAsianGamesProjectedHost {
    char city[48];
    char country[48];
} KboAsianGamesProjectedHost;

static int kbo_get_global_asian_games_projected_hosts_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2u) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_global_data_file("asian_games_projected_hosts.csv", out, out_size);
}

static int kbo_get_save_asian_games_projected_hosts_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2u) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("config\\asian_games_projected_hosts.csv", out, out_size);
}

static int kbo_import_asian_games_projected_hosts_file(
    const char* path,
    KboAsianGamesProjectedHost* hosts,
    int capacity,
    int count)
{
    if (path == NULL || path[0] == '\0' || hosts == NULL || capacity <= 0 || count >= capacity) {
        return count;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return count;
    }

    while (count < capacity && kbo_csv_reader_next_row(reader)) {
        char fields[2][64];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 2);
        char* city = field_count > 0 ? fields[0] : "";
        char* country = field_count > 1 ? fields[1] : "";
        if (city[0] == '\0'
                || city[0] == '#'
                || city[0] == ';'
                || country[0] == '\0'
                || ascii_equals_ignore_case(city, "city")
                || ascii_equals_ignore_case(city, "host_city")) {
            continue;
        }
        kbo_asian_games_schedule_copy_text(hosts[count].city, sizeof(hosts[count].city), city);
        kbo_asian_games_schedule_copy_text(hosts[count].country, sizeof(hosts[count].country), country);
        count++;
    }

    kbo_csv_reader_close(reader);
    return count;
}

static int kbo_load_asian_games_projected_hosts(KboAsianGamesProjectedHost* hosts, int capacity)
{
    if (hosts == NULL || capacity <= 0) {
        return 0;
    }
    memset(hosts, 0, sizeof(hosts[0]) * (size_t)capacity);

    int count = 0;
    char global_path[MAX_PATH] = {0};
    char save_path[MAX_PATH] = {0};
    kbo_get_global_asian_games_projected_hosts_path(global_path, sizeof(global_path));
    kbo_get_save_asian_games_projected_hosts_path(save_path, sizeof(save_path));
    count = kbo_import_asian_games_projected_hosts_file(global_path, hosts, capacity, count);
    count = kbo_import_asian_games_projected_hosts_file(save_path, hosts, capacity, count);
    return count;
}

static int kbo_asian_games_projected_host_conflicts_with_schedule(
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t year,
    const char* city,
    const char* country,
    uint32_t city_cooldown_years,
    uint32_t country_cooldown_years)
{
    if (schedule == NULL || schedule->year == 0u || schedule->year >= year) {
        return 0;
    }
    uint32_t years_since = year - schedule->year;
    if (city != NULL
            && city[0] != '\0'
            && schedule->host_city[0] != '\0'
            && years_since <= city_cooldown_years
            && ascii_equals_ignore_case(schedule->host_city, city)) {
        return 1;
    }
    if (country != NULL
            && country[0] != '\0'
            && schedule->host_country[0] != '\0'
            && years_since <= country_cooldown_years
            && ascii_equals_ignore_case(schedule->host_country, country)) {
        return 1;
    }
    return 0;
}

static int kbo_asian_games_projected_host_is_available(
    uint32_t year,
    const char* city,
    const char* country,
    const KboAsianGamesProjectedPolicy* policy,
    uint32_t city_cooldown_years,
    uint32_t country_cooldown_years)
{
    if (city == NULL || city[0] == '\0' || country == NULL || country[0] == '\0' || policy == NULL) {
        return 0;
    }

    for (int i = 0; i < g_kbo_asian_games_schedule_seed_count; i++) {
        if (kbo_asian_games_projected_host_conflicts_with_schedule(
                &g_kbo_asian_games_schedule_seeds[i],
                year,
                city,
                country,
                city_cooldown_years,
                country_cooldown_years)) {
            return 0;
        }
    }

    for (uint32_t previous_year = year > policy->cycle_years ? year - policy->cycle_years : 0u;
            previous_year >= policy->projected_start_year && year - previous_year <= city_cooldown_years;
            previous_year -= policy->cycle_years) {
        KboAsianGamesScheduleSeed previous;
        kbo_build_projected_asian_games_schedule(previous_year, &previous);
        if (kbo_asian_games_projected_host_conflicts_with_schedule(
                &previous,
                year,
                city,
                country,
                city_cooldown_years,
                country_cooldown_years)) {
            return 0;
        }
        if (previous_year < policy->projected_start_year + policy->cycle_years) {
            break;
        }
    }

    return 1;
}

static int kbo_choose_projected_asian_games_host_index(
    uint32_t year,
    const KboAsianGamesProjectedHost* hosts,
    int host_count,
    const KboAsianGamesProjectedPolicy* policy)
{
    if (hosts == NULL || host_count <= 0 || policy == NULL) {
        return -1;
    }
    uint32_t start = kbo_asian_games_projected_hash(year, policy->host_hash_salt) % (uint32_t)host_count;
    for (int pass = 0; pass < 2; pass++) {
        uint32_t city_cooldown_years = pass == 0
            ? policy->host_city_cooldown_years
            : policy->host_fallback_city_cooldown_years;
        uint32_t country_cooldown_years = pass == 0
            ? policy->host_country_cooldown_years
            : policy->host_fallback_country_cooldown_years;
        for (int offset = 0; offset < host_count; offset++) {
            int index = (int)((start + (uint32_t)offset) % (uint32_t)host_count);
            if (kbo_asian_games_projected_host_is_available(
                    year,
                    hosts[index].city,
                    hosts[index].country,
                    policy,
                    city_cooldown_years,
                    country_cooldown_years)) {
                return index;
            }
        }
    }
    return (int)start;
}

int kbo_choose_projected_asian_games_host(
    uint32_t year,
    char* out_city,
    size_t city_size,
    char* out_country,
    size_t country_size)
{
    if (out_city == NULL || city_size == 0u || out_country == NULL || country_size == 0u) {
        return 0;
    }
    out_city[0] = '\0';
    out_country[0] = '\0';

    KboAsianGamesProjectedPolicy policy;
    kbo_load_asian_games_projected_policy(&policy);
    int capacity = (int)policy.host_max_count;
    if (capacity <= 0) {
        return 0;
    }
    KboAsianGamesProjectedHost* hosts = (KboAsianGamesProjectedHost*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)capacity * sizeof(KboAsianGamesProjectedHost));
    if (hosts == NULL) {
        return 0;
    }
    int host_count = kbo_load_asian_games_projected_hosts(hosts, capacity);
    int host_index = kbo_choose_projected_asian_games_host_index(year, hosts, host_count, &policy);
    if (host_index < 0) {
        HeapFree(GetProcessHeap(), 0, hosts);
        return 0;
    }

    kbo_asian_games_schedule_copy_text(out_city, city_size, hosts[host_index].city);
    kbo_asian_games_schedule_copy_text(out_country, country_size, hosts[host_index].country);
    HeapFree(GetProcessHeap(), 0, hosts);
    return 1;
}
