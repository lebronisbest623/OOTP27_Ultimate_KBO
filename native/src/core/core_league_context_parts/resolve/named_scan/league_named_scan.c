#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "league_named_scan.h"

#include <stdio.h>
#include <string.h>

#include "../../api/league_context_lookup.h"
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../dates/core_text_date.h"
#include "../../../logging/core_log.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/names/team_string.h"
#include "../../../dates/constants/kbo_date_constants.h"

#define KBO_CORE_NAMED_LEAGUE_SCAN_EARLY_SCORE 115
#define KBO_CORE_NAMED_LEAGUE_SCAN_OBJECT_SPAN (OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)

static int kbo_core_ascii_contains_ignore_case(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    size_t needle_len = strlen(needle);
    for (const char* p = text; *p != '\0'; p++) {
        size_t i = 0u;
        for (; i < needle_len; i++) {
            char a = p[i];
            char b = needle[i];
            if (a == '\0') {
                return 0;
            }
            if (a >= 'A' && a <= 'Z') { a = (char)(a - 'A' + 'a'); }
            if (b >= 'A' && b <= 'Z') { b = (char)(b - 'A' + 'a'); }
            if (a != b) {
                break;
            }
        }
        if (i == needle_len) {
            return 1;
        }
    }
    return 0;
}

static int kbo_core_text_ends_with_ignore_case(const char* text, const char* suffix)
{
    if (text == NULL || suffix == NULL) {
        return 0;
    }

    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    return suffix_len > 0u
        && text_len >= suffix_len
        && ascii_equals_ignore_case(text + text_len - suffix_len, suffix);
}

static int kbo_core_league_name_likeness_score(const char* name)
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
    for (size_t i = 0u; i < len; i++) {
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
    if (kbo_core_ascii_contains_ignore_case(name, "League")
            || kbo_core_ascii_contains_ignore_case(name, "KBO")
            || kbo_core_ascii_contains_ignore_case(name, "Organization")
            || kbo_core_ascii_contains_ignore_case(name, "Association")
            || kbo_core_ascii_contains_ignore_case(name, "Federation")
            || kbo_core_ascii_contains_ignore_case(name, "Baseball")
            || kbo_core_ascii_contains_ignore_case(name, "Conference")
            || kbo_core_ascii_contains_ignore_case(name, "Division")
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

static int kbo_core_league_abbr_likeness_score(const char* abbr)
{
    if (abbr == NULL) {
        return 0;
    }

    size_t len = strlen(abbr);
    if (len < 2u || len > 12u) {
        return 0;
    }

    int strong = 1;
    for (size_t i = 0u; i < len; i++) {
        unsigned char c = (unsigned char)abbr[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            strong = 0;
            break;
        }
    }

    return strong ? 15 : 8;
}

int kbo_core_named_league_candidate_score(
    uintptr_t candidate,
    uint32_t league_id,
    char* out_name,
    size_t out_name_size)
{
    if (out_name != NULL && out_name_size > 0u) {
        out_name[0] = '\0';
    }
    if (candidate == 0u
            || league_id == 0u
            || !memory_range_readable((void*)candidate, KBO_CORE_NAMED_LEAGUE_SCAN_OBJECT_SPAN)) {
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

    int name_score = kbo_core_league_name_likeness_score(name);
    if (name_score < 30) {
        return -200;
    }

    int score = primary_id == league_id ? 30 : 20;
    score += 20;
    score += name_score;

    uint32_t subleague_count = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_SUBLEAGUE_COUNT_OFFSET);
    if (subleague_count <= 32u) {
        score += 5;
    }

    char abbr[32] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_ABBR_STRING_OFFSET, abbr, sizeof(abbr))) {
        score += kbo_core_league_abbr_likeness_score(abbr);
    }

    char logo[128] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_LOGO_STRING_OFFSET, logo, sizeof(logo))
            && (kbo_core_text_ends_with_ignore_case(logo, ".png")
                || kbo_core_text_ends_with_ignore_case(logo, ".oi")
                || kbo_core_text_ends_with_ignore_case(logo, ".jpg")
                || kbo_core_text_ends_with_ignore_case(logo, ".jpeg"))) {
        score += 6;
    }

    char stats_path[160] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_STATS_PATH_STRING_OFFSET, stats_path, sizeof(stats_path))
            && kbo_core_ascii_contains_ignore_case(stats_path, "\\data\\stats\\")) {
        score += 4;
    }

    char schedule_file[128] = {0};
    if (copy_ootp_string_object_text((uint8_t*)candidate, OOTP27_KBO_LEAGUE_SCHEDULE_FILE_STRING_OFFSET, schedule_file, sizeof(schedule_file))
            && kbo_core_text_ends_with_ignore_case(schedule_file, ".lsdl")) {
        score += 6;
    }

    if (out_name != NULL && out_name_size > 0u) {
        snprintf(out_name, out_name_size, "%s", name);
    }
    return score;
}

