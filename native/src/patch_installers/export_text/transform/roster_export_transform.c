#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../military_service/players/loans/military_active_loan.h"
#include "../../../military_service/players/state/military_player_state.h"
#include "../../../military_service/players/team_policy/military_service_team_policy.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_string.h"
#include "roster_export_transform.h"

#define KBO_ROSTER_EXPORT_ORIGINAL_COLUMN_COUNT 157u
#define KBO_ROSTER_EXPORT_BASE_EXTRA_COLUMN_COUNT 8u
#define KBO_ROSTER_EXPORT_PRONENESS_EXTRA_COLUMN_COUNT 4u
#define KBO_ROSTER_EXPORT_CHADWICK_EXTRA_COLUMN_COUNT 1u
#define KBO_ROSTER_EXPORT_POPULARITY_EXTRA_COLUMN_COUNT 2u
#define KBO_ROSTER_EXPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT \
    (KBO_ROSTER_EXPORT_BASE_EXTRA_COLUMN_COUNT + KBO_ROSTER_EXPORT_PRONENESS_EXTRA_COLUMN_COUNT)
#define KBO_ROSTER_EXPORT_CHADWICK_SET_EXTRA_COLUMN_COUNT \
    (KBO_ROSTER_EXPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT + KBO_ROSTER_EXPORT_CHADWICK_EXTRA_COLUMN_COUNT)
#define KBO_ROSTER_EXPORT_FULL_EXTRA_COLUMN_COUNT \
    (KBO_ROSTER_EXPORT_CHADWICK_SET_EXTRA_COLUMN_COUNT + KBO_ROSTER_EXPORT_POPULARITY_EXTRA_COLUMN_COUNT)

static const char* kbo_roster_export_extra_header =
    ", kbo_military_active"
    ", kbo_military_exempt"
    ", kbo_military_days_left"
    ", kbo_military_return_yyyymmdd"
    ", kbo_service_team_id"
    ", kbo_original_team_id"
    ", kbo_original_league_id"
    ", kbo_service_time_days"
    ", kbo_injury_proneness_overall"
    ", kbo_injury_proneness_back"
    ", kbo_injury_proneness_leg"
    ", kbo_injury_proneness_arm"
    ", chadwick_id"
    ", kbo_local_popularity"
    ", kbo_national_popularity";

static const char* kbo_roster_export_proneness_extra_header =
    ", kbo_injury_proneness_overall"
    ", kbo_injury_proneness_back"
    ", kbo_injury_proneness_leg"
    ", kbo_injury_proneness_arm"
    ", chadwick_id"
    ", kbo_local_popularity"
    ", kbo_national_popularity";

static const char* kbo_roster_export_chadwick_extra_header =
    ", chadwick_id"
    ", kbo_local_popularity"
    ", kbo_national_popularity";

static const char* kbo_roster_export_popularity_extra_header =
    ", kbo_local_popularity"
    ", kbo_national_popularity";

static int kbo_roster_export_ascii_equal_ci(char a, char b)
{
    if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
    }
    return a == b;
}

int kbo_roster_export_buffer_append(
    char** buffer,
    size_t* len,
    size_t* cap,
    const char* data,
    size_t data_len)
{
    if (buffer == NULL || len == NULL || cap == NULL || data == NULL) {
        return 0;
    }
    if (data_len == 0u) {
        return 1;
    }
    if (*len > KBO_ROSTER_EXPORT_PENDING_MAX || data_len > KBO_ROSTER_EXPORT_PENDING_MAX - *len) {
        return 0;
    }

    size_t required = *len + data_len + 1u;
    if (required > *cap) {
        size_t next_cap = *cap != 0u ? *cap : 4096u;
        while (next_cap < required) {
            if (next_cap > KBO_ROSTER_EXPORT_PENDING_MAX / 2u) {
                next_cap = KBO_ROSTER_EXPORT_PENDING_MAX + 1u;
                break;
            }
            next_cap *= 2u;
        }
        if (next_cap > KBO_ROSTER_EXPORT_PENDING_MAX + 1u) {
            return 0;
        }

        void* next = *buffer == NULL
            ? HeapAlloc(GetProcessHeap(), 0, next_cap)
            : HeapReAlloc(GetProcessHeap(), 0, *buffer, next_cap);
        if (next == NULL) {
            return 0;
        }
        *buffer = (char*)next;
        *cap = next_cap;
    }

    memcpy(*buffer + *len, data, data_len);
    *len += data_len;
    (*buffer)[*len] = '\0';
    return 1;
}

