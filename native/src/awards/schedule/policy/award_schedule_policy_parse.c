#include "../award_schedule_probe_module.h"
#include "../../../core/files/save_paths/platform/core_path_io.h"

static int kbo_award_schedule_read_file(const char* path, char** out_text, DWORD* out_size)
{
    if (path == NULL || out_text == NULL || out_size == NULL) {
        return 0;
    }
    *out_text = NULL;
    *out_size = 0u;

    HANDLE file = kbo_create_file_read_utf8(path);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return 0;
    }

    char* text = (char*)calloc((size_t)size.QuadPart + 1u, 1u);
    if (text == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    BOOL ok = ReadFile(file, text, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!ok || read == 0u) {
        free(text);
        return 0;
    }
    text[read] = '\0';
    *out_text = text;
    *out_size = read;
    return 1;
}

int kbo_award_schedule_load_text(char** out_text, DWORD* out_size, char* out_path, size_t out_path_size)
{
    char path[MAX_PATH] = {0};
    if (kbo_get_save_scoped_data_file(KBO_AWARD_SCHEDULE_POLICY_FILE, path, sizeof(path))
            && kbo_award_schedule_read_file(path, out_text, out_size)) {
        if (out_path != NULL && out_path_size > 0u) {
            snprintf(out_path, out_path_size, "%s", path);
        }
        return 1;
    }

    path[0] = '\0';
    if (kbo_get_global_data_file(KBO_AWARD_SCHEDULE_POLICY_FILE, path, sizeof(path))
            && kbo_award_schedule_read_file(path, out_text, out_size)) {
        if (out_path != NULL && out_path_size > 0u) {
            snprintf(out_path, out_path_size, "%s", path);
        }
        return 1;
    }
    return 0;
}

