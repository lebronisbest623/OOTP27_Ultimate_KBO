#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../military_service/calendar/military_service_date.h"
#include "../../../military_service/players/loans/military_active_loan.h"
#include "../../../military_service/players/state/military_player_state.h"
#include "../../../military_service/seed/parse/military_service_seed_parse.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_string.h"
#include "roster_import_extra_columns_consume.h"

#define KBO_ROSTER_IMPORT_ROW_ITEMS_OFFSET 0x08u
#define KBO_ROSTER_IMPORT_ROW_COUNT_OFFSET 0x14u
#define KBO_ROSTER_IMPORT_MAX_FIELD_COUNT 512
#define KBO_ROSTER_IMPORT_ORIGINAL_COLUMN_COUNT 157
#define KBO_ROSTER_IMPORT_BASE_EXTRA_COLUMN_COUNT 8
#define KBO_ROSTER_IMPORT_PRONENESS_EXTRA_COLUMN_COUNT 4
#define KBO_ROSTER_IMPORT_CHADWICK_EXTRA_COLUMN_COUNT 1
#define KBO_ROSTER_IMPORT_POPULARITY_EXTRA_COLUMN_COUNT 2
#define KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT \
    (KBO_ROSTER_IMPORT_BASE_EXTRA_COLUMN_COUNT + KBO_ROSTER_IMPORT_PRONENESS_EXTRA_COLUMN_COUNT)
#define KBO_ROSTER_IMPORT_CHADWICK_SET_EXTRA_COLUMN_COUNT \
    (KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT + KBO_ROSTER_IMPORT_CHADWICK_EXTRA_COLUMN_COUNT)
#define KBO_ROSTER_IMPORT_FULL_EXTRA_COLUMN_COUNT \
    (KBO_ROSTER_IMPORT_CHADWICK_SET_EXTRA_COLUMN_COUNT + KBO_ROSTER_IMPORT_POPULARITY_EXTRA_COLUMN_COUNT)

static int kbo_roster_import_ascii_equal_ci(char a, char b)
{
    if (a >= 'A' && a <= 'Z') { a = (char)(a - 'A' + 'a'); }
    if (b >= 'A' && b <= 'Z') { b = (char)(b - 'A' + 'a'); }
    return a == b;
}

static int kbo_roster_import_text_is_eol(const char* text)
{
    if (text == NULL) {
        return 0;
    }
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    return kbo_roster_import_ascii_equal_ci(text[0], 'e')
        && kbo_roster_import_ascii_equal_ci(text[1], 'o')
        && kbo_roster_import_ascii_equal_ci(text[2], 'l')
        && (text[3] == '\0' || text[3] == ',' || text[3] == '\r' || text[3] == '\n');
}

static int32_t kbo_roster_import_row_count(void* row)
{
    if (row == NULL || !memory_range_readable((uint8_t*)row + KBO_ROSTER_IMPORT_ROW_COUNT_OFFSET, sizeof(int32_t))) {
        return 0;
    }

    int32_t count = *(int32_t*)((uint8_t*)row + KBO_ROSTER_IMPORT_ROW_COUNT_OFFSET);
    if (count <= 0 || count > KBO_ROSTER_IMPORT_MAX_FIELD_COUNT) {
        return 0;
    }
    return count;
}

static void* kbo_roster_import_row_cell(void* row, int32_t index)
{
    int32_t count = kbo_roster_import_row_count(row);
    if (index < 0 || index >= count) {
        return NULL;
    }
    if (!memory_range_readable((uint8_t*)row + KBO_ROSTER_IMPORT_ROW_ITEMS_OFFSET, sizeof(uintptr_t))) {
        return NULL;
    }

    uintptr_t items = *(uintptr_t*)((uint8_t*)row + KBO_ROSTER_IMPORT_ROW_ITEMS_OFFSET);
    if (items == 0u || !memory_range_readable((void*)(items + ((uintptr_t)index * sizeof(uintptr_t))), sizeof(uintptr_t))) {
        return NULL;
    }

    return *(void**)(items + ((uintptr_t)index * sizeof(uintptr_t)));
}

static int kbo_roster_import_cell_text(void* row, int32_t index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    void* cell = kbo_roster_import_row_cell(row, index);
    if (cell == NULL) {
        return 0;
    }
    return copy_ootp_string_object_raw_text((uint8_t*)cell, 0u, out, out_size);
}

