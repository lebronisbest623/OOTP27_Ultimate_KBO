#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../military_service/players/state/military_player_state.h"
#include "../../../team/lookup/team_lookup.h"
#include "roster_import_extra_columns_consume.h"
#include "roster_import_extra_columns_direct.h"

#define KBO_ROSTER_IMPORT_DIRECT_FIELD_MAX 256
#define KBO_ROSTER_IMPORT_DIRECT_PATH_MAX 1024
#define KBO_ROSTER_IMPORT_DIRECT_FILE_MAX (64u * 1024u * 1024u)

typedef struct KboRosterImportDirectField {
    char* text;
} KboRosterImportDirectField;

typedef struct KboRosterImportDirectIndexes {
    int id;
    int military_active;
    int military_exempt;
    int military_days_left;
    int military_return_yyyymmdd;
    int service_team_id;
    int original_team_id;
    int original_league_id;
    int service_time_days;
    int injury_proneness_overall;
    int injury_proneness_back;
    int injury_proneness_leg;
    int injury_proneness_arm;
    int chadwick_id;
    int local_popularity;
    int national_popularity;
} KboRosterImportDirectIndexes;

typedef struct KboRosterImportDirectDelayRequest {
    char path[KBO_ROSTER_IMPORT_DIRECT_PATH_MAX];
} KboRosterImportDirectDelayRequest;

static volatile LONG g_kbo_roster_import_direct_session_active = 0;
static volatile LONG g_kbo_roster_import_direct_duplicate_logs = 0;
static volatile LONG g_kbo_roster_import_direct_applied_logs = 0;

static int kbo_roster_import_direct_ascii_equal_ci(char a, char b)
{
    if (a >= 'A' && a <= 'Z') { a = (char)(a - 'A' + 'a'); }
    if (b >= 'A' && b <= 'Z') { b = (char)(b - 'A' + 'a'); }
    return a == b;
}

static int kbo_roster_import_direct_ends_with_ci(const char* text, const char* suffix)
{
    if (text == NULL || suffix == NULL) {
        return 0;
    }
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    if (suffix_len == 0u || text_len < suffix_len) {
        return 0;
    }

    const char* start = text + text_len - suffix_len;
    for (size_t i = 0u; i < suffix_len; i++) {
        char a = start[i] == '/' ? '\\' : start[i];
        char b = suffix[i] == '/' ? '\\' : suffix[i];
        if (!kbo_roster_import_direct_ascii_equal_ci(a, b)) {
            return 0;
        }
    }
    return 1;
}

static int kbo_roster_import_direct_path_is_roster(const char* path)
{
    return path != NULL
        && path[0] != '\0'
        && (kbo_roster_import_direct_ends_with_ci(path, "\\import_export\\kbo_rosters.txt")
            || kbo_roster_import_direct_ends_with_ci(path, "\\import_export\\kbo_rosters.csv"));
}

static int kbo_roster_import_direct_mode_is_read_only(const char* mode)
{
    if (mode == NULL || mode[0] == '\0') {
        return 0;
    }

    int has_read = 0;
    for (const char* p = mode; *p != '\0'; p++) {
        char ch = *p;
        if (ch == 'r' || ch == 'R') {
            has_read = 1;
        }
        if (ch == 'w' || ch == 'W' || ch == 'a' || ch == 'A' || ch == '+') {
            return 0;
        }
    }
    return has_read;
}

static char* kbo_roster_import_direct_trim(char* text)
{
    if (text == NULL) {
        return NULL;
    }
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }

    char* end = text + strlen(text);
    while (end > text
            && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';

    if (text[0] == '"' && end > text + 1 && end[-1] == '"') {
        text++;
        end[-1] = '\0';
    }
    if (text[0] == '/' && text[1] == '/') {
        text += 2;
        while (*text == ' ' || *text == '\t') {
            text++;
        }
    }
    return text;
}

