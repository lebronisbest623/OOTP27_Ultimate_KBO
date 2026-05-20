#include "cbt_service_time_probe_internal.h"

int kbo_cbt_probe_year_plausible(uint32_t value, uint32_t current_year)
{
    return value >= 1900u && value <= current_year + 1u;
}

int kbo_cbt_probe_service_year_plausible(uint32_t value, uint32_t current_year)
{
    uint32_t first_year = current_year > 40u ? current_year - 40u : 1900u;
    if (first_year < 1900u) {
        first_year = 1900u;
    }
    return value >= first_year && value <= current_year;
}

int kbo_cbt_probe_team_plausible(uint32_t value, uint32_t a, uint32_t b, uint32_t c)
{
    return value != 0u && (value == a || value == b || value == c);
}

int kbo_cbt_probe_team_in_set(uint32_t value, const uint32_t* team_ids, int team_count)
{
    if (value <= 1u || team_ids == NULL || team_count <= 0) {
        return 0;
    }
    for (int i = 0; i < team_count; i++) {
        if (team_ids[i] == value) {
            return 1;
        }
    }
    return 0;
}

void kbo_cbt_probe_append_row_hex(char* out, size_t out_size, uintptr_t row, uint32_t byte_count)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (row == 0 || byte_count == 0u || byte_count > 64u || !memory_range_readable((void*)row, byte_count)) {
        return;
    }

    size_t used = 0u;
    for (uint32_t i = 0; i < byte_count && used + 4u < out_size; i++) {
        int wrote = snprintf(out + used, out_size - used, "%s%02X", i == 0u ? "" : " ", *(uint8_t*)(row + i));
        if (wrote <= 0) {
            break;
        }
        used += (size_t)wrote;
    }
}

static void kbo_cbt_probe_dump_service_window(uint8_t* player, uint32_t player_id)
{
    if (player == NULL || !memory_range_readable(player + 0x820u, 0x80u)) {
        return;
    }

    for (uint32_t off = 0x820u; off <= 0x880u; off += 0x10u) {
        char hex[384] = {0};
        kbo_cbt_probe_append_row_hex(hex, sizeof(hex), (uintptr_t)(player + off), 64u);
        kbo_log_runtimef(
            "CBT_SERVICE_PROBE: service_window player=%u off=0x%x hex=%s",
            player_id,
            off,
            hex);
    }
}

static void kbo_cbt_probe_decode_service_words(
    uint8_t* player,
    uint32_t player_id,
    uint32_t current_team_id,
    uint32_t active_team_id,
    uint32_t original_team_id)
{
    if (player == NULL || !memory_range_readable(player + 0x840u, 0x50u)) {
        return;
    }

    kbo_log_runtimef(
        "CBT_SERVICE_PROBE: service_decode player=%u cur=%u active=%u original=%u w840=%u w842=%u w844=%u w846=%u w848=%u w84a=%u w84c=%u w84e=%u w850=%u w852=%u w854=%u w856=%u w858=%u w85a=%u w85c=%u w85e=%u w860=%u w862=%u w864=%u w866=%u w868=%u w86a=%u w86c=%u w86e=%u w870=%u w872=%u w874=%u w876=%u w878=%u w87a=%u w87c=%u w87e=%u",
        player_id,
        current_team_id,
        active_team_id,
        original_team_id,
        *(uint16_t*)(player + 0x840u),
        *(uint16_t*)(player + 0x842u),
        *(uint16_t*)(player + 0x844u),
        *(uint16_t*)(player + 0x846u),
        *(uint16_t*)(player + 0x848u),
        *(uint16_t*)(player + 0x84au),
        *(uint16_t*)(player + 0x84cu),
        *(uint16_t*)(player + 0x84eu),
        *(uint16_t*)(player + 0x850u),
        *(uint16_t*)(player + 0x852u),
        *(uint16_t*)(player + 0x854u),
        *(uint16_t*)(player + 0x856u),
        *(uint16_t*)(player + 0x858u),
        *(uint16_t*)(player + 0x85au),
        *(uint16_t*)(player + 0x85cu),
        *(uint16_t*)(player + 0x85eu),
        *(uint16_t*)(player + 0x860u),
        *(uint16_t*)(player + 0x862u),
        *(uint16_t*)(player + 0x864u),
        *(uint16_t*)(player + 0x866u),
        *(uint16_t*)(player + 0x868u),
        *(uint16_t*)(player + 0x86au),
        *(uint16_t*)(player + 0x86cu),
        *(uint16_t*)(player + 0x86eu),
        *(uint16_t*)(player + 0x870u),
        *(uint16_t*)(player + 0x872u),
        *(uint16_t*)(player + 0x874u),
        *(uint16_t*)(player + 0x876u),
        *(uint16_t*)(player + 0x878u),
        *(uint16_t*)(player + 0x87au),
        *(uint16_t*)(player + 0x87cu),
        *(uint16_t*)(player + 0x87eu));
}

