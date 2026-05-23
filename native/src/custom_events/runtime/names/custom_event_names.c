#include "../common/custom_events_common.h"
#include "custom_event_names.h"

#include <stdio.h>
#include <string.h>

#include "../../../core/news/templates/core_news_templates.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/text/ootp_text_encoding.h"
#include "../catalog/custom_event_catalog.h"

#define KBO_CUSTOM_EVENT_TITLE_MAX 160u
#define KBO_CUSTOM_EVENT_TITLE_KEY_MAX 128u
#define KBO_CUSTOM_EVENT_LANGUAGE_COUNT 2u

const char* kbo_custom_event_kind_key(KboCustomEventKind kind)
{
    switch (kind) {
    case KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN:
        return "foreign_priority_open";
    case KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE:
        return "foreign_priority_close";
    case KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION:
        return "military_selection";
    case KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION:
        return "asian_games_selection";
    case KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE:
        return "asian_games_departure";
    case KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL:
        return "asian_games_final";
    case KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE:
        return "cbt_exception_deadline";
    case KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT:
        return "cbt_announcement";
    case KBO_CUSTOM_EVENT_KIND_FA_DECLARATION:
        return "fa_declaration";
    case KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA:
        return "intl_established_fa";
    case KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN:
        return "independent_team_acquisition_open";
    case KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT:
        return "secondary_draft";
    default:
        return "unknown";
    }
}

static INIT_ONCE g_kbo_custom_event_titles_once = INIT_ONCE_STATIC_INIT;
static char g_kbo_custom_event_titles[KBO_CUSTOM_EVENT_KIND_COUNT][KBO_CUSTOM_EVENT_LANGUAGE_COUNT][KBO_CUSTOM_EVENT_TITLE_MAX];

static int kbo_custom_event_language_index_from_dir(const char* language_dir)
{
    return language_dir != NULL && strcmp(language_dir, "en") == 0 ? 1 : 0;
}

static const char* kbo_custom_event_language_dir_from_index(int language_index)
{
    return language_index == 1 ? "en" : "ko";
}