static const char* kbo_award_find_matching_brace(const char* open_brace)
{
    if (open_brace == NULL || *open_brace != '{') {
        return NULL;
    }
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    for (const char* p = open_brace; *p != '\0'; p++) {
        char ch = *p;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
            continue;
        }
        if (ch == '"') {
            in_string = 1;
        } else if (ch == '{') {
            depth++;
        } else if (ch == '}') {
            depth--;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

static const char* kbo_award_find_matching_bracket(const char* open_bracket)
{
    if (open_bracket == NULL || *open_bracket != '[') {
        return NULL;
    }
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    for (const char* p = open_bracket; *p != '\0'; p++) {
        char ch = *p;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
            continue;
        }
        if (ch == '"') {
            in_string = 1;
        } else if (ch == '[') {
            depth++;
        } else if (ch == ']') {
            depth--;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

static const char* kbo_award_json_key_in_range(const char* start, const char* end, const char* key)
{
    char pattern[80] = {0};
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    size_t len = strlen(pattern);
    for (const char* p = start; p != NULL && p + len <= end; p++) {
        if (memcmp(p, pattern, len) == 0) {
            return p + len;
        }
    }
    return NULL;
}

static int kbo_award_json_uint_in_range(const char* start, const char* end, const char* key, uint32_t* out)
{
    const char* p = kbo_award_json_key_in_range(start, end, key);
    if (p == NULL) {
        return 0;
    }
    while (p < end && *p != ':') {
        p++;
    }
    if (p >= end) {
        return 0;
    }
    p++;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
    }
    if (p >= end || *p < '0' || *p > '9') {
        return 0;
    }
    uint32_t value = 0u;
    while (p < end && *p >= '0' && *p <= '9') {
        value = value * 10u + (uint32_t)(*p - '0');
        p++;
    }
    *out = value;
    return 1;
}

static int kbo_award_json_string_in_range(const char* start, const char* end, const char* key, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    const char* p = kbo_award_json_key_in_range(start, end, key);
    if (p == NULL) {
        return 0;
    }
    while (p < end && *p != ':') {
        p++;
    }
    if (p >= end) {
        return 0;
    }
    p++;
    while (p < end && *p != '"') {
        p++;
    }
    if (p >= end || *p != '"') {
        return 0;
    }
    p++;
    size_t used = 0u;
    while (p < end && *p != '"' && used + 1u < out_size) {
        if (*p == '\\' && p + 1 < end) {
            p++;
        }
        out[used++] = *p++;
    }
    out[used] = '\0';
    return used > 0u;
}

static void kbo_award_parse_titles(const char* start, const char* end, KboAwardScheduleRule* rule)
{
    const char* p = kbo_award_json_key_in_range(start, end, "match_titles");
    if (p == NULL) {
        return;
    }
    while (p < end && *p != '[') {
        p++;
    }
    const char* array_end = kbo_award_find_matching_bracket(p);
    if (array_end == NULL || array_end > end) {
        return;
    }
    p++;
    while (p < array_end && rule->title_count < KBO_AWARD_SCHEDULE_MAX_TITLES) {
        while (p < array_end && *p != '"') {
            p++;
        }
        if (p >= array_end) {
            break;
        }
        p++;
        char* out = rule->titles[rule->title_count];
        size_t used = 0u;
        while (p < array_end && *p != '"' && used + 1u < 96u) {
            if (*p == '\\' && p + 1 < array_end) {
                p++;
            }
            out[used++] = *p++;
        }
        out[used] = '\0';
        if (used > 0u) {
            rule->title_count++;
        }
        if (p < array_end) {
            p++;
        }
    }
}

static void kbo_award_parse_event_types(const char* start, const char* end, KboAwardScheduleRule* rule)
{
    const char* p = kbo_award_json_key_in_range(start, end, "event_types");
    if (p == NULL) {
        return;
    }
    while (p < end && *p != '[') {
        p++;
    }
    const char* array_end = kbo_award_find_matching_bracket(p);
    if (array_end == NULL || array_end > end) {
        return;
    }
    p++;
    while (p < array_end && rule->event_type_count < KBO_AWARD_SCHEDULE_MAX_EVENT_TYPES) {
        while (p < array_end && (*p < '0' || *p > '9')) {
            p++;
        }
        if (p >= array_end) {
            break;
        }
        uint32_t value = 0u;
        while (p < array_end && *p >= '0' && *p <= '9') {
            value = value * 10u + (uint32_t)(*p - '0');
            p++;
        }
        if (value <= 0xffffu) {
            rule->event_types[rule->event_type_count++] = (uint16_t)value;
        }
    }
}

static void kbo_award_parse_overrides(const char* start, const char* end, KboAwardScheduleRule* rule)
{
    const char* p = kbo_award_json_key_in_range(start, end, "year_overrides");
    if (p == NULL) {
        return;
    }
    while (p < end && *p != '[') {
        p++;
    }
    const char* array_end = kbo_award_find_matching_bracket(p);
    if (array_end == NULL || array_end > end) {
        return;
    }
    p++;
    while (p < array_end && rule->override_count < KBO_AWARD_SCHEDULE_MAX_OVERRIDES) {
        while (p < array_end && *p != '{') {
            p++;
        }
        if (p >= array_end) {
            break;
        }
        const char* object_end = kbo_award_find_matching_brace(p);
        if (object_end == NULL || object_end > array_end) {
            break;
        }
        KboAwardScheduleOverride item = {0};
        if (kbo_award_json_uint_in_range(p, object_end, "year", &item.year)
                && kbo_award_json_uint_in_range(p, object_end, "month", &item.month)
                && kbo_award_json_uint_in_range(p, object_end, "day", &item.day)
                && item.year >= 1982u && item.year <= 2400u
                && item.month >= 1u && item.month <= 12u
                && item.day >= 1u && item.day <= 31u) {
            rule->overrides[rule->override_count++] = item;
        }
        p = object_end + 1;
    }
}

int kbo_award_schedule_parse_policy(const char* json, KboAwardSchedulePolicy* out)
{
    if (json == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    const char* awards = strstr(json, "\"awards\"");
    if (awards == NULL) {
        return 0;
    }
    const char* array = strchr(awards, '[');
    const char* array_end = kbo_award_find_matching_bracket(array);
    if (array == NULL || array_end == NULL) {
        return 0;
    }

    const char* p = array + 1;
    while (p < array_end && out->rule_count < KBO_AWARD_SCHEDULE_MAX_RULES) {
        while (p < array_end && *p != '{') {
            p++;
        }
        if (p >= array_end) {
            break;
        }
        const char* object_end = kbo_award_find_matching_brace(p);
        if (object_end == NULL || object_end > array_end) {
            break;
        }

        KboAwardScheduleRule rule = {0};
        kbo_award_json_string_in_range(p, object_end, "id", rule.id, sizeof(rule.id));
        kbo_award_json_string_in_range(p, object_end, "label", rule.label, sizeof(rule.label));
        kbo_award_json_string_in_range(p, object_end, "label_en", rule.label_en, sizeof(rule.label_en));
        kbo_award_json_string_in_range(p, object_end, "label_ko", rule.label_ko, sizeof(rule.label_ko));
        kbo_award_json_uint_in_range(p, object_end, "default_month", &rule.default_month);
        kbo_award_json_uint_in_range(p, object_end, "default_day", &rule.default_day);
        kbo_award_parse_titles(p, object_end, &rule);
        kbo_award_parse_event_types(p, object_end, &rule);
        kbo_award_parse_overrides(p, object_end, &rule);
        if ((rule.title_count > 0u || rule.event_type_count > 0u)
                && rule.default_month >= 1u && rule.default_month <= 12u
                && rule.default_day >= 1u && rule.default_day <= 31u) {
            out->rules[out->rule_count++] = rule;
        }
        p = object_end + 1;
    }

    return out->rule_count > 0u;
}
