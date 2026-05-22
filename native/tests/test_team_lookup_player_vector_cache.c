#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/team/lookup/team_lookup.h"

static uint8_t g_global_database[0x600];
static uintptr_t g_player_vector[4];
static uint8_t g_players[4][OOTP27_PLAYER_SCAN_BYTES];

uintptr_t get_ootp_global_database(void)
{
    return (uintptr_t)g_global_database;
}

int memory_range_readable(const void* address, SIZE_T size)
{
    if (address == NULL || size == 0) {
        return 0;
    }

    uintptr_t begin = (uintptr_t)address;
    uintptr_t end = begin + (uintptr_t)size;
    if (end < begin) {
        return 0;
    }

    uintptr_t global_begin = (uintptr_t)g_global_database;
    uintptr_t global_end = global_begin + sizeof(g_global_database);
    uintptr_t vector_begin = (uintptr_t)g_player_vector;
    uintptr_t vector_end = vector_begin + sizeof(g_player_vector);
    uintptr_t players_begin = (uintptr_t)g_players;
    uintptr_t players_end = players_begin + sizeof(g_players);
    return (begin >= global_begin && end <= global_end)
        || (begin >= vector_begin && end <= vector_end)
        || (begin >= players_begin && end <= players_end);
}

int team_has_ootp_string_text(uint8_t* team, const char* expected)
{
    (void)team;
    (void)expected;
    return 0;
}

static void seed_player(int index, uint32_t player_id, uint16_t age)
{
    uint8_t* player = g_players[index];
    memset(player, 0, OOTP27_PLAYER_SCAN_BYTES);
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = player_id;
    *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET) = age;
    g_player_vector[index] = (uintptr_t)player;
}

int main(void)
{
    memset(g_global_database, 0, sizeof(g_global_database));
    memset(g_player_vector, 0, sizeof(g_player_vector));
    seed_player(0, 1001u, 24u);
    seed_player(1, 1002u, 25u);
    seed_player(2, 1003u, 26u);

    uint32_t offset = 0x40u;
    *(uintptr_t*)(g_global_database + offset) = (uintptr_t)g_player_vector;
    *(int32_t*)(g_global_database + offset + OOTP27_GLOBAL_VECTOR_COUNT_DELTA) = 2;

    uintptr_t vector = 0;
    int32_t count = 0;
    uint32_t found_offset = 0;
    if (!find_kbo_global_player_vector(&vector, &count, &found_offset)
            || vector != (uintptr_t)g_player_vector
            || count != 2
            || found_offset != offset) {
        fprintf(stderr, "initial lookup failed vector=%p count=%d offset=0x%x\n", (void*)vector, count, found_offset);
        return 1;
    }

    *(int32_t*)(g_global_database + offset + OOTP27_GLOBAL_VECTOR_COUNT_DELTA) = 3;
    vector = 0;
    count = 0;
    found_offset = 0;
    if (!find_kbo_global_player_vector(&vector, &count, &found_offset)
            || vector != (uintptr_t)g_player_vector
            || count != 3
            || found_offset != offset) {
        fprintf(stderr, "cached lookup kept stale count vector=%p count=%d offset=0x%x\n", (void*)vector, count, found_offset);
        return 1;
    }

    return 0;
}
