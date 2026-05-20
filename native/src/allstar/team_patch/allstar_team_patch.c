/* All-Star team classification and guarded team-id handling. */

#include "allstar_team_patch.h"

#include <stdio.h>
#include <windows.h>

#include "../allstar_league_context/allstar_league_context.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/names/team_string.h"

static int patch_kbo_allstar_team_name(
    uint8_t* team,
    const char* old_city,
    const char* old_full_name,
    const char* abbreviation,
    const char* city,
    const char* nickname,
    const char* full_name,
    const char* source)
{
    if (team == NULL || city == NULL || city[0] == '\0'
            || abbreviation == NULL || abbreviation[0] == '\0'
            || nickname == NULL || nickname[0] == '\0'
            || full_name == NULL || full_name[0] == '\0'
            || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    char old_name[96] = {0};
    copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, old_name, sizeof(old_name));

    int patched = 0;
    patched += assign_ootp_string_object_text_if_different(team, 0x10u, city);
    patched += assign_ootp_string_object_text_if_different(team, 0x28u, abbreviation);
    patched += assign_ootp_string_object_text_if_different(team, 0x40u, nickname);
    patched += assign_ootp_string_object_text_if_different(team, 0x70u, full_name);
    patched += assign_ootp_string_object_text_if_different(team, 0x100u, full_name);

    char duplicated_full_name[160] = {0};
    snprintf(duplicated_full_name, sizeof(duplicated_full_name), "%s %s", city, full_name);

    for (uint32_t offset = 0; offset <= 0x140u; offset += sizeof(uintptr_t)) {
        char text[128] = {0};
        if (!copy_ootp_string_object_text(team, offset, text, sizeof(text))) {
            continue;
        }
        if (old_city != NULL && old_city[0] != '\0' && ascii_equals_ignore_case(text, old_city)) {
            patched += assign_ootp_string_object_text(team, offset, city);
        } else if (old_full_name != NULL && old_full_name[0] != '\0' && ascii_equals_ignore_case(text, old_full_name)) {
            patched += assign_ootp_string_object_text(team, offset, full_name);
        } else if (duplicated_full_name[0] != '\0' && ascii_equals_ignore_case(text, duplicated_full_name)) {
            patched += assign_ootp_string_object_text(team, offset, full_name);
        }
    }

    if (patched > 0) {
        LONG log_index = InterlockedIncrement(&g_allstar_team_name_log_count);
        if (log_index <= 20) {
            kbo_log_runtimef(
                "patched KBO all-star team name source=%s team=%p team_id=%u old=%s new=%s slots=%d",
                source != NULL ? source : "",
                team,
                *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET),
                old_name[0] != '\0' ? old_name : "(blank)",
                full_name,
                patched);
        }
    }

    return patched > 0;
}

static int kbo_allstar_team_contains_any_marker(uint8_t* team, const char* const* markers, size_t count)
{
    for (size_t i = 0u; i < count; i++) {
        if (team_contains_ootp_string_text(team, markers[i])) {
            return 1;
        }
    }
    return 0;
}

