#include "../internal/foreign_replacement_seed_internal.h"
#include "../../../core/csv/core_csv.h"
#include "../../../team/names/team_string.h"

static LONG g_kbo_foreign_replacement_seed_unresolved_log_count = 0;

static int kbo_seed_key_text_matches(const char* text, const char* key)
{
    char copied[KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES] = {0};
    if (text == NULL || key == NULL || key[0] == '\0') {
        return 0;
    }
    if (!copy_limited_ascii_string(text, copied, sizeof(copied))) {
        return 0;
    }
    return _stricmp(copied, key) == 0;
}

static int kbo_player_memory_string_slot_contains_ascii_seed_key(uint8_t* player, uint32_t offset, const char* key)
{
    if (player == NULL || key == NULL || offset >= OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }

    char text[KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES] = {0};
    if (offset + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET + sizeof(uintptr_t) <= OOTP27_PLAYER_SCAN_BYTES
            && copy_ootp_string_object_text(player, offset, text, sizeof(text))
            && _stricmp(text, key) == 0) {
        return 1;
    }

    if (offset + sizeof(uintptr_t) <= OOTP27_PLAYER_SCAN_BYTES
            && memory_range_readable(player + offset, sizeof(uintptr_t))) {
        uintptr_t ptr = *(uintptr_t*)(player + offset);
        if (ptr != 0u && kbo_seed_key_text_matches((const char*)ptr, key)) {
            return 1;
        }
    }

    if (kbo_seed_key_text_matches((const char*)(player + offset), key)) {
        return 1;
    }
    return 0;
}

static uintptr_t* kbo_foreign_replacement_seed_copy_player_vector_snapshot(
    uintptr_t player_vector,
    int32_t player_count,
    const char** out_failure_reason)
{
    if (out_failure_reason != NULL) {
        *out_failure_reason = "unknown";
    }
    if (player_vector == 0u || player_count <= 0 || player_count > 200000) {
        if (out_failure_reason != NULL) { *out_failure_reason = "invalid_vector"; }
        return NULL;
    }
    if ((SIZE_T)player_count > ((SIZE_T)-1 / sizeof(uintptr_t))) {
        if (out_failure_reason != NULL) { *out_failure_reason = "count_overflow"; }
        return NULL;
    }

    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        if (out_failure_reason != NULL) { *out_failure_reason = "unreadable_vector"; }
        return NULL;
    }

    uintptr_t* snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (snapshot == NULL) {
        if (out_failure_reason != NULL) { *out_failure_reason = "alloc_failed"; }
        return NULL;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        if (out_failure_reason != NULL) { *out_failure_reason = "copy_failed"; }
        return NULL;
    }

    if (out_failure_reason != NULL) {
        *out_failure_reason = NULL;
    }
    return snapshot;
}

static void kbo_log_foreign_replacement_seed_unresolved(const char* key, const char* reason, int32_t player_count)
{
    LONG n = InterlockedIncrement(&g_kbo_foreign_replacement_seed_unresolved_log_count);
    if (n > 80) {
        return;
    }
    kbo_log_runtimef(
        "foreign replacement player seed unresolved key=%s reason=%s players=%d",
        key != NULL ? key : "",
        reason != NULL ? reason : "",
        (int)player_count);
}

void kbo_lock_foreign_replacement_player_seeds(void)
{
    kbo_lock_enter(&g_kbo_foreign_replacement_player_seed_lock);
}

void kbo_unlock_foreign_replacement_player_seeds(void)
{
    kbo_lock_leave(&g_kbo_foreign_replacement_player_seed_lock);
}