static const char* kbo_roster_import_trim_left(const char* text)
{
    while (text != NULL && (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')) {
        text++;
    }
    return text;
}

static void kbo_roster_import_copy_id_text(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (text == NULL) {
        return;
    }

    size_t copied = 0u;
    for (const char* p = text; *p != '\0' && copied + 1u < out_size; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch <= 0x20u || ch >= 0x7fu || ch == ',' || ch == '"') {
            continue;
        }
        out[copied++] = (char)ch;
    }
    out[copied] = '\0';
}

static int kbo_roster_import_read_i64(void* row, int32_t index, int64_t* out, int* out_eol)
{
    char text[96];
    if (out != NULL) { *out = 0; }
    if (out_eol != NULL) { *out_eol = 0; }

    if (!kbo_roster_import_cell_text(row, index, text, sizeof(text))) {
        return 0;
    }
    const char* start = kbo_roster_import_trim_left(text);
    if (kbo_roster_import_text_is_eol(start)) {
        if (out_eol != NULL) { *out_eol = 1; }
        return 0;
    }
    if (start == NULL || *start == '\0' || *start == ',') {
        return 1;
    }

    char* end = NULL;
    long long value = strtoll(start, &end, 10);
    if (end == start) {
        return 1;
    }
    if (out != NULL) {
        *out = (int64_t)value;
    }
    return 1;
}

static int kbo_roster_import_ascii_is_digit(char value)
{
    return value >= '0' && value <= '9';
}

static int kbo_roster_import_read_i64_strict(void* row, int32_t index, int64_t* out, int* out_eol)
{
    char text[96];
    if (out != NULL) { *out = 0; }
    if (out_eol != NULL) { *out_eol = 0; }

    if (!kbo_roster_import_cell_text(row, index, text, sizeof(text))) {
        return 0;
    }

    const char* start = kbo_roster_import_trim_left(text);
    if (kbo_roster_import_text_is_eol(start)) {
        if (out_eol != NULL) { *out_eol = 1; }
        return 0;
    }
    if (start == NULL || *start == '\0' || *start == ',') {
        return 0;
    }

    const char* digits = start;
    if (*digits == '-' || *digits == '+') {
        digits++;
    }
    if (!kbo_roster_import_ascii_is_digit(*digits)) {
        return 0;
    }

    char* end = NULL;
    long long value = strtoll(start, &end, 10);
    if (end == start) {
        return 0;
    }
    if (out != NULL) {
        *out = (int64_t)value;
    }
    return 1;
}

static int kbo_roster_import_read_chadwick_id(
    void* row,
    int32_t index,
    char* out,
    size_t out_size)
{
    char text[128];
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    if (!kbo_roster_import_cell_text(row, index, text, sizeof(text))) {
        return 0;
    }
    const char* start = kbo_roster_import_trim_left(text);
    if (kbo_roster_import_text_is_eol(start)) {
        return 0;
    }
    kbo_roster_import_copy_id_text(out, out_size, start);
    return 1;
}

static uint32_t kbo_roster_import_clamp_u32(int64_t value)
{
    if (value <= 0) {
        return 0u;
    }
    if (value > 0xFFFFFFFFll) {
        return 0xFFFFFFFFu;
    }
    return (uint32_t)value;
}

static uint32_t kbo_roster_import_clamp_proneness(int64_t value)
{
    if (value <= 0) {
        return 1u;
    }
    if (value > 200) {
        return 200u;
    }
    return (uint32_t)value;
}

static uint32_t kbo_roster_import_clamp_byte(int64_t value)
{
    if (value <= 0) {
        return 0u;
    }
    if (value > 255) {
        return 255u;
    }
    return (uint32_t)value;
}

static int32_t kbo_roster_import_clamp_i16_days(int64_t value)
{
    if (value <= 0) {
        return 0;
    }
    if (value > 32767) {
        return 32767;
    }
    return (int32_t)value;
}