static int kbo_allstar_exhibition_team_side(uint8_t* team, int* out_kind)
{
    if (out_kind != NULL) {
        *out_kind = 0;
    }
    if (team == NULL) {
        return 0;
    }

    static const char* const allstar_kind_markers[] = {
        "All-Star", "All Star", "Allstars", "All-Stars", "All Stars",
        "\xec\x98\xac\xec\x8a\xa4\xed\x83\x80",
        "\xec\x98\xac\x20\xec\x8a\xa4\xed\x83\x80"
    };
    static const char* const future_kind_markers[] = {
        "Future Star", "Future Stars", "Futures Star", "Futures Stars",
        "\xed\x93\xa8\xec\xb2\x98\xec\x8a\xa4",
        "\xed\x93\xa8\xec\xb2\x98"
    };
    static const char* const allstar_side1_markers[] = {
        "AS1", "Nanum", "Team 1", "Team 1 All-Stars",
        "\xeb\x82\x98\xeb\x88\x94",
        "\xed\x8c\x80\x20\x31",
        "\x31\xed\x8c\x80"
    };
    static const char* const allstar_side2_markers[] = {
        "AS2", "Dream", "Team 2", "Team 2 All-Stars",
        "\xeb\x93\x9c\xeb\xa6\xbc",
        "\xed\x8c\x80\x20\x32",
        "\x32\xed\x8c\x80"
    };
    static const char* const future_side1_markers[] = {
        "FS1", "North", "Team 1", "Team 1 Future Stars",
        "\xeb\xb6\x81\xeb\xb6\x80",
        "\xeb\xb6\x81",
        "\xec\x84\x9c\xea\xb5\xb0",
        "\xec\x84\x9c\xeb\xb6\x80",
        "\xed\x8c\x80\x20\x31",
        "\x31\xed\x8c\x80"
    };
    static const char* const future_side2_markers[] = {
        "FS2", "South", "Team 2", "Team 2 Future Stars",
        "\xeb\x82\xa8\xeb\xb6\x80",
        "\xeb\x82\xa8",
        "\xeb\x8f\x99\xea\xb5\xb0",
        "\xeb\x8f\x99\xeb\xb6\x80",
        "\xed\x8c\x80\x20\x32",
        "\x32\xed\x8c\x80"
    };

    int allstar_kind = 0;
    allstar_kind = kbo_allstar_team_contains_any_marker(
        team,
        allstar_kind_markers,
        sizeof(allstar_kind_markers) / sizeof(allstar_kind_markers[0]));
    int future_kind = 0;
    future_kind = kbo_allstar_team_contains_any_marker(
        team,
        future_kind_markers,
        sizeof(future_kind_markers) / sizeof(future_kind_markers[0]));

    if (allstar_kind) {
        if (kbo_allstar_team_contains_any_marker(
                team,
                allstar_side1_markers,
                sizeof(allstar_side1_markers) / sizeof(allstar_side1_markers[0]))) {
            if (out_kind != NULL) {
                *out_kind = 1;
            }
            return 1;
        }
        if (kbo_allstar_team_contains_any_marker(
                team,
                allstar_side2_markers,
                sizeof(allstar_side2_markers) / sizeof(allstar_side2_markers[0]))) {
            if (out_kind != NULL) {
                *out_kind = 1;
            }
            return 2;
        }
    }

    if (future_kind) {
        if (kbo_allstar_team_contains_any_marker(
                team,
                future_side1_markers,
                sizeof(future_side1_markers) / sizeof(future_side1_markers[0]))) {
            if (out_kind != NULL) {
                *out_kind = 2;
            }
            return 1;
        }
        if (kbo_allstar_team_contains_any_marker(
                team,
                future_side2_markers,
                sizeof(future_side2_markers) / sizeof(future_side2_markers[0]))) {
            if (out_kind != NULL) {
                *out_kind = 2;
            }
            return 2;
        }
    }

    if (team_contains_ootp_string_text(team, "AS1")) {
        if (out_kind != NULL) {
            *out_kind = 1;
        }
        return 1;
    }
    if (team_contains_ootp_string_text(team, "AS2")) {
        if (out_kind != NULL) {
            *out_kind = 1;
        }
        return 2;
    }
    if (team_contains_ootp_string_text(team, "FS1")) {
        if (out_kind != NULL) {
            *out_kind = 2;
        }
        return 1;
    }
    if (team_contains_ootp_string_text(team, "FS2")) {
        if (out_kind != NULL) {
            *out_kind = 2;
        }
        return 2;
    }

    if (team_contains_ootp_string_text(team, "\xeb\x82\x98\xeb\x88\x94")) {
        if (out_kind != NULL) {
            *out_kind = 1;
        }
        return 1;
    }
    if (team_contains_ootp_string_text(team, "\xeb\x93\x9c\xeb\xa6\xbc")) {
        if (out_kind != NULL) {
            *out_kind = 1;
        }
        return 2;
    }
    if (team_contains_ootp_string_text(team, "\xeb\xb6\x81\xeb\xb6\x80")
            || team_contains_ootp_string_text(team, "\xec\x84\x9c\xea\xb5\xb0")
            || team_contains_ootp_string_text(team, "\xec\x84\x9c\xeb\xb6\x80")) {
        if (out_kind != NULL) {
            *out_kind = 2;
        }
        return 1;
    }
    if (team_contains_ootp_string_text(team, "\xeb\x82\xa8\xeb\xb6\x80")
            || team_contains_ootp_string_text(team, "\xeb\x8f\x99\xea\xb5\xb0")
            || team_contains_ootp_string_text(team, "\xeb\x8f\x99\xeb\xb6\x80")) {
        if (out_kind != NULL) {
            *out_kind = 2;
        }
        return 2;
    }

    if (allstar_kind) {
        if (team_contains_ootp_string_text(team, "\xed\x8c\x80\x20\x31")
                || team_contains_ootp_string_text(team, "\x31\xed\x8c\x80")) {
            if (out_kind != NULL) {
                *out_kind = 1;
            }
            return 1;
        }
        if (team_contains_ootp_string_text(team, "\xed\x8c\x80\x20\x32")
                || team_contains_ootp_string_text(team, "\x32\xed\x8c\x80")) {
            if (out_kind != NULL) {
                *out_kind = 1;
            }
            return 2;
        }
    }

    return 0;
}