static int kbo_ascii_segment_equals_ignore_case(const char* text, size_t len, const char* expected)
{
    if (text == NULL || expected == NULL) {
        return 0;
    }
    size_t expected_len = strlen(expected);
    if (len != expected_len) {
        return 0;
    }
    for (size_t i = 0u; i < len; i++) {
        char a = text[i];
        char b = expected[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int kbo_custom_event_prefix_is_known(const char* text, size_t len)
{
    return kbo_ascii_segment_equals_ignore_case(text, len, "KBO")
        || kbo_ascii_segment_equals_ignore_case(text, len, "KBO CBT");
}

static int kbo_custom_event_title_has_plain_prefix(const char* source, const char* prefix)
{
    if (source == NULL || prefix == NULL) {
        return 0;
    }
    size_t i = 0u;
    for (; prefix[i] != '\0'; i++) {
        if (source[i] == '\0') {
            return 0;
        }
        char a = source[i];
        char b = prefix[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return source[i] == ' ';
}

static size_t kbo_custom_event_title_content_start(const char* source)
{
    if (source == NULL || source[0] == '\0') {
        return 0u;
    }
    if (source[0] == '[') {
        size_t close_index = 1u;
        while (close_index < 64u && source[close_index] != '\0' && source[close_index] != ']') {
            unsigned char c = (unsigned char)source[close_index];
            if (c < 0x20u) {
                break;
            }
            close_index++;
        }
        if (source[close_index] == ']'
                && source[close_index + 1u] == ' '
                && close_index > 1u
                && kbo_custom_event_prefix_is_known(source + 1u, close_index - 1u)) {
            return close_index + 2u;
        }
    }
    if (kbo_custom_event_title_has_plain_prefix(source, "KBO CBT")) {
        return 8u;
    }
    if (kbo_custom_event_title_has_plain_prefix(source, "KBO")) {
        return 4u;
    }
    return 0u;
}

static int kbo_copy_title_without_known_prefix(const char* source, char* out, size_t out_size)
{
    if (source == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    size_t source_index = kbo_custom_event_title_content_start(source);
    size_t out_index = 0u;
    for (size_t i = source_index; source[i] != '\0' && out_index + 1u < out_size; i++) {
        out[out_index++] = source[i];
    }
    out[out_index] = '\0';
    return out_index > 0u;
}

static int kbo_custom_event_title_text_matches(const char* name, const char* title)
{
    if (name == NULL || title == NULL || name[0] == '\0' || title[0] == '\0') {
        return 0;
    }
    if (strcmp(name, title) == 0 || ascii_equals_ignore_case(name, title)) {
        return 1;
    }

    char normalized_name[KBO_CUSTOM_EVENT_TITLE_MAX] = {0};
    char normalized_title[KBO_CUSTOM_EVENT_TITLE_MAX] = {0};
    if (!kbo_copy_title_without_known_prefix(name, normalized_name, sizeof(normalized_name))
            || !kbo_copy_title_without_known_prefix(title, normalized_title, sizeof(normalized_title))) {
        return 0;
    }
    return strcmp(normalized_name, normalized_title) == 0
        || ascii_equals_ignore_case(normalized_name, normalized_title);
}

static BOOL CALLBACK kbo_custom_event_titles_init_once(
    PINIT_ONCE init_once,
    PVOID parameter,
    PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    for (int kind_value = KBO_CUSTOM_EVENT_KIND_UNKNOWN + 1; kind_value < KBO_CUSTOM_EVENT_KIND_COUNT; kind_value++) {
        KboCustomEventKind kind = (KboCustomEventKind)kind_value;
        char title_key[KBO_CUSTOM_EVENT_TITLE_KEY_MAX] = {0};
        if (!kbo_custom_event_catalog_title_key(kind, title_key, sizeof(title_key))) {
            continue;
        }

        for (int language_index = 0; language_index < (int)KBO_CUSTOM_EVENT_LANGUAGE_COUNT; language_index++) {
            char* title = g_kbo_custom_event_titles[kind][language_index];
            const char* language_dir = kbo_custom_event_language_dir_from_index(language_index);
            title[0] = '\0';
            if (!kbo_news_template_load_for_language(
                    language_dir,
                    title_key,
                    title,
                    KBO_CUSTOM_EVENT_TITLE_MAX,
                    NULL,
                    0u,
                    "custom_event_title")) {
                snprintf(title, KBO_CUSTOM_EVENT_TITLE_MAX, "%s", title_key);
            }
        }
    }

    return TRUE;
}

static void kbo_custom_event_ensure_title_cache(void)
{
    InitOnceExecuteOnce(
        &g_kbo_custom_event_titles_once,
        kbo_custom_event_titles_init_once,
        NULL,
        NULL);
}

int kbo_custom_event_title_for_kind(KboCustomEventKind kind, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return 0;
    }
    out[0] = '\0';

    kbo_custom_event_ensure_title_cache();

    int language_index = kbo_custom_event_language_index_from_dir(kbo_custom_news_language_dir());
    snprintf(out, out_size, "%s", g_kbo_custom_event_titles[kind][language_index]);
    return out[0] != '\0';
}

int kbo_custom_event_name_equals_title(const char* name, const char* title)
{
    if (name == NULL || title == NULL || name[0] == '\0' || title[0] == '\0') {
        return 0;
    }
    if (kbo_custom_event_title_text_matches(name, title)) {
        return 1;
    }

    char* internal_title = kbo_alloc_ootp_internal_text(title);
    int matches = internal_title != NULL
        && kbo_custom_event_title_text_matches(name, internal_title);
    kbo_free_ootp_internal_text(internal_title);
    return matches;
}

int kbo_custom_event_name_is_kind(const char* name, KboCustomEventKind kind)
{
    if (name == NULL || name[0] == '\0' || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return 0;
    }

    kbo_custom_event_ensure_title_cache();

    for (int language_index = 0; language_index < (int)KBO_CUSTOM_EVENT_LANGUAGE_COUNT; language_index++) {
        if (kbo_custom_event_name_equals_title(name, g_kbo_custom_event_titles[kind][language_index])) {
            return 1;
        }
    }

    return 0;
}

KboCustomEventKind kbo_custom_event_kind_from_name(const char* name)
{
    if (name == NULL || name[0] == '\0') {
        return KBO_CUSTOM_EVENT_KIND_UNKNOWN;
    }

    kbo_custom_event_ensure_title_cache();

    for (int kind_value = KBO_CUSTOM_EVENT_KIND_UNKNOWN + 1; kind_value < KBO_CUSTOM_EVENT_KIND_COUNT; kind_value++) {
        KboCustomEventKind kind = (KboCustomEventKind)kind_value;
        for (int language_index = 0; language_index < (int)KBO_CUSTOM_EVENT_LANGUAGE_COUNT; language_index++) {
            if (kbo_custom_event_name_equals_title(name, g_kbo_custom_event_titles[kind][language_index])) {
                return kind;
            }
        }
    }

    return KBO_CUSTOM_EVENT_KIND_UNKNOWN;
}

int kbo_custom_event_name_is_open(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN);
}

int kbo_custom_event_name_is_close(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE);
}

int kbo_custom_event_name_is_military_selection(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION);
}

int kbo_custom_event_name_is_asian_games_selection(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION);
}

int kbo_custom_event_name_is_asian_games_departure(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE);
}

int kbo_custom_event_name_is_asian_games_final(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL);
}

int kbo_custom_event_name_is_cbt_exception_deadline(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE);
}

int kbo_custom_event_name_is_cbt_announcement(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT);
}

int kbo_custom_event_name_is_fa_declaration(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_FA_DECLARATION);
}

int kbo_custom_event_name_is_intl_established_fa(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA);
}

int kbo_custom_event_name_is_independent_team_acquisition_open(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN);
}

int kbo_custom_event_name_is_secondary_draft(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT);
}