static int kbo_roster_import_extra_values_are_plausible(const int64_t* values, int extra_column_count)
{
    if (values == NULL) {
        return 0;
    }
    if (extra_column_count != KBO_ROSTER_IMPORT_BASE_EXTRA_COLUMN_COUNT
            && extra_column_count != KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT
            && extra_column_count != KBO_ROSTER_IMPORT_CHADWICK_SET_EXTRA_COLUMN_COUNT
            && extra_column_count != KBO_ROSTER_IMPORT_FULL_EXTRA_COLUMN_COUNT) {
        return 0;
    }
    if ((values[0] != 0 && values[0] != 1) || (values[1] != 0 && values[1] != 1)) {
        return 0;
    }
    if (values[2] < 0 || values[2] > 32767) {
        return 0;
    }
    if (values[3] != 0 && (values[3] < 19000101 || values[3] > 21001231)) {
        return 0;
    }
    for (int i = 4; i <= 6; i++) {
        if (values[i] < 0 || values[i] > 1000000) {
            return 0;
        }
    }
    if (values[7] < 0 || values[7] > 0xFFFF) {
        return 0;
    }
    if (extra_column_count >= KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT) {
        for (int i = KBO_ROSTER_IMPORT_BASE_EXTRA_COLUMN_COUNT;
                i < KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT;
                i++) {
            if (values[i] < 0 || values[i] > 200) {
                return 0;
            }
        }
    }
    return 1;
}

static int kbo_roster_import_read_extra_values(
    void* row,
    int32_t start_index,
    int extra_column_count,
    KboRosterImportExtraValues* out)
{
    int32_t row_count = kbo_roster_import_row_count(row);
    if (row_count <= 0 || start_index < 0 || row_count < start_index + extra_column_count) {
        return 0;
    }

    int has_chadwick = extra_column_count >= KBO_ROSTER_IMPORT_CHADWICK_SET_EXTRA_COLUMN_COUNT;
    int has_popularity = extra_column_count >= KBO_ROSTER_IMPORT_FULL_EXTRA_COLUMN_COUNT;
    int numeric_column_count = has_chadwick
        ? KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT
        : extra_column_count;
    int64_t values[KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT] = {0};
    for (int i = 0; i < numeric_column_count; i++) {
        int eol = 0;
        if (!kbo_roster_import_read_i64_strict(row, start_index + i, &values[i], &eol)) {
            return 0;
        }
        if (eol) {
            return 0;
        }
    }
    if (!kbo_roster_import_extra_values_are_plausible(values, extra_column_count)) {
        return 0;
    }
    int64_t popularity_values[KBO_ROSTER_IMPORT_POPULARITY_EXTRA_COLUMN_COUNT] = {0};
    if (has_popularity) {
        for (int i = 0; i < KBO_ROSTER_IMPORT_POPULARITY_EXTRA_COLUMN_COUNT; i++) {
            int eol = 0;
            if (!kbo_roster_import_read_i64_strict(
                    row,
                    start_index + KBO_ROSTER_IMPORT_CHADWICK_SET_EXTRA_COLUMN_COUNT + i,
                    &popularity_values[i],
                    &eol)) {
                return 0;
            }
            if (eol || popularity_values[i] < 0 || popularity_values[i] > 255) {
                return 0;
            }
        }
    }

    memset(out, 0, sizeof(*out));
    out->military_active = values[0] > 0 ? 1 : 0;
    out->military_exempt = values[1] > 0 ? 1 : 0;
    out->military_days_left = kbo_roster_import_clamp_i16_days(values[2]);
    out->military_return_yyyymmdd = kbo_roster_import_clamp_u32(values[3]);
    out->service_team_id = kbo_roster_import_clamp_u32(values[4]);
    out->original_team_id = kbo_roster_import_clamp_u32(values[5]);
    out->original_league_id = kbo_roster_import_clamp_u32(values[6]);
    out->service_time_days = kbo_roster_import_clamp_u32(values[7]);
    if (extra_column_count >= KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT) {
        out->has_injury_proneness = 1;
        out->injury_proneness_overall = kbo_roster_import_clamp_proneness(values[8]);
        out->injury_proneness_back = kbo_roster_import_clamp_proneness(values[9]);
        out->injury_proneness_leg = kbo_roster_import_clamp_proneness(values[10]);
        out->injury_proneness_arm = kbo_roster_import_clamp_proneness(values[11]);
    }
    if (has_chadwick) {
        if (!kbo_roster_import_read_chadwick_id(
                row,
                start_index + KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT,
                out->chadwick_id,
                sizeof(out->chadwick_id))) {
            return 0;
        }
        out->has_chadwick_id = 1;
    }
    if (has_popularity) {
        out->has_popularity = 1;
        out->local_popularity = kbo_roster_import_clamp_byte(popularity_values[0]);
        out->national_popularity = kbo_roster_import_clamp_byte(popularity_values[1]);
    }
    return 1;
}