static int patch_kbo_allstar_team_pair_name(uint8_t* team, int kind, int side, const char* source)
{
    if (kind == 1 && side == 1) {
        return patch_kbo_allstar_team_name(team, "Team 1", "Team 1 All-Stars", "AS1", "Nanum", "All-Stars", "Nanum All-Stars", source);
    }
    if (kind == 1 && side == 2) {
        return patch_kbo_allstar_team_name(team, "Team 2", "Team 2 All-Stars", "AS2", "Dream", "All-Stars", "Dream All-Stars", source);
    }
    if (kind == 2 && side == 1) {
        return patch_kbo_allstar_team_name(team, "Team 1", "Team 1 Future Stars", "FS1", "North", "Future Stars", "North Future Stars", source);
    }
    if (kind == 2 && side == 2) {
        return patch_kbo_allstar_team_name(team, "Team 2", "Team 2 Future Stars", "FS2", "South", "Future Stars", "South Future Stars", source);
    }
    return 0;
}

static int patch_kbo_allstar_team_names_for_league_id_or_any(uint32_t league_id, const char* source)
{
    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    int patched = 0;
    int candidates = 0;
    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0
                || team[OOTP27_KBO_TEAM_ACTIVE_OFFSET] == 0) {
            continue;
        }
        if (league_id != 0u && *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        int kind = 0;
        int side = kbo_allstar_exhibition_team_side(team, &kind);
        if (side == 0) {
            continue;
        }

        candidates++;
        if (patch_kbo_allstar_team_pair_name(team, kind, side, source)) {
            patched++;
        }
    }

    if (candidates > 0 && patched == 0) {
        static volatile LONG already_named_log_count = 0;
        LONG log_index = InterlockedIncrement(&already_named_log_count);
        if (log_index <= 5) {
            kbo_log_runtimef(
                "KBO all-star team names already patched source=%s league_id=%u candidates=%d",
                source != NULL ? source : "",
                league_id,
                candidates);
        }
    }

    return patched;
}

int patch_kbo_allstar_team_names_for_league_id(uint32_t league_id, const char* source)
{
    if (league_id == 0u) {
        return 0;
    }
    return patch_kbo_allstar_team_names_for_league_id_or_any(league_id, source);
}

int patch_kbo_allstar_team_names_for_known_exhibition_teams(const char* source)
{
    return patch_kbo_allstar_team_names_for_league_id_or_any(0u, source);
}

int patch_kbo_allstar_team_names_for_configured_league(const char* source)
{
    uint32_t league_id = kbo_resolve_kbo_league_id();
    return patch_kbo_allstar_team_names_for_league_id(league_id, source);
}

uint8_t kbo_allstar_side_for_team(uint8_t* team, uint32_t league_year)
{
    return kbo_allstar_seed_side_for_team_strings(team, league_year);
}

int ensure_kbo_allstar_team_ids(uintptr_t league_ptr, const char* source)
{
    if (league_ptr == 0) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    uint32_t league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    if (league_id == 0u) {
        league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    }
    int patched_names = patch_kbo_allstar_team_names_for_league_id(league_id, source);

    static volatile LONG unsafe_team_id_write_logged = 0;
    if (InterlockedCompareExchange(&unsafe_team_id_write_logged, 1, 0) == 0) {
        kbo_log_runtimef(
            "KBO all-star team id direct write disabled source=%s league=%p offsets=0x%x/0x%x reason=latest_build_layout_overlaps_serialized_object",
            source != NULL ? source : "",
            (void*)league_ptr,
            layout.team_a_offset,
            layout.team_b_offset);
    }
    return patched_names > 0;
}
