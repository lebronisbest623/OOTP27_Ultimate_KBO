#include "../military_seed_registry_internal.h"
#include "../../../../core/csv/core_csv.h"
#include "../../../../core/dates/constants/kbo_date_constants.h"
#include "../sql/military_resolved_seed_sql_store.h"

void kbo_lock_military_service_seeds(void)
{
    kbo_lock_enter(&g_kbo_military_service_seed_lock);
}

void kbo_unlock_military_service_seeds(void)
{
    kbo_lock_leave(&g_kbo_military_service_seed_lock);
}

int kbo_add_military_service_seed_locked(const KboMilitaryServiceSeed* seed)
{
    if (seed == NULL || (seed->key[0] == '\0' && seed->player_id == 0u)) {
        return 0;
    }
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* existing = &g_kbo_military_service_seeds[i];
        if ((seed->player_id != 0u && existing->player_id == seed->player_id)
                || (seed->key[0] != '\0' && existing->key[0] != '\0'
                    && _stricmp(existing->key, seed->key) == 0)) {
            if (existing->player_id == 0u) {
                existing->player_id = seed->player_id;
            }
            if (seed->service_team_code[0] != '\0') {
                snprintf(existing->service_team_code, sizeof(existing->service_team_code), "%s", seed->service_team_code);
            }
            if (seed->original_team_code[0] != '\0') {
                snprintf(existing->original_team_code, sizeof(existing->original_team_code), "%s", seed->original_team_code);
            }
            if (seed->service_start_yyyymmdd != 0u) {
                existing->service_start_yyyymmdd = seed->service_start_yyyymmdd;
            }
            if (seed->service_return_yyyymmdd != 0u) {
                existing->service_return_yyyymmdd = seed->service_return_yyyymmdd;
            }
            if (seed->service_total_days > 0) {
                existing->service_total_days = seed->service_total_days;
            }
            return 1;
        }
    }
    if (g_kbo_military_service_seed_count >= KBO_MILITARY_SERVICE_SEED_MAX) {
        return 0;
    }
    g_kbo_military_service_seeds[g_kbo_military_service_seed_count++] = *seed;
    return 1;
}

int kbo_load_military_service_seed_file_locked(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }
    int loaded = 0;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[8][96];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 8);
        KboMilitaryServiceSeed seed;
        if (kbo_parse_military_service_seed_fields(fields, field_count, &seed)
                && kbo_add_military_service_seed_locked(&seed)) {
            loaded++;
        }
    }
    kbo_csv_reader_close(reader);
    if (loaded > 0) {
        kbo_log_runtimef("KBO military service seed loaded=%d path=%s", loaded, path);
    }
    return loaded;
}

int kbo_load_military_service_resolved_cache_locked(void)
{
    KboMilitaryServiceSeed* resolved = (KboMilitaryServiceSeed*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_MILITARY_SERVICE_SEED_MAX * sizeof(KboMilitaryServiceSeed));
    if (resolved == NULL) {
        return 0;
    }

    int count = 0;
    int ok = kbo_military_resolved_seed_sql_load(
        resolved,
        KBO_MILITARY_SERVICE_SEED_MAX,
        &count);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, resolved);
        return 0;
    }

    int loaded = 0;
    for (int i = 0; i < count; i++) {
        if (kbo_add_military_service_seed_locked(&resolved[i])) {
            loaded++;
        }
    }
    HeapFree(GetProcessHeap(), 0, resolved);
    return loaded;
}

int kbo_persist_military_service_resolved_cache_locked(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_military_resolved_seed_sql_path(path, sizeof(path))) {
        return 0;
    }
    if (!kbo_military_resolved_seed_sql_replace_all(
            g_kbo_military_service_seeds,
            g_kbo_military_service_seed_count)) {
        kbo_log_runtimef("KBO military service resolved cache sqlite persist failed path=%s", path);
        return 0;
    }
    return 1;
}