static uint32_t kbo_roster_export_read_u32(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(player + offset);
}

static uint16_t kbo_roster_export_read_u16(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint16_t))) {
        return 0u;
    }
    return *(uint16_t*)(player + offset);
}

static uint8_t kbo_roster_export_read_u8(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint8_t))) {
        return 0u;
    }
    return player[offset];
}

static void kbo_roster_export_sanitize_id_text(char* text)
{
    if (text == NULL) {
        return;
    }
    char* out = text;
    for (const char* in = text; *in != '\0'; in++) {
        unsigned char ch = (unsigned char)*in;
        if (ch <= 0x20u || ch >= 0x7fu || ch == ',' || ch == '"') {
            continue;
        }
        *out++ = (char)ch;
    }
    *out = '\0';
}

static void kbo_roster_export_chadwick_id(uint8_t* player, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (player == NULL) {
        return;
    }
    copy_ootp_string_object_raw_text(
        player,
        OOTP27_PLAYER_CHADWICK_ID_STRING_OFFSET,
        out,
        out_size);
    kbo_roster_export_sanitize_id_text(out);
}

static uint32_t kbo_roster_export_service_team_id(uint8_t* player, uint32_t player_id)
{
    int active_index = player_id != 0u ? find_active_kbo_military_loan_index(player_id) : -1;
    if (active_index >= 0) {
        KboMilitaryActiveLoan* loan = kbo_active_military_loan_at(active_index);
        if (loan != NULL && loan->service_team_id != 0u) {
            return loan->service_team_id;
        }
    }

    uint32_t current_team_id = kbo_roster_export_read_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t loan_team_id = kbo_roster_export_read_u32(player, OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    uint32_t active_team_id = kbo_roster_export_read_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (kbo_team_id_is_military_service_team(current_team_id)) {
        return current_team_id;
    }
    if (kbo_team_id_is_military_service_team(loan_team_id)) {
        return loan_team_id;
    }
    if (kbo_team_id_is_military_service_team(active_team_id)) {
        return active_team_id;
    }
    return 0u;
}

static void kbo_roster_export_original_team_ids(
    uint8_t* player,
    uint32_t player_id,
    uint32_t service_team_id,
    uint32_t* out_team_id,
    uint32_t* out_league_id)
{
    if (out_team_id != NULL) { *out_team_id = 0u; }
    if (out_league_id != NULL) { *out_league_id = 0u; }

    int active_index = player_id != 0u ? find_active_kbo_military_loan_index(player_id) : -1;
    if (active_index >= 0) {
        KboMilitaryActiveLoan* loan = kbo_active_military_loan_at(active_index);
        if (loan != NULL) {
            if (out_team_id != NULL) { *out_team_id = loan->original_team_id; }
            if (out_league_id != NULL) { *out_league_id = loan->original_league_id; }
            if ((out_team_id == NULL || *out_team_id != 0u)
                    && (out_league_id == NULL || *out_league_id != 0u)) {
                return;
            }
        }
    }

    uint32_t original_team_id = kbo_roster_export_read_u32(player, OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint32_t original_league_id = kbo_roster_export_read_u32(player, OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    if (original_team_id == 0u || original_team_id == service_team_id
            || kbo_team_id_is_military_service_team(original_team_id)) {
        original_team_id = kbo_roster_export_read_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    }
    if (original_team_id == 0u || original_team_id == service_team_id
            || kbo_team_id_is_military_service_team(original_team_id)) {
        original_team_id = kbo_roster_export_read_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    }

    if (out_team_id != NULL) { *out_team_id = original_team_id; }
    if (out_league_id != NULL) { *out_league_id = original_league_id; }
}

static void kbo_roster_export_append_player_columns(char* out, size_t out_size, uint32_t player_id)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    uint8_t* player = kbo_military_find_player_by_id(player_id);
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        snprintf(out, out_size, ",0,0,0,0,0,0,0,0,0,0,0,0,,0,0");
        return;
    }

    uint8_t military_active = kbo_roster_export_read_u8(player, OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET);
    uint8_t military_exempt = kbo_roster_export_read_u8(player, OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET);
    int32_t days_left = kbo_military_effective_days_left(player);
    uint32_t return_yyyymmdd = kbo_military_effective_return_yyyymmdd(player);
    uint32_t service_team_id = kbo_roster_export_service_team_id(player, player_id);
    uint32_t original_team_id = 0u;
    uint32_t original_league_id = 0u;
    kbo_roster_export_original_team_ids(player, player_id, service_team_id, &original_team_id, &original_league_id);
    uint16_t service_time_days = kbo_roster_export_read_u16(player, OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET);
    uint8_t injury_proneness_overall =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_INJURY_PRONENESS_OVERALL_OFFSET);
    uint8_t injury_proneness_back =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_INJURY_PRONENESS_BACK_OFFSET);
    uint8_t injury_proneness_leg =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_INJURY_PRONENESS_LEG_OFFSET);
    uint8_t injury_proneness_arm =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_INJURY_PRONENESS_ARM_OFFSET);
    char chadwick_id[80] = {0};
    kbo_roster_export_chadwick_id(player, chadwick_id, sizeof(chadwick_id));
    uint8_t local_popularity =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_LOCAL_POPULARITY_OFFSET);
    uint8_t national_popularity =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_NATIONAL_POPULARITY_OFFSET);

    snprintf(
        out,
        out_size,
        ",%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%s,%u,%u",
        (unsigned)military_active,
        (unsigned)military_exempt,
        (int)days_left,
        (unsigned)return_yyyymmdd,
        (unsigned)service_team_id,
        (unsigned)original_team_id,
        (unsigned)original_league_id,
        (unsigned)service_time_days,
        (unsigned)injury_proneness_overall,
        (unsigned)injury_proneness_back,
        (unsigned)injury_proneness_leg,
        (unsigned)injury_proneness_arm,
        chadwick_id,
        (unsigned)local_popularity,
        (unsigned)national_popularity);
}