static void kbo_cbt_probe_search_known_player_season_team(
    uint8_t* player,
    uint32_t player_id,
    uint32_t season,
    uint32_t team_id)
{
    if (player == NULL || season == 0u || team_id == 0u || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    int emitted = 0;
    for (uint32_t off = 0x80u; off + 96u <= OOTP27_PLAYER_SCAN_BYTES && emitted < 24; off += 2u) {
        uint32_t value = (uint32_t)*(uint16_t*)(player + off);
        if (value != season) {
            continue;
        }

        uint32_t lo = off > 96u ? off - 96u : 0u;
        uint32_t hi = off + 98u <= OOTP27_PLAYER_SCAN_BYTES ? off + 98u : OOTP27_PLAYER_SCAN_BYTES;
        for (uint32_t near_off = lo; near_off + sizeof(uint16_t) <= hi; near_off += 2u) {
            if (near_off == off) {
                continue;
            }
            uint32_t near_value = (uint32_t)*(uint16_t*)(player + near_off);
            if (near_value != team_id) {
                continue;
            }

            uint32_t dump_off = off > 32u ? off - 32u : 0u;
            char hex[384] = {0};
            kbo_cbt_probe_append_row_hex(hex, sizeof(hex), (uintptr_t)(player + dump_off), 96u);
            kbo_log_runtimef(
                "CBT_SERVICE_PROBE: known_season_team_hit player=%u season=%u season_off=0x%x team=%u team_off=0x%x dump_off=0x%x hex=%s",
                player_id,
                season,
                off,
                team_id,
                near_off,
                dump_off,
                hex);
            emitted++;
            break;
        }
    }
    if (emitted == 0) {
        kbo_log_runtimef(
            "CBT_SERVICE_PROBE: known_season_team_hit player=%u season=%u team=%u result=none",
            player_id,
            season,
            team_id);
    }
}

static void kbo_cbt_probe_player_vectors(
    uint8_t* player,
    uint32_t player_year,
    const uint32_t* team_ids,
    int team_count,
    int* emitted_vectors,
    int* emitted_inline)
{
    if (player == NULL || emitted_vectors == NULL || *emitted_vectors >= 200) {
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);

    kbo_cbt_probe_scan_player_inline_tables(
        player,
        player_id,
        player_year,
        team_ids,
        team_count,
        emitted_inline);
    if (*emitted_inline < 40 && player_id == 715u) {
        kbo_cbt_probe_dump_service_window(player, player_id);
    }
    if (player_id == 715u || (current_team_id != 0u && active_team_id != 0u && original_team_id != 0u)) {
        kbo_cbt_probe_decode_service_words(
            player,
            player_id,
            current_team_id,
            active_team_id,
            original_team_id);
    }
    if (player_id == 715u) {
        kbo_cbt_probe_search_known_player_season_team(player, player_id, 2024u, 6u);
    }

    for (uint32_t off = 0x80u; off + 24u <= OOTP27_PLAYER_SCAN_BYTES && *emitted_vectors < 200; off += 8u) {
        if (!memory_range_readable(player + off, 24u)) {
            continue;
        }

        uintptr_t begin = *(uintptr_t*)(player + off);
        uintptr_t end = *(uintptr_t*)(player + off + 8u);
        uintptr_t cap = *(uintptr_t*)(player + off + 16u);
        if (begin == 0 || end <= begin || cap < end) {
            continue;
        }
        uintptr_t bytes = end - begin;
        uintptr_t cap_bytes = cap - begin;
        if (bytes > 64u * 1024u || cap_bytes > 128u * 1024u || (bytes % 4u) != 0u) {
            continue;
        }
        if (!memory_range_readable((void*)begin, (SIZE_T)(bytes < 64u ? bytes : 64u))) {
            continue;
        }

        (*emitted_vectors)++;

        if (off == 0x7e0u || off == 0x810u || off == 0x818u || off == 0x820u) {
            kbo_cbt_probe_dump_candidate_rows(
                player,
                player_id,
                player_year,
                current_team_id,
                active_team_id,
                original_team_id,
                team_ids,
                team_count,
                off,
                begin,
                end);
        }
    }
}

void kbo_cbt_service_time_probe_once(void)
{
    static volatile LONG done = 0;
    if (InterlockedCompareExchange(&done, 1, 0) != 0) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t player_vector_offset = 0u;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &player_vector_offset)) {
        kbo_log_runtime_line("CBT_SERVICE_PROBE: skipped reason=no_player_vector");
        InterlockedExchange(&done, 0);
        return;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    uint32_t current_year = kbo_find_current_kbo_league_year();
    if (current_year == 0u) {
        current_year = KBO_SIM_YEAR_MAX;
    }

    kbo_log_runtimef(
        "CBT_SERVICE_PROBE: begin player_vector=%p count=%d vector_offset=0x%x league=%u year=%u",
        (void*)player_vector,
        player_count,
        player_vector_offset,
        kbo_league_id,
        current_year);

    uint32_t kbo_team_ids[64] = {0};
    int team_scanned = 0;
    int team_unreadable = 0;
    int kbo_team_count = collect_kbo_league_team_ids(
        kbo_league_id,
        kbo_team_ids,
        (int)(sizeof(kbo_team_ids) / sizeof(kbo_team_ids[0])),
        &team_scanned,
        &team_unreadable);
    kbo_log_runtimef(
        "CBT_SERVICE_PROBE: team_set league=%u count=%d scanned=%d unreadable=%d ids=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
        kbo_league_id,
        kbo_team_count,
        team_scanned,
        team_unreadable,
        kbo_team_count > 0 ? kbo_team_ids[0] : 0u,
        kbo_team_count > 1 ? kbo_team_ids[1] : 0u,
        kbo_team_count > 2 ? kbo_team_ids[2] : 0u,
        kbo_team_count > 3 ? kbo_team_ids[3] : 0u,
        kbo_team_count > 4 ? kbo_team_ids[4] : 0u,
        kbo_team_count > 5 ? kbo_team_ids[5] : 0u,
        kbo_team_count > 6 ? kbo_team_ids[6] : 0u,
        kbo_team_count > 7 ? kbo_team_ids[7] : 0u,
        kbo_team_count > 8 ? kbo_team_ids[8] : 0u,
        kbo_team_count > 9 ? kbo_team_ids[9] : 0u);

    int checked = 0;
    int emitted_vectors = 0;
    int emitted_inline = 0;
    for (int32_t i = 0; i < player_count && checked < 160 && emitted_vectors < 200; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (kbo_league_id != 0u && league_id != kbo_league_id) {
            continue;
        }
        if (current_team_id == 0u && active_team_id == 0u) {
            continue;
        }

        checked++;
        kbo_cbt_probe_player_vectors(
            player,
            current_year,
            kbo_team_ids,
            kbo_team_count,
            &emitted_vectors,
            &emitted_inline);
    }

    kbo_log_runtimef(
        "CBT_SERVICE_PROBE: done checked=%d emitted_vectors=%d emitted_inline=%d",
        checked,
        emitted_vectors,
        emitted_inline);
}
