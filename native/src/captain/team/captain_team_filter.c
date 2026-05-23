#include "../internal/captain_selection_internal.h"

static const char* const kbo_captain_exhibition_markers[] = {
    "All-Star", "All Star", "Allstars", "All-Stars", "All Stars",
    "Future Star", "Future Stars", "Futures Star", "Futures Stars",
    "AS1", "AS2", "FS1", "FS2",
    "\xec\x98\xac\xec\x8a\xa4\xed\x83\x80",
    "\xec\x98\xac\x20\xec\x8a\xa4\xed\x83\x80",
    "\xed\x93\xa8\xec\xb2\x98\xec\x8a\xa4",
    "\xed\x93\xa8\xec\xb2\x98"
};

int kbo_captain_team_is_exhibition(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    for (size_t i = 0u;
            i < sizeof(kbo_captain_exhibition_markers) / sizeof(kbo_captain_exhibition_markers[0]);
            i++) {
        if (team_contains_ootp_string_text(team, kbo_captain_exhibition_markers[i])) {
            return 1;
        }
    }
    return 0;
}

int kbo_captain_filter_regular_team_ids(
    uint32_t* team_ids,
    int team_count,
    int max_count,
    int* out_exhibition_count)
{
    if (out_exhibition_count != NULL) {
        *out_exhibition_count = 0;
    }
    if (team_ids == NULL || team_count <= 0 || max_count <= 0) {
        return 0;
    }

    int write_count = 0;
    int exhibition_count = 0;
    for (int read_index = 0; read_index < team_count; read_index++) {
        uint32_t team_id = team_ids[read_index];
        if (team_id == 0u) {
            continue;
        }
        if (kbo_captain_team_is_exhibition(team_id)) {
            exhibition_count++;
            continue;
        }
        if (write_count < max_count) {
            team_ids[write_count++] = team_id;
        }
    }

    if (out_exhibition_count != NULL) {
        *out_exhibition_count = exhibition_count;
    }
    return write_count;
}