static uintptr_t kbo_find_named_league_ptr_by_memory_scan(
    uint32_t league_id,
    SIZE_T max_region_size,
    int* out_score,
    char* out_name,
    size_t out_name_size)
{
    uintptr_t best_ptr = 0u;
    int best_score = -1000;
    char best_name[96] = {0};

    uintptr_t address = 0x10000u;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)address, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= base) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && mbi.Type == MEM_PRIVATE
                && kbo_league_scan_protect_allows_read(mbi.Protect)
                && mbi.RegionSize >= (SIZE_T)KBO_CORE_NAMED_LEAGUE_SCAN_OBJECT_SPAN
                && mbi.RegionSize <= max_region_size) {
            uintptr_t scan_end = end >= sizeof(uint32_t) ? end - sizeof(uint32_t) : base;
            for (uintptr_t p = base; p <= scan_end; p += sizeof(uint32_t)) {
                if (*(uint32_t*)p != league_id) {
                    continue;
                }

                static const uint32_t id_offsets[] = {
                    OOTP27_KBO_LEAGUE_ID_OFFSET,
                    OOTP27_KBO_LEAGUE_ID_OFFSET + 8u
                };
                for (size_t i = 0u; i < sizeof(id_offsets) / sizeof(id_offsets[0]); i++) {
                    uint32_t id_offset = id_offsets[i];
                    if (p < base + id_offset) {
                        continue;
                    }

                    uintptr_t candidate = p - id_offset;
                    if ((candidate & 7u) != 0u
                            || candidate < base
                            || candidate + KBO_CORE_NAMED_LEAGUE_SCAN_OBJECT_SPAN > end) {
                        continue;
                    }

                    char candidate_name[96] = {0};
                    int score = kbo_core_named_league_candidate_score(
                        candidate,
                        league_id,
                        candidate_name,
                        sizeof(candidate_name));
                    if (score > best_score) {
                        best_score = score;
                        best_ptr = candidate;
                        snprintf(best_name, sizeof(best_name), "%s", candidate_name);
                        if (score >= KBO_CORE_NAMED_LEAGUE_SCAN_EARLY_SCORE) {
                            if (out_score != NULL) {
                                *out_score = best_score;
                            }
                            if (out_name != NULL && out_name_size > 0u) {
                                snprintf(out_name, out_name_size, "%s", best_name);
                            }
                            return best_ptr;
                        }
                    }
                }
            }
        }

        address = end;
#if UINTPTR_MAX > 0xffffffffu
        if (address >= (uintptr_t)0x0000800000000000ull) {
            break;
        }
#endif
    }

    if (out_score != NULL) {
        *out_score = best_score;
    }
    if (out_name != NULL && out_name_size > 0u) {
        snprintf(out_name, out_name_size, "%s", best_name);
    }
    return best_ptr;
}

uintptr_t kbo_find_named_league_ptr_by_memory_scan_all(uint32_t league_id)
{
    int score = -1000;
    char name[96] = {0};
    uintptr_t ptr = kbo_find_named_league_ptr_by_memory_scan(
        league_id,
        (SIZE_T)0x00040000u,
        &score,
        name,
        sizeof(name));
    if (score < KBO_CORE_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        ptr = kbo_find_named_league_ptr_by_memory_scan(
            league_id,
            (SIZE_T)0x00400000u,
            &score,
            name,
            sizeof(name));
    }
    if (ptr != 0u && score >= KBO_CORE_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        kbo_log_runtimef(
            "KBO named league ptr found by memory scan league_id=%u ptr=%p score=%d name=%s league_year=%u",
            league_id,
            (void*)ptr,
            score,
            name,
            *(uint32_t*)(ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET));
        return ptr;
    }

    kbo_log_runtimef(
        "KBO named league ptr memory scan missed league_id=%u best_score=%d name=%s",
        league_id,
        score,
        name[0] != '\0' ? name : "(none)");
    return 0u;
}