static int kbo_roster_import_read_extra_values_any(
    void* row,
    int32_t start_index,
    KboRosterImportExtraValues* out,
    int* out_extra_column_count)
{
    if (out_extra_column_count != NULL) {
        *out_extra_column_count = 0;
    }
    if (kbo_roster_import_read_extra_values(
            row,
            start_index,
            KBO_ROSTER_IMPORT_FULL_EXTRA_COLUMN_COUNT,
            out)) {
        if (out_extra_column_count != NULL) {
            *out_extra_column_count = KBO_ROSTER_IMPORT_FULL_EXTRA_COLUMN_COUNT;
        }
        return 1;
    }
    if (kbo_roster_import_read_extra_values(
            row,
            start_index,
            KBO_ROSTER_IMPORT_CHADWICK_SET_EXTRA_COLUMN_COUNT,
            out)) {
        if (out_extra_column_count != NULL) {
            *out_extra_column_count = KBO_ROSTER_IMPORT_CHADWICK_SET_EXTRA_COLUMN_COUNT;
        }
        return 1;
    }
    if (kbo_roster_import_read_extra_values(
            row,
            start_index,
            KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT,
            out)) {
        if (out_extra_column_count != NULL) {
            *out_extra_column_count = KBO_ROSTER_IMPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT;
        }
        return 1;
    }
    if (kbo_roster_import_read_extra_values(
            row,
            start_index,
            KBO_ROSTER_IMPORT_BASE_EXTRA_COLUMN_COUNT,
            out)) {
        if (out_extra_column_count != NULL) {
            *out_extra_column_count = KBO_ROSTER_IMPORT_BASE_EXTRA_COLUMN_COUNT;
        }
        return 1;
    }
    return 0;
}

static int kbo_roster_import_row_has_trailing_eol(void* row, int32_t row_count)
{
    if (row_count <= 0) {
        return 0;
    }

    int64_t ignored = 0;
    int eol = 0;
    (void)kbo_roster_import_read_i64(row, row_count - 1, &ignored, &eol);
    return eol;
}

static uint32_t kbo_roster_import_player_u32(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(player + offset);
}

