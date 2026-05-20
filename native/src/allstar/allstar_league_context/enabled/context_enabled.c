#include "../allstar_league_context.h"
#include "../../../core/dates/constants/kbo_date_constants.h"
#include "../../../core/league_roles/kbo_league_roles.h"

static int kbo_ascii_contains_ignore_case_local(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || *needle == '\0') {
        return 0;
    }

    for (const char* p = text; *p != '\0'; ++p) {
        const char* a = p;
        const char* b = needle;
        while (*a != '\0' && *b != '\0') {
            char ca = *a;
            char cb = *b;
            if (ca >= 'A' && ca <= 'Z') {
                ca = (char)(ca - 'A' + 'a');
            }
            if (cb >= 'A' && cb <= 'Z') {
                cb = (char)(cb - 'A' + 'a');
            }
            if (ca != cb) {
                break;
            }
            ++a;
            ++b;
        }
        if (*b == '\0') {
            return 1;
        }
    }
    return 0;
}

int kbo_allstar_league_uses_kbo_schedule_file(uintptr_t league_ptr)
{
    if (league_ptr == 0
            || !memory_range_readable(
                (void*)league_ptr,
                OOTP27_KBO_LEAGUE_SCHEDULE_FILE_STRING_OFFSET + sizeof(uintptr_t))) {
        return 0;
    }

    char schedule_file[160] = {0};
    if (!copy_ootp_string_object_text(
            (uint8_t*)league_ptr,
            OOTP27_KBO_LEAGUE_SCHEDULE_FILE_STRING_OFFSET,
            schedule_file,
            sizeof(schedule_file))) {
        return 0;
    }

    return kbo_ascii_contains_ignore_case_local(schedule_file, "korean_baseball_organization")
        && kbo_ascii_contains_ignore_case_local(schedule_file, ".lsdl");
}

int kbo_allstar_raw_kbo_league_context_enabled(uintptr_t league_ptr)
{
    if (!kbo_fix_enabled() || league_ptr == 0) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint32_t max_off = layout.game_flag_offset;
    max_off = max_off > layout.auto_schedule_offset ? max_off : layout.auto_schedule_offset;
    max_off = max_off > layout.rules_flag_offset ? max_off : layout.rules_flag_offset;
    max_off = max_off > layout.league_id_fallback_offset ? max_off : layout.league_id_fallback_offset;
    if (!memory_range_readable((void*)league_ptr, max_off + sizeof(uint32_t))) {
        return 0;
    }

    uint8_t* league = (uint8_t*)league_ptr;
    uint32_t year = memory_range_readable(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET)
        : 0u;
    if (year != 0u && (year < KBO_SEASON_YEAR_MIN || year > KBO_SIM_YEAR_MAX)) {
        return 0;
    }

    if (is_kbo_historical_league_context(league_ptr)
            || kbo_allstar_league_uses_kbo_schedule_file(league_ptr)
            || kbo_allstar_league_has_seeded_division_split(league_ptr)) {
        return 1;
    }

    uint32_t configured_league_id = kbo_resolve_kbo_league_id();
    if (configured_league_id == 0u) {
        configured_league_id = kbo_league_role_main_league_id();
    }
    uint32_t default_league_id = kbo_league_role_main_league_id();
    uint32_t legacy_id = memory_range_readable(league + OOTP27_KBO_LEAGUE_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(league + OOTP27_KBO_LEAGUE_ID_OFFSET)
        : 0u;
    uint32_t primary_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);

    return legacy_id == configured_league_id
        || primary_id == configured_league_id
        || fallback_id == configured_league_id
        || legacy_id == default_league_id
        || primary_id == default_league_id
        || fallback_id == default_league_id;
}

int kbo_allstar_league_context_enabled(uintptr_t league_ptr)
{
    if (!kbo_fix_enabled() || league_ptr == 0) {
        return 0;
    }

    if (is_kbo_historical_league_context(league_ptr)) {
        return 1;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.team_b_offset + sizeof(uint32_t))
            || !kbo_allstar_league_core_plausible(league_ptr)) {
        return 0;
    }

    uint32_t year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    if (year < KBO_SEASON_YEAR_MIN || year > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    uint32_t configured_league_id = kbo_resolve_kbo_league_id();
    uint32_t primary_league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    if (primary_league_id == configured_league_id || fallback_league_id == configured_league_id) {
        return 1;
    }
    uint32_t default_league_id = kbo_league_role_main_league_id();
    if (primary_league_id == default_league_id || fallback_league_id == default_league_id) {
        return 1;
    }
    if (kbo_allstar_league_has_seeded_division_split(league_ptr)) {
        return 1;
    }

    return 0;
}

int kbo_allstar_team_matches_league(uint8_t* team, uint32_t primary_league_id, uint32_t fallback_league_id)
{
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }

    return kbo_allstar_team_matches_league_ids(team, primary_league_id, fallback_league_id);
}
