#include "allstar_csv_parse.h"

#include <stdlib.h>
#include <string.h>

#include "../../core/dates/constants/kbo_date_constants.h"
#include "../../core/dates/core_text_date.h"

static void kbo_allstar_trim_cell(char* value)
{
    if (value == NULL) {
        return;
    }

    char* start = value;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }
    if (start != value) {
        memmove(value, start, strlen(start) + 1);
    }

    size_t len = strlen(value);
    while (len > 0) {
        char c = value[len - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        value[--len] = '\0';
    }
}

static void kbo_allstar_copy_text(char* out, size_t out_size, const char* value)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }

    size_t len = strlen(value);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, value, len);
    out[len] = '\0';
}

static int kbo_is_allstar_team_header_name(const char* name)
{
    return ascii_equals_ignore_case(name, "allstar_team")
        || ascii_equals_ignore_case(name, "allstarTeam")
        || ascii_equals_ignore_case(name, "all_star_team")
        || ascii_equals_ignore_case(name, "allstar_side")
        || ascii_equals_ignore_case(name, "all_star_side")
        || ascii_equals_ignore_case(name, "allstar_division")
        || ascii_equals_ignore_case(name, "all_star_division")
        || ascii_equals_ignore_case(name, "kbo_allstar_team");
}

uint8_t kbo_parse_allstar_side(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    if (ascii_equals_ignore_case(text, "1")
            || ascii_equals_ignore_case(text, "nanum")
            || ascii_equals_ignore_case(text, "na")
            || ascii_equals_ignore_case(text, "north")
            || ascii_equals_ignore_case(text, "northern")
            || ascii_equals_ignore_case(text, "n")
            || ascii_equals_ignore_case(text, "buk")
            || ascii_equals_ignore_case(text, "bukbu")
            || ascii_equals_ignore_case(text, "west")
            || ascii_equals_ignore_case(text, "western")
            || ascii_equals_ignore_case(text, "w")
            || ascii_equals_ignore_case(text, "seo")
            || ascii_equals_ignore_case(text, "seogun")
            || ascii_equals_ignore_case(text, "seo-gun")) {
        return 1;
    }
    if (ascii_equals_ignore_case(text, "2")
            || ascii_equals_ignore_case(text, "dream")
            || ascii_equals_ignore_case(text, "dr")
            || ascii_equals_ignore_case(text, "south")
            || ascii_equals_ignore_case(text, "southern")
            || ascii_equals_ignore_case(text, "s")
            || ascii_equals_ignore_case(text, "nam")
            || ascii_equals_ignore_case(text, "nambu")
            || ascii_equals_ignore_case(text, "east")
            || ascii_equals_ignore_case(text, "eastern")
            || ascii_equals_ignore_case(text, "e")
            || ascii_equals_ignore_case(text, "dong")
            || ascii_equals_ignore_case(text, "donggun")
            || ascii_equals_ignore_case(text, "dong-gun")) {
        return 2;
    }
    return 0;
}

static void kbo_derive_current_city_from_team_name(const char* name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (name == NULL || name[0] == '\0') {
        return;
    }

    size_t len = 0;
    while (name[len] != '\0' && name[len] != ' ' && name[len] != '\t' && len + 1 < out_size) {
        out[len] = name[len];
        len++;
    }
    out[len] = '\0';
    if ((unsigned char)out[0] == 0xEF
            && (unsigned char)out[1] == 0xBB
            && (unsigned char)out[2] == 0xBF) {
        memmove(out, out + 3, strlen(out + 3) + 1);
    }
    kbo_allstar_trim_cell(out);
}

void kbo_csv_find_allstar_team_columns_from_fields(
    char fields[][KBO_ALLSTAR_CSV_FIELD_SIZE],
    int field_count,
    int* year_col,
    int* team_id_col,
    int* name_col,
    int* allstar_col)
{
    *year_col = -1;
    *team_id_col = -1;
    *name_col = -1;
    *allstar_col = -1;

    if (fields == NULL || field_count <= 0) {
        return;
    }

    for (int col = 0; col < field_count && col < KBO_ALLSTAR_CSV_MAX_COLUMNS; col++) {
        const char* cell = fields[col];
        if (*year_col < 0 && (
                ascii_equals_ignore_case(cell, "yearID")
                || ascii_equals_ignore_case(cell, "year")
                || ascii_equals_ignore_case(cell, "year_id"))) {
            *year_col = col;
        }
        if (*team_id_col < 0 && (
                ascii_equals_ignore_case(cell, "teamID")
                || ascii_equals_ignore_case(cell, "team_id"))) {
            *team_id_col = col;
        }
        if (*name_col < 0 && ascii_equals_ignore_case(cell, "name")) {
            *name_col = col;
        }
        if (*allstar_col < 0 && kbo_is_allstar_team_header_name(cell)) {
            *allstar_col = col;
        }
    }
}

void kbo_csv_extract_allstar_team_fields_from_fields(
    char fields[][KBO_ALLSTAR_CSV_FIELD_SIZE],
    int field_count,
    int year_col,
    int team_id_col,
    int name_col,
    int allstar_col,
    uint16_t* year,
    char* team_id,
    size_t team_id_size,
    char* team_name,
    size_t team_name_size,
    char* current_city,
    size_t current_city_size,
    uint8_t* side)
{
    char name[128] = {0};
    *year = 0;
    *side = 0;
    if (team_id != NULL && team_id_size > 0) {
        team_id[0] = '\0';
    }
    if (team_name != NULL && team_name_size > 0) {
        team_name[0] = '\0';
    }
    if (current_city != NULL && current_city_size > 0) {
        current_city[0] = '\0';
    }
    if (fields == NULL || field_count <= 0) {
        return;
    }

    int max_col = year_col;
    if (team_id_col > max_col) {
        max_col = team_id_col;
    }
    if (name_col > max_col) {
        max_col = name_col;
    }
    if (allstar_col > max_col) {
        max_col = allstar_col;
    }

    for (int col = 0; col <= max_col && col < field_count; col++) {
        const char* cell = fields[col];
        if (col == year_col) {
            int parsed_year = atoi(cell);
            if (parsed_year >= (int)KBO_HISTORY_YEAR_MIN && parsed_year <= (int)KBO_SIM_YEAR_MAX) {
                *year = (uint16_t)parsed_year;
            }
        } else if (col == team_id_col && team_id != NULL && team_id_size > 0) {
            kbo_allstar_copy_text(team_id, team_id_size, cell);
        } else if (col == name_col) {
            kbo_allstar_copy_text(name, sizeof(name), cell);
            if (team_name != NULL && team_name_size > 0) {
                kbo_allstar_copy_text(team_name, team_name_size, cell);
            }
        } else if (col == allstar_col) {
            *side = kbo_parse_allstar_side(cell);
        }
    }

    if (current_city != NULL && current_city_size > 0) {
        kbo_derive_current_city_from_team_name(name, current_city, current_city_size);
    }
}