static void kbo_roster_export_append_player_proneness_columns(char* out, size_t out_size, uint32_t player_id)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    uint8_t* player = kbo_military_find_player_by_id(player_id);
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        snprintf(out, out_size, ",0,0,0,0,,0,0");
        return;
    }

    uint8_t injury_proneness_overall =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_INJURY_PRONENESS_OVERALL_OFFSET);
    uint8_t injury_proneness_back =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_INJURY_PRONENESS_BACK_OFFSET);
    uint8_t injury_proneness_leg =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_INJURY_PRONENESS_LEG_OFFSET);
    uint8_t injury_proneness_arm =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_INJURY_PRONENESS_ARM_OFFSET);
    char chadwick_id[80] = {0};
    kbo_roster_export_chadwick_id(player, chadwick_id, sizeof(chadwick_id));
    uint8_t local_popularity =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_LOCAL_POPULARITY_OFFSET);
    uint8_t national_popularity =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_NATIONAL_POPULARITY_OFFSET);

    snprintf(
        out,
        out_size,
        ",%u,%u,%u,%u,%s,%u,%u",
        (unsigned)injury_proneness_overall,
        (unsigned)injury_proneness_back,
        (unsigned)injury_proneness_leg,
        (unsigned)injury_proneness_arm,
        chadwick_id,
        (unsigned)local_popularity,
        (unsigned)national_popularity);
}

static void kbo_roster_export_append_player_chadwick_column(char* out, size_t out_size, uint32_t player_id)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    uint8_t* player = kbo_military_find_player_by_id(player_id);
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        snprintf(out, out_size, ",,0,0");
        return;
    }

    char chadwick_id[80] = {0};
    kbo_roster_export_chadwick_id(player, chadwick_id, sizeof(chadwick_id));
    uint8_t local_popularity =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_LOCAL_POPULARITY_OFFSET);
    uint8_t national_popularity =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_NATIONAL_POPULARITY_OFFSET);
    snprintf(
        out,
        out_size,
        ",%s,%u,%u",
        chadwick_id,
        (unsigned)local_popularity,
        (unsigned)national_popularity);
}

