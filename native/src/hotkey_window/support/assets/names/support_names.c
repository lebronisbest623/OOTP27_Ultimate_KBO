#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../custom_events/asian_games/player_eval/asian_games_player_eval.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/lookup/team_lookup.h"
#include "../../../../team/names/team_name_cache.h"
#include "../../../../team/names/team_string.h"
#include "support_names.h"

void kbo_hub_draw_text(HDC hdc, const char* text, RECT rect, COLORREF color, HFONT font, UINT format)
{
    if (text == NULL) {
        return;
    }

    HFONT old_font = NULL;
    if (font != NULL) {
        old_font = (HFONT)SelectObject(hdc, font);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wide_len > 0) {
        WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_len) > 0) {
                DrawTextW(hdc, wide, -1, &rect, format);
            }
            HeapFree(GetProcessHeap(), 0, wide);
        }
    }

    if (old_font != NULL) {
        SelectObject(hdc, old_font);
    }
}

static int kbo_hub_ascii_is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int kbo_hub_ascii_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

int kbo_hub_ascii_is_alnum(char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

void kbo_hub_copy_player_display_name(uint8_t* player, char* out, size_t out_size)
{
    kbo_copy_player_display_name(player, out, out_size);
}

static int kbo_hub_team_name_candidate_score(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return -1000;
    }

    size_t len = strlen(text);
    if (len < 2 || len > 64) {
        return -1000;
    }

    int alpha_count = 0;
    int space_count = 0;
    int lowercase_count = 0;
    int uppercase_count = 0;
    int non_ascii_count = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x20 || kbo_hub_ascii_is_digit((char)c)) {
            return -1000;
        }
        if (c >= 0x80u) {
            non_ascii_count++;
        } else if (kbo_hub_ascii_is_alpha((char)c)) {
            alpha_count++;
            if (c >= 'a' && c <= 'z') { lowercase_count++; } else { uppercase_count++; }
        } else if (c == ' ') {
            space_count++;
        } else if (!(c == '-' || c == '\'' || c == '.' || c == '&')) {
            return -1000;
        }
    }

    if (alpha_count < 2 && non_ascii_count == 0) {
        return -1000;
    }

    int score = (int)len;
    if (non_ascii_count > 0) { score += 60; }
    if (space_count > 0) { score += 80; }
    if (lowercase_count > 0) { score += 20; }
    if (space_count == 0 && len <= 5 && uppercase_count == alpha_count) { score -= 80; }
    if (ascii_equals_ignore_case(text, "SANG") || ascii_equals_ignore_case(text, "KPB")) { score -= 200; }

    return score;
}

static int kbo_hub_team_name_contains(const char* name, const char* marker)
{
    if (name == NULL || marker == NULL || marker[0] == '\0') {
        return 0;
    }
    return kbo_ascii_contains_ignore_case(name, marker) || strstr(name, marker) != NULL;
}

void kbo_hub_copy_team_display_name_from_ptr(uint8_t* team, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        snprintf(out, out_size, "%s", fallback != NULL && fallback[0] != '\0' ? fallback : "Unknown club");
        return;
    }

    static const uint32_t string_offsets[] = OOTP27_KBO_TEAM_SEARCH_STRING_OFFSETS;

    char city[64] = {0};
    char nickname[64] = {0};
    char best[96] = {0};
    int best_score = -1000;

    copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, city, sizeof(city));
    copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_NICKNAME_STRING_OFFSET, nickname, sizeof(nickname));

    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        char text[96] = {0};
        if (!copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))) {
            continue;
        }
        int score = kbo_hub_team_name_candidate_score(text);
        if (score > best_score) {
            best_score = score;
            snprintf(best, sizeof(best), "%s", text);
        }
    }

    if (best_score > 0 && strchr(best, ' ') != NULL) {
        snprintf(out, out_size, "%s", best);
        return;
    }

    if (kbo_hub_team_name_candidate_score(city) > 0
            && kbo_hub_team_name_candidate_score(nickname) > 0
            && !ascii_equals_ignore_case(city, nickname)) {
        snprintf(out, out_size, "%s %s", city, nickname);
        return;
    }

    if (best_score > 0) {
        snprintf(out, out_size, "%s", best);
        return;
    }

    snprintf(out, out_size, "%s", fallback != NULL && fallback[0] != '\0' ? fallback : "Unknown club");
}

void kbo_hub_copy_team_display_name_by_id(uint32_t team_id, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL) {
        kbo_hub_copy_team_display_name_from_ptr(team, out, out_size, fallback);
        return;
    }

    if (fallback != NULL && fallback[0] != '\0') {
        snprintf(out, out_size, "%s", fallback);
    } else if (team_id != 0) {
        snprintf(out, out_size, "Unknown club #%u", team_id);
    } else {
        snprintf(out, out_size, "Unknown club");
    }
}