static int kbo_roster_import_direct_text_equals_ci(const char* a, const char* b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a != '\0' && *b != '\0') {
        if (!kbo_roster_import_direct_ascii_equal_ci(*a, *b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int kbo_roster_import_direct_split_csv_line(
    char* line,
    KboRosterImportDirectField* fields,
    int max_fields)
{
    if (line == NULL || fields == NULL || max_fields <= 0) {
        return 0;
    }

    int count = 0;
    int in_quote = 0;
    char* field_start = line;
    for (char* p = line; ; p++) {
        char ch = *p;
        if (ch == '"') {
            if (in_quote && p[1] == '"') {
                p++;
                continue;
            }
            in_quote = !in_quote;
        }
        if ((ch == ',' && !in_quote) || ch == '\0') {
            if (count >= max_fields) {
                return count;
            }
            *p = '\0';
            fields[count].text = kbo_roster_import_direct_trim(field_start);
            count++;
            if (ch == '\0') {
                break;
            }
            field_start = p + 1;
        }
    }
    return count;
}

static int kbo_roster_import_direct_find_column(
    KboRosterImportDirectField* fields,
    int field_count,
    const char* name)
{
    for (int i = 0; i < field_count; i++) {
        if (kbo_roster_import_direct_text_equals_ci(fields[i].text, name)) {
            return i;
        }
    }
    return -1;
}

static int kbo_roster_import_direct_find_column_any(
    KboRosterImportDirectField* fields,
    int field_count,
    const char* primary_name,
    const char* alternate_name)
{
    int index = kbo_roster_import_direct_find_column(fields, field_count, primary_name);
    if (index >= 0 || alternate_name == NULL) {
        return index;
    }
    return kbo_roster_import_direct_find_column(fields, field_count, alternate_name);
}

static int kbo_roster_import_direct_indexes_complete(const KboRosterImportDirectIndexes* idx)
{
    return idx != NULL
        && idx->id >= 0
        && idx->military_active >= 0
        && idx->military_exempt >= 0
        && idx->military_days_left >= 0
        && idx->military_return_yyyymmdd >= 0
        && idx->service_team_id >= 0
        && idx->original_team_id >= 0
        && idx->original_league_id >= 0
        && idx->service_time_days >= 0;
}

static int kbo_roster_import_direct_indexes_have_proneness(const KboRosterImportDirectIndexes* idx)
{
    return idx != NULL
        && idx->injury_proneness_overall >= 0
        && idx->injury_proneness_back >= 0
        && idx->injury_proneness_leg >= 0
        && idx->injury_proneness_arm >= 0;
}

static int kbo_roster_import_direct_indexes_have_partial_proneness(const KboRosterImportDirectIndexes* idx)
{
    if (idx == NULL || kbo_roster_import_direct_indexes_have_proneness(idx)) {
        return 0;
    }
    return idx->injury_proneness_overall >= 0
        || idx->injury_proneness_back >= 0
        || idx->injury_proneness_leg >= 0
        || idx->injury_proneness_arm >= 0;
}

static int kbo_roster_import_direct_indexes_have_popularity(const KboRosterImportDirectIndexes* idx)
{
    return idx != NULL
        && idx->local_popularity >= 0
        && idx->national_popularity >= 0;
}

static int kbo_roster_import_direct_indexes_have_partial_popularity(const KboRosterImportDirectIndexes* idx)
{
    if (idx == NULL || kbo_roster_import_direct_indexes_have_popularity(idx)) {
        return 0;
    }
    return idx->local_popularity >= 0
        || idx->national_popularity >= 0;
}

static int kbo_roster_import_direct_parse_header(
    char* line,
    KboRosterImportDirectIndexes* idx)
{
    if (line == NULL || idx == NULL) {
        return 0;
    }

    KboRosterImportDirectField fields[KBO_ROSTER_IMPORT_DIRECT_FIELD_MAX];
    memset(fields, 0, sizeof(fields));
    int field_count = kbo_roster_import_direct_split_csv_line(
        line,
        fields,
        KBO_ROSTER_IMPORT_DIRECT_FIELD_MAX);
    idx->id = kbo_roster_import_direct_find_column(fields, field_count, "id");
    idx->military_active = kbo_roster_import_direct_find_column(fields, field_count, "kbo_military_active");
    idx->military_exempt = kbo_roster_import_direct_find_column(fields, field_count, "kbo_military_exempt");
    idx->military_days_left = kbo_roster_import_direct_find_column(fields, field_count, "kbo_military_days_left");
    idx->military_return_yyyymmdd = kbo_roster_import_direct_find_column(fields, field_count, "kbo_military_return_yyyymmdd");
    idx->service_team_id = kbo_roster_import_direct_find_column(fields, field_count, "kbo_service_team_id");
    idx->original_team_id = kbo_roster_import_direct_find_column(fields, field_count, "kbo_original_team_id");
    idx->original_league_id = kbo_roster_import_direct_find_column(fields, field_count, "kbo_original_league_id");
    idx->service_time_days = kbo_roster_import_direct_find_column(fields, field_count, "kbo_service_time_days");
    idx->injury_proneness_overall =
        kbo_roster_import_direct_find_column(fields, field_count, "kbo_injury_proneness_overall");
    idx->injury_proneness_back =
        kbo_roster_import_direct_find_column(fields, field_count, "kbo_injury_proneness_back");
    idx->injury_proneness_leg =
        kbo_roster_import_direct_find_column(fields, field_count, "kbo_injury_proneness_leg");
    idx->injury_proneness_arm =
        kbo_roster_import_direct_find_column(fields, field_count, "kbo_injury_proneness_arm");
    idx->chadwick_id =
        kbo_roster_import_direct_find_column_any(fields, field_count, "kbo_chadwick_id", "chadwick_id");
    idx->local_popularity =
        kbo_roster_import_direct_find_column_any(fields, field_count, "kbo_local_popularity", "local_pop");
    idx->national_popularity =
        kbo_roster_import_direct_find_column_any(fields, field_count, "kbo_national_popularity", "national_pop");
    return kbo_roster_import_direct_indexes_complete(idx)
        && !kbo_roster_import_direct_indexes_have_partial_proneness(idx)
        && !kbo_roster_import_direct_indexes_have_partial_popularity(idx);
}

static int64_t kbo_roster_import_direct_parse_i64(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    char* end = NULL;
    long long value = strtoll(text, &end, 10);
    if (end == text) {
        return 0;
    }
    return (int64_t)value;
}

static void kbo_roster_import_direct_copy_id_text(char* out, size_t out_size, const char* text)
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

static uint32_t kbo_roster_import_direct_clamp_u32(int64_t value)
{
    if (value <= 0) {
        return 0u;
    }
    if (value > 0xFFFFFFFFll) {
        return 0xFFFFFFFFu;
    }
    return (uint32_t)value;
}

static uint32_t kbo_roster_import_direct_clamp_proneness(int64_t value)
{
    if (value <= 0) {
        return 1u;
    }
    if (value > 200) {
        return 200u;
    }
    return (uint32_t)value;
}

static uint32_t kbo_roster_import_direct_clamp_byte(int64_t value)
{
    if (value <= 0) {
        return 0u;
    }
    if (value > 255) {
        return 255u;
    }
    return (uint32_t)value;
}

static int32_t kbo_roster_import_direct_clamp_i16_days(int64_t value)
{
    if (value <= 0) {
        return 0;
    }
    if (value > 32767) {
        return 32767;
    }
    return (int32_t)value;
}

static int kbo_roster_import_direct_values_from_fields(
    KboRosterImportDirectField* fields,
    int field_count,
    const KboRosterImportDirectIndexes* idx,
    uint32_t* out_player_id,
    KboRosterImportExtraValues* out_values)
{
    if (fields == NULL || idx == NULL || out_player_id == NULL || out_values == NULL
            || !kbo_roster_import_direct_indexes_complete(idx)) {
        return 0;
    }
    int required[] = {
        idx->id,
        idx->military_active,
        idx->military_exempt,
        idx->military_days_left,
        idx->military_return_yyyymmdd,
        idx->service_team_id,
        idx->original_team_id,
        idx->original_league_id,
        idx->service_time_days
    };
    for (size_t i = 0u; i < sizeof(required) / sizeof(required[0]); i++) {
        if (required[i] < 0 || required[i] >= field_count) {
            return 0;
        }
    }
    int has_proneness = kbo_roster_import_direct_indexes_have_proneness(idx);
    if (has_proneness) {
        int proneness_required[] = {
            idx->injury_proneness_overall,
            idx->injury_proneness_back,
            idx->injury_proneness_leg,
            idx->injury_proneness_arm
        };
        for (size_t i = 0u; i < sizeof(proneness_required) / sizeof(proneness_required[0]); i++) {
            if (proneness_required[i] < 0 || proneness_required[i] >= field_count) {
                return 0;
            }
        }
    }
    int has_popularity = kbo_roster_import_direct_indexes_have_popularity(idx);
    if (has_popularity) {
        int popularity_required[] = {
            idx->local_popularity,
            idx->national_popularity
        };
        for (size_t i = 0u; i < sizeof(popularity_required) / sizeof(popularity_required[0]); i++) {
            if (popularity_required[i] < 0 || popularity_required[i] >= field_count) {
                return 0;
            }
        }
    }

    uint32_t player_id = kbo_roster_import_direct_clamp_u32(
        kbo_roster_import_direct_parse_i64(fields[idx->id].text));
    if (player_id == 0u) {
        return 0;
    }

    memset(out_values, 0, sizeof(*out_values));
    out_values->military_active = kbo_roster_import_direct_parse_i64(fields[idx->military_active].text) > 0 ? 1 : 0;
    out_values->military_exempt = kbo_roster_import_direct_parse_i64(fields[idx->military_exempt].text) > 0 ? 1 : 0;
    out_values->military_days_left = kbo_roster_import_direct_clamp_i16_days(
        kbo_roster_import_direct_parse_i64(fields[idx->military_days_left].text));
    out_values->military_return_yyyymmdd = kbo_roster_import_direct_clamp_u32(
        kbo_roster_import_direct_parse_i64(fields[idx->military_return_yyyymmdd].text));
    out_values->service_team_id = kbo_roster_import_direct_clamp_u32(
        kbo_roster_import_direct_parse_i64(fields[idx->service_team_id].text));
    out_values->original_team_id = kbo_roster_import_direct_clamp_u32(
        kbo_roster_import_direct_parse_i64(fields[idx->original_team_id].text));
    out_values->original_league_id = kbo_roster_import_direct_clamp_u32(
        kbo_roster_import_direct_parse_i64(fields[idx->original_league_id].text));
    out_values->service_time_days = kbo_roster_import_direct_clamp_u32(
        kbo_roster_import_direct_parse_i64(fields[idx->service_time_days].text));
    if (has_proneness) {
        out_values->has_injury_proneness = 1;
        out_values->injury_proneness_overall = kbo_roster_import_direct_clamp_proneness(
            kbo_roster_import_direct_parse_i64(fields[idx->injury_proneness_overall].text));
        out_values->injury_proneness_back = kbo_roster_import_direct_clamp_proneness(
            kbo_roster_import_direct_parse_i64(fields[idx->injury_proneness_back].text));
        out_values->injury_proneness_leg = kbo_roster_import_direct_clamp_proneness(
            kbo_roster_import_direct_parse_i64(fields[idx->injury_proneness_leg].text));
        out_values->injury_proneness_arm = kbo_roster_import_direct_clamp_proneness(
            kbo_roster_import_direct_parse_i64(fields[idx->injury_proneness_arm].text));
    }
    if (idx->chadwick_id >= 0 && idx->chadwick_id < field_count) {
        out_values->has_chadwick_id = 1;
        kbo_roster_import_direct_copy_id_text(
            out_values->chadwick_id,
            sizeof(out_values->chadwick_id),
            fields[idx->chadwick_id].text);
    }
    if (has_popularity) {
        out_values->has_popularity = 1;
        out_values->local_popularity = kbo_roster_import_direct_clamp_byte(
            kbo_roster_import_direct_parse_i64(fields[idx->local_popularity].text));
        out_values->national_popularity = kbo_roster_import_direct_clamp_byte(
            kbo_roster_import_direct_parse_i64(fields[idx->national_popularity].text));
    }
    *out_player_id = player_id;
    return 1;
}

static char* kbo_roster_import_direct_next_line(char* cursor, char** out_next)
{
    if (out_next != NULL) {
        *out_next = NULL;
    }
    if (cursor == NULL || *cursor == '\0') {
        return NULL;
    }

    char* p = cursor;
    while (*p != '\0' && *p != '\r' && *p != '\n') {
        p++;
    }
    if (*p == '\0') {
        return cursor;
    }

    char* next = p + 1;
    if (*p == '\r' && *next == '\n') {
        next++;
    }
    *p = '\0';
    if (out_next != NULL) {
        *out_next = next;
    }
    return cursor;
}

static char* kbo_roster_import_direct_read_file(const char* path, size_t* out_size)
{
    if (out_size != NULL) {
        *out_size = 0u;
    }
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    LARGE_INTEGER size;
    memset(&size, 0, sizeof(size));
    if (!GetFileSizeEx(file, &size)
            || size.QuadPart <= 0
            || size.QuadPart > (LONGLONG)KBO_ROSTER_IMPORT_DIRECT_FILE_MAX) {
        CloseHandle(file);
        return NULL;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size.QuadPart + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return NULL;
    }

    DWORD total = 0u;
    while (total < (DWORD)size.QuadPart) {
        DWORD chunk = 0u;
        DWORD want = (DWORD)size.QuadPart - total;
        if (!ReadFile(file, buffer + total, want, &chunk, NULL) || chunk == 0u) {
            HeapFree(GetProcessHeap(), 0, buffer);
            CloseHandle(file);
            return NULL;
        }
        total += chunk;
    }
    CloseHandle(file);
    buffer[total] = '\0';
    if (out_size != NULL) {
        *out_size = (size_t)total;
    }
    return buffer;
}

static int kbo_roster_import_direct_apply_path_once(const char* path, const char* reason)
{
    size_t file_size = 0u;
    char* buffer = kbo_roster_import_direct_read_file(path, &file_size);
    if (buffer == NULL) {
        kbo_log_runtimef(
            "KBO roster import extra columns direct import skipped reason=read_failed source=%s path=%s",
            reason != NULL ? reason : "",
            path != NULL ? path : "");
        return 0;
    }

    KboRosterImportDirectIndexes idx;
    memset(&idx, 0xFF, sizeof(idx));
    int header_found = 0;
    int missing_columns = 0;
    int rows = 0;
    int applied = 0;
    int active = 0;
    int missing_player = 0;
    int invalid = 0;

    char* cursor = buffer;
    while (cursor != NULL && *cursor != '\0') {
        char* next = NULL;
        char* line = kbo_roster_import_direct_next_line(cursor, &next);
        cursor = next;
        if (line == NULL || line[0] == '\0') {
            continue;
        }

        if (!header_found) {
            if (line[0] == '/' && line[1] == '/' && strstr(line, "kbo_military_active") != NULL) {
                header_found = 1;
                missing_columns = !kbo_roster_import_direct_parse_header(line, &idx);
            }
            continue;
        }

        if (missing_columns) {
            continue;
        }
        if (line[0] == '/' && line[1] == '/') {
            continue;
        }

        rows++;
        KboRosterImportDirectField fields[KBO_ROSTER_IMPORT_DIRECT_FIELD_MAX];
        memset(fields, 0, sizeof(fields));
        int field_count = kbo_roster_import_direct_split_csv_line(
            line,
            fields,
            KBO_ROSTER_IMPORT_DIRECT_FIELD_MAX);
        uint32_t player_id = 0u;
        KboRosterImportExtraValues values;
        if (!kbo_roster_import_direct_values_from_fields(fields, field_count, &idx, &player_id, &values)) {
            invalid++;
            continue;
        }

        uint8_t* player = kbo_military_find_player_by_id(player_id);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            missing_player++;
            continue;
        }

        kbo_roster_import_apply_extra_values(player, &values);
        applied++;
        if (values.military_active) {
            active++;
        }

        LONG log_slot = InterlockedIncrement(&g_kbo_roster_import_direct_applied_logs);
        if (log_slot <= 40) {
            kbo_log_runtimef(
                "KBO roster import extra columns direct applied player_id=%u active=%d exempt=%d days_left=%d service_team=%u original_team=%u service_days=%u prone_overall_back_leg_arm=%u/%u/%u/%u has_prone=%d chadwick_id=%s has_chadwick=%d popularity_local_national=%u/%u has_popularity=%d source=%s",
                player_id,
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
                values.has_popularity,
                reason != NULL ? reason : "");
        }
    }

    if (!header_found) {
        kbo_log_runtimef(
            "KBO roster import extra columns direct import skipped reason=missing_header source=%s bytes=%llu path=%s",
            reason != NULL ? reason : "",
            (unsigned long long)file_size,
            path != NULL ? path : "");
    } else if (missing_columns) {
        kbo_log_runtimef(
            "KBO roster import extra columns direct import skipped reason=missing_columns source=%s id=%d active=%d exempt=%d days=%d return=%d service=%d original=%d league=%d service_days=%d prone_overall=%d prone_back=%d prone_leg=%d prone_arm=%d chadwick=%d local_popularity=%d national_popularity=%d path=%s",
            reason != NULL ? reason : "",
            idx.id,
            idx.military_active,
            idx.military_exempt,
            idx.military_days_left,
            idx.military_return_yyyymmdd,
            idx.service_team_id,
            idx.original_team_id,
            idx.original_league_id,
            idx.service_time_days,
            idx.injury_proneness_overall,
            idx.injury_proneness_back,
            idx.injury_proneness_leg,
            idx.injury_proneness_arm,
            idx.chadwick_id,
            idx.local_popularity,
            idx.national_popularity,
            path != NULL ? path : "");
    } else {
        kbo_log_runtimef(
            "KBO roster import extra columns direct import summary source=%s rows=%d applied=%d active=%d missing_player=%d invalid=%d bytes=%llu path=%s",
            reason != NULL ? reason : "",
            rows,
            applied,
            active,
            missing_player,
            invalid,
            (unsigned long long)file_size,
            path != NULL ? path : "");
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return applied;
}

static DWORD WINAPI kbo_roster_import_direct_delayed_thread(LPVOID parameter)
{
    KboRosterImportDirectDelayRequest* request = (KboRosterImportDirectDelayRequest*)parameter;
    if (request != NULL) {
        if (kbo_runtime_sleep_should_continue(750u)) {
            kbo_roster_import_direct_apply_path_once(request->path, "delayed_750ms");
        }
        if (kbo_runtime_sleep_should_continue(2000u)) {
            kbo_roster_import_direct_apply_path_once(request->path, "delayed_2750ms");
        }
        HeapFree(GetProcessHeap(), 0, request);
    }
    InterlockedExchange(&g_kbo_roster_import_direct_session_active, 0);
    return 0;
}

void kbo_roster_import_extra_columns_direct_maybe_apply_path(const char* path, const char* mode)
{
    if (!kbo_roster_import_direct_path_is_roster(path)
            || !kbo_roster_import_direct_mode_is_read_only(mode)) {
        return;
    }

    if (InterlockedCompareExchange(&g_kbo_roster_import_direct_session_active, 1, 0) != 0) {
        LONG duplicate_slot = InterlockedIncrement(&g_kbo_roster_import_direct_duplicate_logs);
        if (duplicate_slot <= 16) {
            kbo_log_runtimef(
                "KBO roster import extra columns direct import skipped reason=duplicate_session mode=%s path=%s",
                mode != NULL ? mode : "",
                path != NULL ? path : "");
        }
        return;
    }

    kbo_roster_import_direct_apply_path_once(path, "stdio_open");

    KboRosterImportDirectDelayRequest* request =
        (KboRosterImportDirectDelayRequest*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*request));
    if (request == NULL) {
        InterlockedExchange(&g_kbo_roster_import_direct_session_active, 0);
        kbo_log_runtime_line("KBO roster import extra columns direct import delayed request allocation failed");
        return;
    }

    snprintf(request->path, sizeof(request->path), "%s", path);
    if (!kbo_start_runtime_thread(
            kbo_roster_import_direct_delayed_thread,
            request,
            "roster import extra columns direct delayed apply")) {
        HeapFree(GetProcessHeap(), 0, request);
        InterlockedExchange(&g_kbo_roster_import_direct_session_active, 0);
    }
}
