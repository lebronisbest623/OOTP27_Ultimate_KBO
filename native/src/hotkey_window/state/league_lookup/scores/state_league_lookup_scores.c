#include "../internal/state_league_lookup_internal.h"
#include "../../../../core/dates/constants/kbo_date_constants.h"

int kbo_hub_text_ends_with_ignore_case(const char* text, const char* suffix)
{
    if (text == NULL || suffix == NULL) {
        return 0;
    }

    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    if (suffix_len == 0 || text_len < suffix_len) {
        return 0;
    }

    return ascii_equals_ignore_case(text + text_len - suffix_len, suffix);
}

int kbo_hub_league_name_likeness_score(const char* name)
{
    if (name == NULL) {
        return -100;
    }

    size_t len = strlen(name);
    if (len < 3u || len > 80u) {
        return -100;
    }

    int letters = 0;
    int digits = 0;
    int spaces = 0;
    int non_ascii = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            letters++;
        } else if (c >= '0' && c <= '9') {
            digits++;
        } else if (c == ' ') {
            spaces++;
        } else if (c >= 0x80u) {
            non_ascii = 1;
        } else if (c != '-' && c != '_' && c != '.' && c != '&' && c != '\'') {
            return -50;
        }
    }

    if ((letters < 3 && !non_ascii) || (letters > 0 && digits > letters / 2)) {
        return -50;
    }

    int score = 0;
    if (non_ascii) {
        score += 30;
    }
    if (spaces > 0) {
        score += 12;
    }
    if (len >= 12u) {
        score += 8;
    }
    if (kbo_ascii_contains_ignore_case(name, "League")
            || kbo_ascii_contains_ignore_case(name, "KBO")
            || kbo_ascii_contains_ignore_case(name, "Organization")
            || kbo_ascii_contains_ignore_case(name, "Association")
            || kbo_ascii_contains_ignore_case(name, "Federation")
            || kbo_ascii_contains_ignore_case(name, "Baseball")
            || kbo_ascii_contains_ignore_case(name, "Conference")
            || kbo_ascii_contains_ignore_case(name, "Division")
            || strstr(name, "\xeb\xa6\xac\xea\xb7\xb8") != NULL
            || strstr(name, "\xec\x95\xbc\xea\xb5\xac") != NULL
            || strstr(name, "\xec\x97\xb0\xeb\xa7\xb9") != NULL
            || strstr(name, "\xed\x98\x91\xed\x9a\x8c") != NULL
            || strstr(name, "\xed\x95\x9c\xea\xb5\xad") != NULL
            || strstr(name, "\xeb\x8c\x80\xed\x95\x9c\xeb\xaf\xbc\xea\xb5\xad") != NULL) {
        score += 30;
    }

    return score;
}

int kbo_hub_abbr_likeness_score(const char* abbr)
{
    if (abbr == NULL) {
        return 0;
    }

    size_t len = strlen(abbr);
    if (len < 2u || len > 12u) {
        return 0;
    }

    int strong = 1;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)abbr[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            strong = 0;
            break;
        }
    }

    return strong ? 15 : 8;
}

int kbo_hub_named_league_candidate_score(
    uintptr_t candidate,
    uint32_t league_id,
    char* out_name,
    size_t out_name_size,
    char* out_logo_file,
    size_t out_logo_file_size)
{
    if (out_name != NULL && out_name_size > 0) {
        out_name[0] = '\0';
    }
    if (out_logo_file != NULL && out_logo_file_size > 0) {
        out_logo_file[0] = '\0';
    }
    if (candidate == 0 || league_id == 0
            || !memory_range_readable((void*)candidate, KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN)) {
        return -1000;
    }

    uint32_t primary_id = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_ID_OFFSET);
    uint32_t alternate_id = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
    if (primary_id != league_id && alternate_id != league_id) {
        return -1000;
    }

    uint32_t year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    if (year < KBO_SEASON_YEAR_MIN || year > KBO_SIM_YEAR_MAX) {
        return -200;
    }

    char name[96] = {0};
    if (!copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET, name, sizeof(name))) {
        return -200;
    }

    int name_score = kbo_hub_league_name_likeness_score(name);
    if (name_score < 30) {
        return -200;
    }

    int score = 0;
    score += (primary_id == league_id) ? 30 : 20;
    score += 20;
    score += name_score;

    uint32_t subleague_count = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_SUBLEAGUE_COUNT_OFFSET);
    if (subleague_count <= 32u) {
        score += 5;
    }

    char abbr[32] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_ABBR_STRING_OFFSET, abbr, sizeof(abbr))) {
        score += kbo_hub_abbr_likeness_score(abbr);
    }

    char logo[128] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_LOGO_STRING_OFFSET, logo, sizeof(logo))
            && (kbo_hub_text_ends_with_ignore_case(logo, ".png")
                || kbo_hub_text_ends_with_ignore_case(logo, ".oi")
                || kbo_hub_text_ends_with_ignore_case(logo, ".jpg")
                || kbo_hub_text_ends_with_ignore_case(logo, ".jpeg"))) {
        score += 6;
        if (out_logo_file != NULL && out_logo_file_size > 0) {
            snprintf(out_logo_file, out_logo_file_size, "%s", logo);
        }
    }

    char stats_path[160] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_STATS_PATH_STRING_OFFSET, stats_path, sizeof(stats_path))
            && kbo_ascii_contains_ignore_case(stats_path, "\\data\\stats\\")) {
        score += 4;
    }

    char schedule_file[128] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_SCHEDULE_FILE_STRING_OFFSET, schedule_file, sizeof(schedule_file))
            && kbo_hub_text_ends_with_ignore_case(schedule_file, ".lsdl")) {
        score += 6;
    }

    if (out_name != NULL && out_name_size > 0) {
        snprintf(out_name, out_name_size, "%s", name);
    }
    return score;
}