uint32_t kbo_military_resolve_player_id_from_players_dat_record_start(
    const uint8_t* raw,
    size_t read,
    size_t key_pos)
{
    if (raw == NULL || read < 16u || key_pos < 16u) {
        return 0u;
    }

    size_t min_start = key_pos > 512u ? key_pos - 512u : 0u;
    for (size_t start = key_pos - 16u; ; start--) {
        if (start + 16u <= read) {
            uint32_t candidate_id =
                (uint32_t)raw[start]
                | ((uint32_t)raw[start + 1u] << 8)
                | ((uint32_t)raw[start + 2u] << 16)
                | ((uint32_t)raw[start + 3u] << 24);
            uint32_t first_name_id =
                (uint32_t)raw[start + 4u]
                | ((uint32_t)raw[start + 5u] << 8)
                | ((uint32_t)raw[start + 6u] << 16)
                | ((uint32_t)raw[start + 7u] << 24);
            uint32_t last_name_id =
                (uint32_t)raw[start + 8u]
                | ((uint32_t)raw[start + 9u] << 8)
                | ((uint32_t)raw[start + 10u] << 16)
                | ((uint32_t)raw[start + 11u] << 24);
            uint8_t day = raw[start + 12u];
            uint8_t month = raw[start + 13u];
            uint16_t year = (uint16_t)raw[start + 14u] | ((uint16_t)raw[start + 15u] << 8);
            if (candidate_id != 0u
                    && first_name_id != 0u
                    && last_name_id != 0u
                    && day >= 1u && day <= 31u
                    && month >= 1u && month <= 12u
                    && year >= KBO_HISTORY_YEAR_MIN && year <= KBO_RUNTIME_HISTORICAL_YEAR_MAX
                    && kbo_military_find_player_by_id(candidate_id) != NULL) {
                return candidate_id;
            }
        }
        if (start == min_start || start == 0u) {
            break;
        }
    }
    return 0u;
}

uint32_t kbo_resolve_military_service_seed_key_from_players_dat(const char* key)
{
    if (key == NULL || key[0] == '\0' || (key[0] >= '0' && key[0] <= '9')) {
        return 0u;
    }

    char players_dat_path[MAX_PATH] = {0};
    if (!kbo_get_current_players_dat_path_for_military_seed(players_dat_path, sizeof(players_dat_path))) {
        kbo_log_runtimef("KBO military service seed unresolved key=%s reason=players.dat missing or save not written", key);
        return 0u;
    }

    HANDLE file = CreateFileA(players_dat_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("KBO military service seed unresolved key=%s reason=players.dat open failed gle=%lu", key, (unsigned long)GetLastError());
        return 0u;
    }

    LARGE_INTEGER file_size;
    file_size.QuadPart = 0;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0 || file_size.QuadPart > 512ll * 1024ll * 1024ll) {
        CloseHandle(file);
        kbo_log_runtimef("KBO military service seed unresolved key=%s reason=players.dat size unsupported", key);
        return 0u;
    }

    uint8_t* raw = (uint8_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)file_size.QuadPart);
    if (raw == NULL) {
        CloseHandle(file);
        return 0u;
    }

    DWORD read = 0;
    int read_ok = ReadFile(file, raw, (DWORD)file_size.QuadPart, &read, NULL) && read == (DWORD)file_size.QuadPart;
    CloseHandle(file);
    if (!read_ok) {
        HeapFree(GetProcessHeap(), 0, raw);
        kbo_log_runtimef("KBO military service seed unresolved key=%s reason=players.dat read failed", key);
        return 0u;
    }

    size_t key_len = strlen(key);
    uint32_t matched_player_id = 0u;
    int ambiguous = 0;
    for (size_t pos = 0; pos + key_len <= (size_t)read; pos++) {
        if (memcmp(raw + pos, key, key_len) != 0) {
            continue;
        }
        char before = pos > 0u ? (char)raw[pos - 1u] : '\0';
        char after = (char)raw[pos + key_len];
        if (kbo_military_ascii_is_seed_id_char(before) || kbo_military_ascii_is_seed_id_char(after)) {
            continue;
        }

        uint32_t record_player_id = kbo_military_resolve_player_id_from_players_dat_record_start(raw, (size_t)read, pos);
        if (record_player_id != 0u) {
            if (matched_player_id != 0u && matched_player_id != record_player_id) {
                ambiguous = 1;
                break;
            }
            matched_player_id = record_player_id;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    if (ambiguous) {
        kbo_log_runtimef("KBO military service seed unresolved key=%s reason=players.dat ambiguous", key);
        return 0u;
    }
    if (matched_player_id != 0u) {
        kbo_log_runtimef("KBO military service seed resolved via players.dat key=%s player=%u", key, matched_player_id);
    } else {
        kbo_log_runtimef("KBO military service seed unresolved key=%s reason=players.dat no matching player record header", key);
    }
    return matched_player_id;
}

int kbo_resolve_military_service_seeds_locked(void)
{
    int resolved = 0;
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* seed = &g_kbo_military_service_seeds[i];
        if (seed->player_id != 0u || seed->key[0] == '\0') {
            continue;
        }
        uint32_t player_id = 0u;
        if (seed->key[0] >= '0' && seed->key[0] <= '9') {
            player_id = (uint32_t)strtoul(seed->key, NULL, 10);
        } else {
            player_id = kbo_resolve_military_service_seed_key_from_players_dat(seed->key);
        }
        if (player_id != 0u) {
            seed->player_id = player_id;
            resolved++;
        }
    }
    if (resolved > 0) {
        kbo_persist_military_service_resolved_cache_locked();
    }
    return resolved;
}