static uint32_t kbo_roster_import_team_league_id(uint32_t team_id)
{
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

static void kbo_roster_import_apply_active_loan(
    uint8_t* player,
    uint32_t player_id,
    const KboRosterImportExtraValues* values)
{
    if (values->military_active == 0 || values->service_team_id == 0u) {
        unregister_active_kbo_military_loan(player_id);
        return;
    }

    uint32_t original_team_id = values->original_team_id;
    uint32_t original_league_id = values->original_league_id;
    if (original_team_id != 0u && original_league_id == 0u) {
        original_league_id = kbo_roster_import_team_league_id(original_team_id);
    }

    uint32_t service_league_id = kbo_roster_import_team_league_id(values->service_team_id);
    register_active_kbo_military_loan(
        player_id,
        (uintptr_t)player,
        original_team_id,
        original_league_id,
        values->service_team_id,
        service_league_id);

    int active_index = find_active_kbo_military_loan_index(player_id);
    KboMilitaryActiveLoan* loan = kbo_active_military_loan_at(active_index);
    if (loan != NULL && values->military_return_yyyymmdd != 0u) {
        loan->service_return_date_serial = kbo_military_yyyymmdd_to_serial(values->military_return_yyyymmdd);
    }
    if (loan != NULL && values->military_days_left > 0) {
        uint32_t today = kbo_current_date_serial();
        if (today != 0u) {
            loan->service_return_date_serial = today + (uint32_t)values->military_days_left;
        }
    }

    if (original_team_id != 0u) {
        kbo_military_repair_original_team_memory(
            player,
            original_team_id,
            original_league_id,
            values->service_team_id,
            0u,
            0u);
    }
}

static void kbo_roster_import_cleanup_misassigned_chadwick_id(
    uint8_t* player,
    const char* chadwick_id)
{
    if (player == NULL || chadwick_id == NULL || chadwick_id[0] == '\0') {
        return;
    }

    char current_mlb_id[KBO_ROSTER_IMPORT_CHADWICK_ID_BYTES] = {0};
    if (!copy_ootp_string_object_raw_text(
            player,
            OOTP27_PLAYER_MLB_ID_STRING_OFFSET,
            current_mlb_id,
            sizeof(current_mlb_id))) {
        return;
    }
    if (strcmp(current_mlb_id, chadwick_id) == 0) {
        assign_ootp_string_object_text_allow_empty(
            player,
            OOTP27_PLAYER_MLB_ID_STRING_OFFSET,
            "");
    }
}

void kbo_roster_import_apply_extra_values(
    uint8_t* player,
    const KboRosterImportExtraValues* values)
{
    if (player == NULL || values == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint32_t player_id = kbo_roster_import_player_u32(player, OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u || player_id > KBO_RUNTIME_PLAUSIBLE_PLAYER_ID_MAX) {
        return;
    }

    player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = values->military_active ? 1u : 0u;
    player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] = values->military_exempt ? 1u : 0u;

    if (values->military_active) {
        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] = 1u;
        kbo_set_military_days_left(player, values->military_days_left);
        kbo_roster_import_apply_active_loan(player, player_id, values);
    } else {
        unregister_active_kbo_military_loan(player_id);
        if (values->military_days_left == 0) {
            kbo_set_military_days_left(player, 0);
        }
    }

    if (values->military_exempt && !values->military_active) {
        kbo_set_military_days_left(player, 0);
    }

    if (values->service_time_days <= 0xFFFFu
            && memory_range_readable(player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET, sizeof(uint16_t))) {
        *(uint16_t*)(player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET) = (uint16_t)values->service_time_days;
    }

    if (values->has_injury_proneness) {
        if (player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] == 0u
                && memory_range_readable(
                    player + OOTP27_PLAYER_INJURY_PLAYOFF_RETURN_ROUND_OFFSET,
                    sizeof(uint32_t))) {
            uint32_t* playoff_return_round =
                (uint32_t*)(player + OOTP27_PLAYER_INJURY_PLAYOFF_RETURN_ROUND_OFFSET);
            if (*playoff_return_round == values->injury_proneness_overall) {
                *playoff_return_round = 0u;
            }
        }
        player[OOTP27_PLAYER_INJURY_PRONENESS_OVERALL_OFFSET] = (uint8_t)values->injury_proneness_overall;
        player[OOTP27_PLAYER_INJURY_PRONENESS_BACK_OFFSET] = (uint8_t)values->injury_proneness_back;
        player[OOTP27_PLAYER_INJURY_PRONENESS_LEG_OFFSET] = (uint8_t)values->injury_proneness_leg;
        player[OOTP27_PLAYER_INJURY_PRONENESS_ARM_OFFSET] = (uint8_t)values->injury_proneness_arm;
    }

    if (values->has_chadwick_id) {
        assign_ootp_string_object_text_allow_empty(
            player,
            OOTP27_PLAYER_CHADWICK_ID_STRING_OFFSET,
            values->chadwick_id);
        kbo_roster_import_cleanup_misassigned_chadwick_id(player, values->chadwick_id);
    }

    if (values->has_popularity) {
        player[OOTP27_PLAYER_LOCAL_POPULARITY_OFFSET] = (uint8_t)values->local_popularity;
        player[OOTP27_PLAYER_NATIONAL_POPULARITY_OFFSET] = (uint8_t)values->national_popularity;
    }
}