int kbo_player_memory_contains_ascii_seed_key(uint8_t* player, const char* key)
{
    if (player == NULL || key == NULL || key[0] == '\0' || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    size_t key_len = strlen(key);
    if (key_len < 3u || key_len >= KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES || key_len >= OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }

    static const uint32_t export_key_offsets[] = OOTP27_PLAYER_EXPORT_KEY_STRING_OFFSETS;
    for (int i = 0; i < (int)(sizeof(export_key_offsets) / sizeof(export_key_offsets[0])); i++) {
        if (kbo_player_memory_string_slot_contains_ascii_seed_key(player, export_key_offsets[i], key)) {
            return 1;
        }
    }

    for (size_t i = 0; i + key_len < OOTP27_PLAYER_SCAN_BYTES; i++) {
        if (memcmp(player + i, key, key_len) != 0) {
            continue;
        }
        char before = i > 0u ? (char)player[i - 1u] : '\0';
        char after = (char)player[i + key_len];
        if (!kbo_ascii_is_seed_id_char(before) && !kbo_ascii_is_seed_id_char(after)) {
            return 1;
        }
    }
    return 0;
}

uint32_t kbo_resolve_foreign_replacement_player_seed_key(const char* key, uint8_t* out_slot_type)
{
    if (out_slot_type != NULL) {
        *out_slot_type = 0u;
    }
    if (key == NULL || key[0] == '\0') {
        return 0u;
    }
    if (key[0] >= '0' && key[0] <= '9') {
        return (uint32_t)strtoul(key, NULL, 10);
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_log_foreign_replacement_seed_unresolved(key, "player_vector_missing", player_count);
        return 0u;
    }

    const char* snapshot_failure_reason = NULL;
    uintptr_t* snapshot = kbo_foreign_replacement_seed_copy_player_vector_snapshot(
        player_vector,
        player_count,
        &snapshot_failure_reason);
    if (snapshot == NULL) {
        kbo_log_foreign_replacement_seed_unresolved(key, snapshot_failure_reason, player_count);
        return 0u;
    }

    uint32_t first_match = 0u;
    uint8_t first_slot_type = 0u;
    int matches = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_player_memory_contains_ascii_seed_key(player, key)) {
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        if (first_match == 0u) {
            first_match = player_id;
            first_slot_type = kbo_player_is_asian_quota_candidate(player)
                ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                : KBO_FOREIGN_INJURY_SLOT_REGULAR;
        }
        matches++;
    }

    HeapFree(GetProcessHeap(), 0, snapshot);
    if (first_match != 0u) {
        if (out_slot_type != NULL) {
            *out_slot_type = first_slot_type;
        }
        kbo_log_runtimef(
            "foreign replacement player seed resolved key=%s player=%u matches=%d slot=%s",
            key,
            first_match,
            matches,
            first_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular");
    } else {
        kbo_log_foreign_replacement_seed_unresolved(key, "memory_scan_no_match", player_count);
    }
    return first_match;
}

int kbo_add_foreign_replacement_player_seed_locked(const KboForeignReplacementPlayerSeed* seed)
{
    if (seed == NULL || (seed->key[0] == '\0' && seed->player_id == 0u)) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* existing = &g_kbo_foreign_replacement_player_seeds[i];
        if ((seed->player_id != 0u && existing->player_id == seed->player_id)
                || (seed->key[0] != '\0' && existing->key[0] != '\0' && _stricmp(existing->key, seed->key) == 0)) {
            if (existing->slot_type == 0u && seed->slot_type != 0u) {
                existing->slot_type = seed->slot_type;
            }
            return 0;
        }
    }
    if (g_kbo_foreign_replacement_player_seed_count >= KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX) {
        return 0;
    }
    g_kbo_foreign_replacement_player_seeds[g_kbo_foreign_replacement_player_seed_count++] = *seed;
    return 1;
}

int kbo_import_foreign_replacement_player_seed_file_locked(const char* path, const char* source)
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
        char fields[3][80];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 3);
        if (field_count <= 0 || fields[0][0] == '\0'
                || _stricmp(fields[0], "replacement_player_key") == 0
                || _stricmp(fields[0], "player_id") == 0) {
            continue;
        }

        KboForeignReplacementPlayerSeed seed;
        memset(&seed, 0, sizeof(seed));
        snprintf(seed.key, sizeof(seed.key), "%s", fields[0]);
        seed.slot_type = kbo_parse_foreign_replacement_seed_slot_type(field_count > 1 ? fields[1] : "");
        if (fields[0][0] >= '0' && fields[0][0] <= '9') {
            seed.player_id = (uint32_t)strtoul(fields[0], NULL, 10);
        }
        if (kbo_add_foreign_replacement_player_seed_locked(&seed)) {
            imported++;
        }
    }

    kbo_csv_reader_close(reader);
    if (imported > 0) {
        kbo_log_runtimef(
            "foreign replacement player seed import source=%s imported=%d path=%s",
            source != NULL ? source : "",
            imported,
            path);
    }
    return imported;
}