void kbo_ensure_military_service_seeds_loaded(void)
{
    char save_seed_path[MAX_PATH] = {0};
    char global_seed_path[MAX_PATH] = {0};
    char resolved_path[MAX_PATH] = {0};
    char loaded_key[MAX_PATH * 3] = {0};
    kbo_get_save_military_service_seed_path(save_seed_path, sizeof(save_seed_path));
    kbo_get_global_military_service_seed_path(global_seed_path, sizeof(global_seed_path));
    kbo_get_save_military_service_resolved_path(resolved_path, sizeof(resolved_path));
    snprintf(
        loaded_key,
        sizeof(loaded_key),
        "%s|%s|%s",
        save_seed_path,
        global_seed_path,
        resolved_path);

    int should_reload = 0;
    if (InterlockedCompareExchange(&g_kbo_military_service_seed_loaded, 1, 0) == 0) {
        should_reload = 1;
    } else if (_stricmp(g_kbo_military_service_seed_loaded_key, loaded_key) != 0) {
        should_reload = 1;
    }

    if (should_reload) {
        kbo_lock_military_service_seeds();
        g_kbo_military_service_seed_count = 0;
        snprintf(g_kbo_military_service_seed_loaded_key, sizeof(g_kbo_military_service_seed_loaded_key), "%s", loaded_key);
        g_kbo_military_service_seed_last_resolve_tick = GetTickCount64();
        kbo_load_military_service_seed_file_locked(save_seed_path);
        if (global_seed_path[0] != '\0'
                && (save_seed_path[0] == '\0' || _stricmp(save_seed_path, global_seed_path) != 0)) {
            kbo_load_military_service_seed_file_locked(global_seed_path);
        }
        kbo_load_military_service_resolved_cache_locked();
        kbo_resolve_military_service_seeds_locked();
        kbo_unlock_military_service_seeds();
        return;
    }

    ULONGLONG now = GetTickCount64();
    ULONGLONG last_tick = g_kbo_military_service_seed_last_resolve_tick;
    if (last_tick == 0ull || now < last_tick || now - last_tick > 5000ull) {
        g_kbo_military_service_seed_last_resolve_tick = now;
        kbo_lock_military_service_seeds();
        kbo_resolve_military_service_seeds_locked();
        kbo_unlock_military_service_seeds();
    }
}

int kbo_snapshot_military_service_seeds(KboMilitaryServiceSeed* out, int max_count)
{
    if (out == NULL || max_count <= 0) {
        return 0;
    }
    kbo_ensure_military_service_seeds_loaded();
    kbo_lock_military_service_seeds();
    int count = g_kbo_military_service_seed_count;
    if (count < 0) { count = 0; }
    if (count > max_count) { count = max_count; }
    if (count > KBO_MILITARY_SERVICE_SEED_MAX) { count = KBO_MILITARY_SERVICE_SEED_MAX; }
    for (int i = 0; i < count; i++) {
        out[i] = g_kbo_military_service_seeds[i];
    }
    kbo_unlock_military_service_seeds();
    return count;
}

