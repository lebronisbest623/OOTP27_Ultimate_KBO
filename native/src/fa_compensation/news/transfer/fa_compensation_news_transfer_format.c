#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_string.h"
#include "../fa_compensation_news_transfer_internal.h"

void kbo_fa_compensation_format_salary_text(int32_t salary, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (salary <= 0) {
        snprintf(out, out_size, "-");
        return;
    }

    char raw[32] = {0};
    char formatted[48] = {0};
    snprintf(raw, sizeof(raw), "%d", salary);
    size_t raw_len = strlen(raw);
    size_t pos = 0u;
    for (size_t i = 0u; i < raw_len && pos + 1u < sizeof(formatted); i++) {
        if (i > 0u && ((raw_len - i) % 3u) == 0u && pos + 1u < sizeof(formatted)) {
            formatted[pos++] = ',';
        }
        formatted[pos++] = raw[i];
    }
    formatted[pos] = '\0';
    snprintf(out, out_size, "$%s", formatted);
}

static int kbo_fa_compensation_team_name_placeholder(const char* text)
{
    return text == NULL
        || text[0] == '\0'
        || _stricmp(text, "Team") == 0
        || _stricmp(text, "Unknown") == 0;
}

void kbo_fa_compensation_copy_team_history_name(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, city, sizeof(city));
        copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_NICKNAME_STRING_OFFSET, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));

        if (!kbo_fa_compensation_team_name_placeholder(full_name)
                && (strchr(full_name, ' ') != NULL || kbo_ootp_text_has_non_ascii(full_name))) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (_stricmp(city, "Doosan") == 0 && _stricmp(nickname, "DOO") == 0) {
            snprintf(out, out_size, "Doosan Bears");
            return;
        }
        if (_stricmp(city, "Lotte") == 0 && _stricmp(nickname, "LOT") == 0) {
            snprintf(out, out_size, "Lotte Giants");
            return;
        }
        if (_stricmp(city, "Samsung") == 0 && _stricmp(nickname, "SAM") == 0) {
            snprintf(out, out_size, "Samsung Lions");
            return;
        }
        if (_stricmp(city, "KIA") == 0 && _stricmp(nickname, "KIA") == 0) {
            snprintf(out, out_size, "KIA Tigers");
            return;
        }
        if (_stricmp(city, "SSG") == 0 && _stricmp(nickname, "SSG") == 0) {
            snprintf(out, out_size, "SSG Landers");
            return;
        }
        if (_stricmp(city, "Hanwha") == 0
                && (_stricmp(nickname, "HAN") == 0 || _stricmp(nickname, "HH") == 0)) {
            snprintf(out, out_size, "Hanwha Eagles");
            return;
        }
        if (_stricmp(city, "Kiwoom") == 0 && _stricmp(nickname, "KIW") == 0) {
            snprintf(out, out_size, "Kiwoom Heroes");
            return;
        }
        if (_stricmp(city, "NC") == 0 && _stricmp(nickname, "NC") == 0) {
            snprintf(out, out_size, "NC Dinos");
            return;
        }
        if (_stricmp(city, "KT") == 0 && _stricmp(nickname, "KT") == 0) {
            snprintf(out, out_size, "KT Wiz");
            return;
        }
        if (_stricmp(city, "LG") == 0 && _stricmp(nickname, "LG") == 0) {
            snprintf(out, out_size, "LG Twins");
            return;
        }
        if (!kbo_fa_compensation_team_name_placeholder(city)
                && !kbo_fa_compensation_team_name_placeholder(nickname)
                && _stricmp(city, nickname) != 0) {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        if (!kbo_fa_compensation_team_name_placeholder(full_name)) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (!kbo_fa_compensation_team_name_placeholder(nickname)) {
            snprintf(out, out_size, "%s", nickname);
            return;
        }
        if (!kbo_fa_compensation_team_name_placeholder(city)) {
            snprintf(out, out_size, "%s", city);
            return;
        }
    }

    snprintf(out, out_size, "Team #%u", team_id);
}

void kbo_fa_compensation_copy_team_link(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    char team_name[96] = {0};
    kbo_fa_compensation_copy_team_history_name(team_id, team_name, sizeof(team_name));
    snprintf(out, out_size, "<%s:team#%u>", team_name, team_id);
}
