#include "core_news_links.h"

#include <string.h>

void kbo_news_related_ids_init(KboNewsRelatedIds* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
}

static int kbo_news_related_ids_contains(const uint32_t* ids, int count, uint32_t id)
{
    if (ids == NULL || id == 0u) {
        return 1;
    }
    for (int i = 0; i < count; i++) {
        if (ids[i] == id) {
            return 1;
        }
    }
    return 0;
}

static void kbo_news_related_ids_add_player(KboNewsRelatedIds* out, uint32_t id)
{
    if (out == NULL || id == 0u || out->player_count >= KBO_NEWS_RELATED_PLAYER_MAX
            || kbo_news_related_ids_contains(out->player_ids, out->player_count, id)) {
        return;
    }
    out->player_ids[out->player_count++] = id;
}

static void kbo_news_related_ids_add_team(KboNewsRelatedIds* out, uint32_t id)
{
    if (out == NULL || id == 0u || out->team_count >= KBO_NEWS_RELATED_TEAM_MAX
            || kbo_news_related_ids_contains(out->team_ids, out->team_count, id)) {
        return;
    }
    out->team_ids[out->team_count++] = id;
}

static uint32_t kbo_news_parse_link_id(const char* p)
{
    if (p == NULL) {
        return 0u;
    }
    uint32_t value = 0u;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        uint32_t digit = (uint32_t)(*p - '0');
        if (value > 429496729u || (value == 429496729u && digit > 5u)) {
            return 0u;
        }
        value = value * 10u + digit;
        digits++;
        p++;
    }
    if (digits == 0 || *p != '>') {
        return 0u;
    }
    return value;
}

void kbo_news_related_ids_collect(KboNewsRelatedIds* out, const char* text)
{
    if (out == NULL || text == NULL || text[0] == '\0') {
        return;
    }

    const char* p = text;
    while (*p != '\0') {
        const char* player = strstr(p, ":player#");
        const char* team = strstr(p, ":team#");
        if (player == NULL && team == NULL) {
            break;
        }
        if (player != NULL && (team == NULL || player < team)) {
            kbo_news_related_ids_add_player(out, kbo_news_parse_link_id(player + 8));
            p = player + 8;
        } else {
            kbo_news_related_ids_add_team(out, kbo_news_parse_link_id(team + 6));
            p = team + 6;
        }
    }
}

void kbo_news_related_ids_collect_pair(KboNewsRelatedIds* out, const char* title, const char* body)
{
    kbo_news_related_ids_init(out);
    kbo_news_related_ids_collect(out, title);
    kbo_news_related_ids_collect(out, body);
}

static int kbo_news_link_id_suffix_valid(const char* p, const char* expected_end)
{
    if (p == NULL || expected_end == NULL) {
        return 0;
    }
    int digits = 0;
    while (p < expected_end && *p >= '0' && *p <= '9') {
        digits++;
        p++;
    }
    return digits > 0 && p == expected_end;
}

static const char* kbo_news_link_marker_before_end(
    const char* start,
    const char* end,
    size_t* marker_len)
{
    if (start == NULL || end == NULL || marker_len == NULL) {
        return NULL;
    }
    const char* player = strstr(start, ":player#");
    const char* team = strstr(start, ":team#");
    const char* marker = NULL;
    *marker_len = 0u;
    if (player != NULL && player < end) {
        marker = player;
        *marker_len = 8u;
    }
    if (team != NULL && team < end && (marker == NULL || team < marker)) {
        marker = team;
        *marker_len = 6u;
    }
    if (marker == NULL || marker <= start + 1 || !kbo_news_link_id_suffix_valid(marker + *marker_len, end)) {
        return NULL;
    }
    return marker;
}

static int kbo_news_strip_copy_bytes(char* out, size_t out_size, size_t* out_pos, const char* text, size_t len)
{
    if (out == NULL || out_size == 0u || out_pos == NULL || text == NULL) {
        return 0;
    }
    int complete = 1;
    for (size_t i = 0; i < len; i++) {
        if (*out_pos + 1u < out_size) {
            out[*out_pos] = text[i];
            (*out_pos)++;
        } else {
            complete = 0;
        }
    }
    out[*out_pos < out_size ? *out_pos : out_size - 1u] = '\0';
    return complete;
}

int kbo_news_strip_link_markup(const char* text, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (text == NULL || text[0] == '\0') {
        return 1;
    }

    int complete = 1;
    size_t out_pos = 0u;
    const char* p = text;
    while (*p != '\0') {
        if (*p != '<') {
            complete &= kbo_news_strip_copy_bytes(out, out_size, &out_pos, p, 1u);
            p++;
            continue;
        }

        const char* end = strchr(p, '>');
        size_t marker_len = 0u;
        const char* marker = kbo_news_link_marker_before_end(p, end, &marker_len);
        if (marker == NULL) {
            complete &= kbo_news_strip_copy_bytes(out, out_size, &out_pos, p, 1u);
            p++;
            continue;
        }

        complete &= kbo_news_strip_copy_bytes(out, out_size, &out_pos, p + 1, (size_t)(marker - (p + 1)));
        p = end + 1;
    }
    out[out_pos < out_size ? out_pos : out_size - 1u] = '\0';
    return complete;
}
