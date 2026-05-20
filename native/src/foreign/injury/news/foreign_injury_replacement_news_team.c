#include "foreign_injury_replacement_news_internal.h"

#include "../../../team/names/team_string.h"

int kbo_foreign_injury_news_uses_korean(void)
{
    const char* language_dir = kbo_custom_news_language_dir();
    return language_dir == NULL || strcmp(language_dir, "en") != 0;
}

static int kbo_foreign_injury_ascii_equal_nocase(const char* a, const char* b, size_t len)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    for (size_t i = 0u; i < len; i++) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') { ca = (char)(ca - 'A' + 'a'); }
        if (cb >= 'A' && cb <= 'Z') { cb = (char)(cb - 'A' + 'a'); }
        if (ca != cb) {
            return 0;
        }
    }
    return 1;
}

static int kbo_foreign_injury_should_strip_team_suffix(
    const char* text,
    size_t first_len,
    const char* suffix,
    size_t suffix_len)
{
    if (text == NULL || suffix == NULL || first_len == 0u || suffix_len < 2u || suffix_len > 4u) {
        return 0;
    }
    if (first_len == suffix_len && kbo_foreign_injury_ascii_equal_nocase(text, suffix, suffix_len)) {
        return 1;
    }
    if (first_len >= suffix_len && kbo_foreign_injury_ascii_equal_nocase(text, suffix, suffix_len)) {
        return 1;
    }

    char first = text[0];
    if (first >= 'A' && first <= 'Z') { first = (char)(first - 'A' + 'a'); }
    for (size_t i = 0u; i < suffix_len; i++) {
        char c = suffix[i];
        if (c >= 'A' && c <= 'Z') { c = (char)(c - 'A' + 'a'); }
        if (c != first) {
            return 0;
        }
    }
    return 1;
}

static int kbo_foreign_injury_team_name_placeholder(const char* text)
{
    return text == NULL
        || text[0] == '\0'
        || strncmp(text, "Team #", 6u) == 0;
}

static void kbo_foreign_injury_copy_display_team_name(const char* raw, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (kbo_foreign_injury_team_name_placeholder(raw)) {
        return;
    }

    size_t len = strlen(raw);
    while (len > 0u && (raw[len - 1u] == ' ' || raw[len - 1u] == '\t')) {
        len--;
    }

    size_t split = len;
    while (split > 0u && raw[split - 1u] != ' ' && raw[split - 1u] != '\t') {
        split--;
    }
    if (split > 0u) {
        size_t first_len = 0u;
        while (first_len < len && raw[first_len] != ' ' && raw[first_len] != '\t') {
            first_len++;
        }

        size_t suffix_start = split;
        while (suffix_start < len && (raw[suffix_start] == ' ' || raw[suffix_start] == '\t')) {
            suffix_start++;
        }
        if (suffix_start < len
                && kbo_foreign_injury_should_strip_team_suffix(raw, first_len, raw + suffix_start, len - suffix_start)) {
            len = split;
            while (len > 0u && (raw[len - 1u] == ' ' || raw[len - 1u] == '\t')) {
                len--;
            }
        }
    }

    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, raw, len);
    out[len] = '\0';
}

static void kbo_foreign_injury_copy_team_name(uint32_t team_id, char* out, size_t out_size)
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

        if (!kbo_foreign_injury_team_name_placeholder(full_name)
                && (strchr(full_name, ' ') != NULL || kbo_ootp_text_has_non_ascii(full_name))) {
            kbo_foreign_injury_copy_display_team_name(full_name, out, out_size);
            if (out[0] != '\0') { return; }
        }
        if (!kbo_foreign_injury_team_name_placeholder(city)
                && !kbo_foreign_injury_team_name_placeholder(nickname)
                && _stricmp(city, nickname) != 0) {
            char combined[128] = {0};
            snprintf(combined, sizeof(combined), "%s %s", city, nickname);
            kbo_foreign_injury_copy_display_team_name(combined, out, out_size);
            if (out[0] != '\0') { return; }
        }
        if (!kbo_foreign_injury_team_name_placeholder(full_name)) {
            kbo_foreign_injury_copy_display_team_name(full_name, out, out_size);
            if (out[0] != '\0') { return; }
        }
        if (!kbo_foreign_injury_team_name_placeholder(nickname)) {
            kbo_foreign_injury_copy_display_team_name(nickname, out, out_size);
            if (out[0] != '\0') { return; }
        }
        if (!kbo_foreign_injury_team_name_placeholder(city)) {
            kbo_foreign_injury_copy_display_team_name(city, out, out_size);
            if (out[0] != '\0') { return; }
        }
    }

    snprintf(
        out,
        out_size,
        "%s",
        kbo_foreign_injury_news_uses_korean()
            ? "\xed\x95\xb4\xeb\x8b\xb9 \xea\xb5\xac\xeb\x8b\xa8"
            : "the club");
}

void kbo_foreign_injury_copy_team_link(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    char team_name[96] = {0};
    kbo_foreign_injury_copy_team_name(team_id, team_name, sizeof(team_name));
    if (team_id != 0u) {
        snprintf(out, out_size, "<%s:team#%u>", team_name, team_id);
    } else {
        snprintf(out, out_size, "%s", team_name);
    }
}
