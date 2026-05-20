#include "../allstar_league_context.h"
#include "../../../core/dates/constants/kbo_date_constants.h"
#include "../../../runtime_memory/runtime_memory.h"

uint8_t kbo_allstar_seed_side_for_team_strings(uint8_t* team, uint32_t league_year)
{
    load_allstar_team_rules_once();
    if (team == NULL || g_allstar_team_row_count <= 0) {
        return 0;
    }

    char current_city[64] = {0};
    copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, current_city, sizeof(current_city));

    uint16_t target_year = 0;
    if (league_year >= KBO_HISTORY_YEAR_MIN && league_year <= KBO_SIM_YEAR_MAX) {
        target_year = (uint16_t)league_year;
    }

    for (int pass = 0; pass < 2; pass++) {
        uint16_t year_to_check = target_year;
        if (pass == 1) {
            year_to_check = 0;
            for (int i = 0; i < g_allstar_team_row_count; i++) {
                uint16_t row_year = g_allstar_team_rows[i].year;
                if (row_year <= target_year && row_year > year_to_check) {
                    year_to_check = row_year;
                }
            }
        }

        if (year_to_check == 0) {
            continue;
        }

        for (int i = 0; i < g_allstar_team_row_count; i++) {
            KboAllstarTeamRow* row = &g_allstar_team_rows[i];
            if (row->year != year_to_check) {
                continue;
            }
            if (team_has_ootp_string_text(team, row->team_id)) {
                return row->side;
            }
            if (row->team_name[0] != '\0' && team_has_ootp_string_text(team, row->team_name)) {
                return row->side;
            }
        }

        for (int i = 0; i < g_allstar_team_row_count; i++) {
            KboAllstarTeamRow* row = &g_allstar_team_rows[i];
            if (row->year != year_to_check || row->current_city[0] == '\0') {
                continue;
            }
            if (current_city[0] != '\0' && ascii_equals_ignore_case(current_city, row->current_city)) {
                return row->side;
            }
        }
    }

    return 0;
}

int kbo_allstar_team_matches_league_ids(uint8_t* team, uint32_t primary_league_id, uint32_t fallback_league_id)
{
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (primary_league_id != 0u && team_league_id == primary_league_id) {
        return 1;
    }
    return fallback_league_id != 0u && fallback_league_id != primary_league_id && team_league_id == fallback_league_id;
}

int kbo_allstar_league_has_seeded_division_split(uintptr_t league_ptr)
{
    if (league_ptr == 0 || !kbo_allstar_league_core_plausible(league_ptr)) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.subleague_count_offset + sizeof(uint32_t))) {
        return 0;
    }

    if (*(uint32_t*)(league + layout.subleague_count_offset) != 1u) {
        return 0;
    }

    uintptr_t subleague_array = *(uintptr_t*)(league + layout.subleague_array_offset);
    if (subleague_array == 0 || !memory_range_readable((void*)subleague_array, sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t subleague = *(uintptr_t*)subleague_array;
    if (subleague == 0 || !memory_range_readable((void*)subleague, OOTP27_SUBLEAGUE_READABLE_BYTES)) {
        return 0;
    }

    uint32_t division_count = *(uint32_t*)(subleague + OOTP27_SUBLEAGUE_DIVISION_COUNT_OFFSET);
    if (division_count != 1u) {
        return 0;
    }

    uintptr_t division_array = *(uintptr_t*)(subleague + OOTP27_SUBLEAGUE_DIVISION_ARRAY_OFFSET);
    if (division_array == 0 || !memory_range_readable((void*)division_array, sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t division = *(uintptr_t*)division_array;
    if (division == 0 || !memory_range_readable((void*)division, OOTP27_DIVISION_READABLE_BYTES)) {
        return 0;
    }

    uintptr_t team_ids_begin = *(uintptr_t*)(division + OOTP27_DIVISION_TEAM_IDS_BEGIN_OFFSET);
    uintptr_t team_ids_end = *(uintptr_t*)(division + OOTP27_DIVISION_TEAM_IDS_END_OFFSET);
    if (team_ids_begin == 0 || team_ids_end <= team_ids_begin || ((team_ids_end - team_ids_begin) % sizeof(uint32_t)) != 0) {
        return 0;
    }

    size_t team_id_count = (size_t)((team_ids_end - team_ids_begin) / sizeof(uint32_t));
    if (team_id_count < 2 || team_id_count > 128 || !memory_range_readable((void*)team_ids_begin, (SIZE_T)(team_id_count * sizeof(uint32_t)))) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > KBO_RUNTIME_MAX_TEAM_VECTOR_COUNT
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    uint32_t league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    uint32_t league_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    int side_one = 0;
    int side_two = 0;

    for (size_t i = 0; i < team_id_count; i++) {
        uint32_t team_id = *(uint32_t*)(team_ids_begin + (i * sizeof(uint32_t)));
        if (team_id == 0 || team_id > (uint32_t)team_count) {
            continue;
        }

        uintptr_t team_ptr = *(uintptr_t*)(team_vector + (((uintptr_t)team_id - 1u) * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0 || team[OOTP27_KBO_TEAM_DELETED_OFFSET + 1u] != 0) {
            continue;
        }
        if (!kbo_allstar_team_matches_league_ids(team, league_id, fallback_league_id)) {
            continue;
        }

        uint8_t side = kbo_allstar_seed_side_for_team_strings(team, league_year);
        if (side == 1) {
            side_one++;
        } else if (side == 2) {
            side_two++;
        }
    }

    return side_one > 0 && side_two > 0;
}