void kbo_hub_copy_team_abbrev_by_id(uint32_t team_id, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint32_t parent_team_id = 0u;
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    }
    if (parent_team_id != 0u && parent_team_id != team_id) {
        char parent_abbrev[16] = {0};
        kbo_hub_copy_team_abbrev_by_id(parent_team_id, parent_abbrev, sizeof(parent_abbrev), fallback);
        if (parent_abbrev[0] != '\0' && strcmp(parent_abbrev, "-") != 0) {
            snprintf(out, out_size, "%s2", parent_abbrev);
            return;
        }
    }

    char name[96] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, name, sizeof(name), fallback);
    if (kbo_hub_team_name_contains(name, "Lotte")
            || kbo_hub_team_name_contains(name, "Giants")
            || kbo_hub_team_name_contains(name, "\xeb\xa1\xaf\xeb\x8d\xb0")
            || kbo_hub_team_name_contains(name, "\xec\x9e\x90\xec\x9d\xb4\xec\x96\xb8\xec\xb8\xa0")) {
        snprintf(out, out_size, "LOT");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Doosan")
            || kbo_hub_team_name_contains(name, "Bears")
            || kbo_hub_team_name_contains(name, "\xeb\x91\x90\xec\x82\xb0")
            || kbo_hub_team_name_contains(name, "\xeb\xb2\xa0\xec\x96\xb4\xec\x8a\xa4")) {
        snprintf(out, out_size, "DOO");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Samsung")
            || kbo_hub_team_name_contains(name, "Lions")
            || kbo_hub_team_name_contains(name, "\xec\x82\xbc\xec\x84\xb1")
            || kbo_hub_team_name_contains(name, "\xeb\x9d\xbc\xec\x9d\xb4\xec\x98\xa8\xec\xa6\x88")) {
        snprintf(out, out_size, "SAM");
        return;
    }
    if (kbo_hub_team_name_contains(name, "KIA")
            || kbo_hub_team_name_contains(name, "Tigers")
            || kbo_hub_team_name_contains(name, "\xea\xb8\xb0\xec\x95\x84")
            || kbo_hub_team_name_contains(name, "\xed\x83\x80\xec\x9d\xb4\xea\xb1\xb0\xec\xa6\x88")) {
        snprintf(out, out_size, "KIA");
        return;
    }
    if (kbo_hub_team_name_contains(name, "SSG")
            || kbo_hub_team_name_contains(name, "Landers")
            || kbo_hub_team_name_contains(name, "\xeb\x9e\x9c\xeb\x8d\x94\xec\x8a\xa4")) {
        snprintf(out, out_size, "SSG");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Hanwha")
            || kbo_hub_team_name_contains(name, "Eagles")
            || kbo_hub_team_name_contains(name, "\xed\x95\x9c\xed\x99\x94")
            || kbo_hub_team_name_contains(name, "\xec\x9d\xb4\xea\xb8\x80\xec\x8a\xa4")) {
        snprintf(out, out_size, "HAN");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Nexen")
            || kbo_hub_team_name_contains(name, "\xeb\x84\xa5\xec\x84\xbc")) {
        snprintf(out, out_size, "NEX");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Kiwoom")
            || kbo_hub_team_name_contains(name, "Heroes")
            || kbo_hub_team_name_contains(name, "\xed\x82\xa4\xec\x9b\x80")
            || kbo_hub_team_name_contains(name, "\xed\x9e\x88\xec\x96\xb4\xeb\xa1\x9c\xec\xa6\x88")) {
        snprintf(out, out_size, "KIW");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Dinos")
            || kbo_hub_team_name_contains(name, "NC")
            || kbo_hub_team_name_contains(name, "\xec\x97\x94\xec\x94\xa8")
            || kbo_hub_team_name_contains(name, "\xeb\x8b\xa4\xec\x9d\xb4\xeb\x85\xb8\xec\x8a\xa4")) {
        snprintf(out, out_size, "NC");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Wiz")
            || kbo_hub_team_name_contains(name, "KT")
            || kbo_hub_team_name_contains(name, "\xec\xbc\x80\xec\x9d\xb4\xed\x8b\xb0")
            || kbo_hub_team_name_contains(name, "\xec\x9c\x84\xec\xa6\x88")) {
        snprintf(out, out_size, "KT");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Twins")
            || kbo_hub_team_name_contains(name, "LG")
            || kbo_hub_team_name_contains(name, "\xec\x97\x98\xec\xa7\x80")
            || kbo_hub_team_name_contains(name, "\xed\x8a\xb8\xec\x9c\x88\xec\x8a\xa4")) {
        snprintf(out, out_size, "LG");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Sangmu")
            || kbo_hub_team_name_contains(name, "\xec\x83\x81\xeb\xac\xb4")) {
        snprintf(out, out_size, "SANG");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Police")
            || kbo_hub_team_name_contains(name, "\xea\xb2\xbd\xec\xb0\xb0")) {
        snprintf(out, out_size, "KPB");
        return;
    }
    if (kbo_hub_team_name_contains(name, "Ulsan")
            || kbo_hub_team_name_contains(name, "\xec\x9a\xb8\xec\x82\xb0")) {
        snprintf(out, out_size, "ULS");
        return;
    }

    if (fallback != NULL && fallback[0] != '\0' && strlen(fallback) <= 6u) {
        snprintf(out, out_size, "%s", fallback);
    } else if (team_id != 0u) {
        snprintf(out, out_size, "T%u", team_id);
    } else {
        snprintf(out, out_size, "-");
    }
}