void kbo_roster_import_extra_columns_consume(uint8_t* player, void* row, int32_t* column_index)
{
    if (player == NULL || row == NULL || column_index == NULL
            || !memory_range_readable(column_index, sizeof(int32_t))) {
        return;
    }

    int32_t start_index = *column_index + 1;
    static volatile LONG probe_log_count = 0;
    LONG probe_slot = InterlockedIncrement(&probe_log_count);
    if (probe_slot <= 20) {
        char first_extra[64] = {0};
        char second_extra[64] = {0};
        kbo_roster_import_cell_text(row, start_index, first_extra, sizeof(first_extra));
        kbo_roster_import_cell_text(row, start_index + 1, second_extra, sizeof(second_extra));
        kbo_log_runtimef(
            "KBO roster import extra columns probe player_id=%u column_index=%d start=%d row_count=%d first=%s second=%s",
            kbo_roster_import_player_u32(player, OOTP27_PLAYER_ID_OFFSET),
            *column_index,
            start_index,
            kbo_roster_import_row_count(row),
            first_extra,
            second_extra);
    }

    KboRosterImportExtraValues values;
    memset(&values, 0, sizeof(values));
    int extra_column_count = 0;
    if (!kbo_roster_import_read_extra_values_any(row, start_index, &values, &extra_column_count)) {
        if (probe_slot <= 20) {
            kbo_log_runtimef(
                "KBO roster import extra columns probe failed player_id=%u column_index=%d start=%d row_count=%d",
                kbo_roster_import_player_u32(player, OOTP27_PLAYER_ID_OFFSET),
                *column_index,
                start_index,
                kbo_roster_import_row_count(row));
        }
        return;
    }

    kbo_roster_import_apply_extra_values(player, &values);
    *column_index += extra_column_count;

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 40) {
        kbo_log_runtimef(
            "KBO roster import extra columns applied player_id=%u columns=%d active=%d exempt=%d days_left=%d service_team=%u original_team=%u service_days=%u prone_overall_back_leg_arm=%u/%u/%u/%u has_prone=%d chadwick_id=%s has_chadwick=%d popularity_local_national=%u/%u has_popularity=%d",
            kbo_roster_import_player_u32(player, OOTP27_PLAYER_ID_OFFSET),
            extra_column_count,
            values.military_active,
            values.military_exempt,
            values.military_days_left,
            values.service_team_id,
            values.original_team_id,
            values.service_time_days,
            values.injury_proneness_overall,
            values.injury_proneness_back,
            values.injury_proneness_leg,
            values.injury_proneness_arm,
            values.has_injury_proneness,
            values.chadwick_id,
            values.has_chadwick_id,
            values.local_popularity,
            values.national_popularity,
            values.has_popularity);
    }
}

void kbo_roster_import_extra_columns_consume_tail(uint8_t* player, void* row)
{
    if (player == NULL || row == NULL) {
        return;
    }

    int32_t row_count = kbo_roster_import_row_count(row);
    if (row_count <= KBO_ROSTER_IMPORT_ORIGINAL_COLUMN_COUNT) {
        return;
    }

    int has_eol = kbo_roster_import_row_has_trailing_eol(row, row_count);
    int32_t start_index = KBO_ROSTER_IMPORT_ORIGINAL_COLUMN_COUNT;

    static volatile LONG probe_log_count = 0;
    LONG probe_slot = InterlockedIncrement(&probe_log_count);
    if (probe_slot <= 20) {
        char first_extra[64] = {0};
        char second_extra[64] = {0};
        kbo_roster_import_cell_text(row, start_index, first_extra, sizeof(first_extra));
        kbo_roster_import_cell_text(row, start_index + 1, second_extra, sizeof(second_extra));
        kbo_log_runtimef(
            "KBO roster import extra columns tail probe player_id=%u start=%d row_count=%d has_eol=%d first=%s second=%s",
            kbo_roster_import_player_u32(player, OOTP27_PLAYER_ID_OFFSET),
            start_index,
            row_count,
            has_eol,
            first_extra,
            second_extra);
    }

    KboRosterImportExtraValues values;
    memset(&values, 0, sizeof(values));
    int extra_column_count = 0;
    if (!kbo_roster_import_read_extra_values_any(row, start_index, &values, &extra_column_count)) {
        if (probe_slot <= 20) {
            kbo_log_runtimef(
                "KBO roster import extra columns tail probe failed player_id=%u start=%d row_count=%d has_eol=%d",
                kbo_roster_import_player_u32(player, OOTP27_PLAYER_ID_OFFSET),
                start_index,
                row_count,
                has_eol);
        }
        return;
    }

    kbo_roster_import_apply_extra_values(player, &values);

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 40) {
        kbo_log_runtimef(
            "KBO roster import extra columns tail applied player_id=%u columns=%d active=%d exempt=%d days_left=%d service_team=%u original_team=%u original_league=%u service_days=%u prone_overall_back_leg_arm=%u/%u/%u/%u has_prone=%d chadwick_id=%s has_chadwick=%d popularity_local_national=%u/%u has_popularity=%d",
            kbo_roster_import_player_u32(player, OOTP27_PLAYER_ID_OFFSET),
            extra_column_count,
            values.military_active,
            values.military_exempt,
            values.military_days_left,
            values.service_team_id,
            values.original_team_id,
            values.original_league_id,
            values.service_time_days,
            values.injury_proneness_overall,
            values.injury_proneness_back,
            values.injury_proneness_leg,
            values.injury_proneness_arm,
            values.has_injury_proneness,
            values.chadwick_id,
            values.has_chadwick_id,
            values.local_popularity,
            values.national_popularity,
            values.has_popularity);
    }
}