static void kbo_roster_export_append_player_popularity_columns(char* out, size_t out_size, uint32_t player_id)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    uint8_t* player = kbo_military_find_player_by_id(player_id);
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        snprintf(out, out_size, ",0,0");
        return;
    }

    uint8_t local_popularity =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_LOCAL_POPULARITY_OFFSET);
    uint8_t national_popularity =
        kbo_roster_export_read_u8(player, OOTP27_PLAYER_NATIONAL_POPULARITY_OFFSET);
    snprintf(
        out,
        out_size,
        ",%u,%u",
        (unsigned)local_popularity,
        (unsigned)national_popularity);
}

static int kbo_roster_export_parse_player_id(const char* line, size_t line_len, uint32_t* out_player_id)
{
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (line == NULL || line_len == 0u || line[0] < '0' || line[0] > '9') {
        return 0;
    }

    uint64_t value = 0u;
    size_t i = 0u;
    for (; i < line_len; i++) {
        char ch = line[i];
        if (ch == ',') {
            break;
        }
        if (ch < '0' || ch > '9') {
            return 0;
        }
        value = value * 10u + (uint64_t)(ch - '0');
        if (value > 2000000000u) {
            return 0;
        }
    }
    if (i == 0u || i >= line_len || line[i] != ',' || value == 0u) {
        return 0;
    }
    if (out_player_id != NULL) {
        *out_player_id = (uint32_t)value;
    }
    return 1;
}

static int kbo_roster_export_line_has_header(const char* line, size_t line_len)
{
    static const char prefix[] = "//id, del, team_id";
    size_t prefix_len = sizeof(prefix) - 1u;
    return line != NULL
        && line_len >= prefix_len
        && memcmp(line, prefix, prefix_len) == 0;
}

static int kbo_roster_export_line_contains(const char* line, size_t line_len, const char* needle)
{
    if (needle == NULL) {
        return 0;
    }
    size_t needle_len = strlen(needle);
    if (line == NULL || needle_len == 0u || line_len < needle_len) {
        return 0;
    }
    for (size_t i = 0u; i + needle_len <= line_len; i++) {
        if (memcmp(line + i, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static size_t kbo_roster_export_field_count_before_insert(const char* line, size_t insert_len)
{
    if (line == NULL || insert_len == 0u) {
        return 0u;
    }

    size_t count = 1u;
    for (size_t i = 0u; i < insert_len; i++) {
        if (line[i] == ',') {
            count++;
        }
    }
    return count;
}

static int kbo_roster_export_line_terminal_eol_offset(const char* line, size_t line_len, size_t* out_insert_len)
{
    if (out_insert_len != NULL) {
        *out_insert_len = line_len;
    }
    if (line == NULL || line_len < 4u) {
        return 0;
    }

    size_t pos = line_len - 4u;
    if (line[pos] != ','
            || !kbo_roster_export_ascii_equal_ci(line[pos + 1u], 'e')
            || !kbo_roster_export_ascii_equal_ci(line[pos + 2u], 'o')
            || !kbo_roster_export_ascii_equal_ci(line[pos + 3u], 'l')) {
        return 0;
    }

    if (out_insert_len != NULL) {
        *out_insert_len = pos;
    }
    return 1;
}

static int kbo_roster_export_transform_line(const char* line, size_t line_len, char** out, size_t* out_len, size_t* out_cap)
{
    if (line == NULL || out == NULL || out_len == NULL || out_cap == NULL) {
        return 0;
    }

    size_t body_len = line_len;
    size_t eol_start = line_len;
    if (body_len > 0u && line[body_len - 1u] == '\n') {
        eol_start = body_len - 1u;
        if (eol_start > 0u && line[eol_start - 1u] == '\r') {
            eol_start--;
        }
        body_len = eol_start;
    }

    size_t insert_len = body_len;
    kbo_roster_export_line_terminal_eol_offset(line, body_len, &insert_len);

    if (!kbo_roster_export_buffer_append(out, out_len, out_cap, line, insert_len)) {
        return 0;
    }

    if (kbo_roster_export_line_has_header(line, body_len)) {
        if (kbo_roster_export_line_contains(line, body_len, "kbo_local_popularity")
                || kbo_roster_export_line_contains(line, body_len, "kbo_national_popularity")) {
            /* Already contains the full extension set. */
        } else if (kbo_roster_export_line_contains(line, body_len, "chadwick_id")) {
            if (!kbo_roster_export_buffer_append(
                    out,
                    out_len,
                    out_cap,
                    kbo_roster_export_popularity_extra_header,
                    strlen(kbo_roster_export_popularity_extra_header))) {
                return 0;
            }
        } else if (kbo_roster_export_line_contains(line, body_len, "kbo_military_active")) {
            const char* header = kbo_roster_export_line_contains(line, body_len, "kbo_injury_proneness_back")
                ? kbo_roster_export_chadwick_extra_header
                : kbo_roster_export_proneness_extra_header;
            if (!kbo_roster_export_buffer_append(
                    out,
                    out_len,
                    out_cap,
                    header,
                    strlen(header))) {
                return 0;
            }
        } else if (!kbo_roster_export_buffer_append(
                    out,
                    out_len,
                    out_cap,
                    kbo_roster_export_extra_header,
                    strlen(kbo_roster_export_extra_header))) {
            return 0;
        }
    } else {
        uint32_t player_id = 0u;
        if (kbo_roster_export_parse_player_id(line, body_len, &player_id)) {
            char columns[256] = {0};
            size_t field_count = kbo_roster_export_field_count_before_insert(line, insert_len);
            if (field_count >= KBO_ROSTER_EXPORT_ORIGINAL_COLUMN_COUNT + KBO_ROSTER_EXPORT_FULL_EXTRA_COLUMN_COUNT) {
                columns[0] = '\0';
            } else if (field_count >= KBO_ROSTER_EXPORT_ORIGINAL_COLUMN_COUNT + KBO_ROSTER_EXPORT_CHADWICK_SET_EXTRA_COLUMN_COUNT) {
                kbo_roster_export_append_player_popularity_columns(columns, sizeof(columns), player_id);
            } else if (field_count >= KBO_ROSTER_EXPORT_ORIGINAL_COLUMN_COUNT + KBO_ROSTER_EXPORT_PRONENESS_SET_EXTRA_COLUMN_COUNT) {
                kbo_roster_export_append_player_chadwick_column(columns, sizeof(columns), player_id);
            } else if (field_count >= KBO_ROSTER_EXPORT_ORIGINAL_COLUMN_COUNT + KBO_ROSTER_EXPORT_BASE_EXTRA_COLUMN_COUNT) {
                kbo_roster_export_append_player_proneness_columns(columns, sizeof(columns), player_id);
            } else {
                kbo_roster_export_append_player_columns(columns, sizeof(columns), player_id);
            }
            if (columns[0] != '\0'
                    && !kbo_roster_export_buffer_append(out, out_len, out_cap, columns, strlen(columns))) {
                return 0;
            }
        }
    }

    if (insert_len < body_len
            && !kbo_roster_export_buffer_append(out, out_len, out_cap, line + insert_len, body_len - insert_len)) {
        return 0;
    }

    if (eol_start < line_len) {
        return kbo_roster_export_buffer_append(out, out_len, out_cap, line + eol_start, line_len - eol_start);
    }
    return 1;
}

int kbo_roster_export_build_transformed(
    char** pending,
    size_t* pending_len,
    size_t* pending_cap,
    const char* data,
    size_t data_len,
    int flush_pending,
    char** out,
    size_t* out_len,
    size_t* out_cap)
{
    if (pending == NULL || pending_len == NULL || pending_cap == NULL
            || out == NULL || out_len == NULL || out_cap == NULL) {
        return 0;
    }
    if (data_len > 0u && !kbo_roster_export_buffer_append(pending, pending_len, pending_cap, data, data_len)) {
        return 0;
    }

    size_t consumed = 0u;
    for (size_t i = 0u; i < *pending_len; i++) {
        if ((*pending)[i] != '\n') {
            continue;
        }
        size_t line_len = i + 1u - consumed;
        if (!kbo_roster_export_transform_line(*pending + consumed, line_len, out, out_len, out_cap)) {
            return 0;
        }
        consumed = i + 1u;
    }

    if (flush_pending && consumed < *pending_len) {
        size_t line_len = *pending_len - consumed;
        if (!kbo_roster_export_transform_line(*pending + consumed, line_len, out, out_len, out_cap)) {
            return 0;
        }
        consumed = *pending_len;
    }

    if (consumed > 0u) {
        size_t remaining = *pending_len - consumed;
        if (remaining > 0u) {
            memmove(*pending, *pending + consumed, remaining);
        }
        *pending_len = remaining;
        if (*pending != NULL) {
            (*pending)[remaining] = '\0';
        }
    }
    return 1;
}
