#include "../internal/foreign_injury_internal.h"

static int kbo_foreign_injury_ascii_lower(int ch)
{
    return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch;
}

static int kbo_foreign_injury_ascii_starts_with_nocase(const char* text, const char* prefix)
{
    if (text == NULL || prefix == NULL) {
        return 0;
    }
    while (*prefix != '\0') {
        if (kbo_foreign_injury_ascii_lower((unsigned char)*text)
                != kbo_foreign_injury_ascii_lower((unsigned char)*prefix)) {
            return 0;
        }
        ++text;
        ++prefix;
    }
    return 1;
}

static int kbo_foreign_injury_ascii_contains_nocase(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    for (const char* p = text; *p != '\0'; ++p) {
        if (kbo_foreign_injury_ascii_starts_with_nocase(p, needle)) {
            return 1;
        }
    }
    return 0;
}

static const char* kbo_foreign_injury_ascii_match_unit(const char* text, const char* unit)
{
    if (text == NULL || unit == NULL) {
        return NULL;
    }
    while (*unit != '\0') {
        if (kbo_foreign_injury_ascii_lower((unsigned char)*text)
                != kbo_foreign_injury_ascii_lower((unsigned char)*unit)) {
            return NULL;
        }
        ++text;
        ++unit;
    }
    return text;
}

static int kbo_foreign_injury_ascii_unit_followed_by_old(const char* unit_end)
{
    if (unit_end == NULL) {
        return 0;
    }
    const char* p = unit_end;
    if (*p == 's' || *p == 'S') {
        ++p;
    }
    while (*p == ' ' || *p == '\t' || *p == '-' || *p == '\r' || *p == '\n') {
        ++p;
    }
    return kbo_foreign_injury_ascii_starts_with_nocase(p, "old");
}

int kbo_foreign_injury_duration_text_meets_minimum(
    const char* text,
    int min_days,
    int* out_days)
{
    if (out_days != NULL) {
        *out_days = 0;
    }
    if (text == NULL || min_days <= 0) {
        return 0;
    }

    int best_days = 0;
    if (kbo_foreign_injury_ascii_contains_nocase(text, "season is officially over")
            || kbo_foreign_injury_ascii_contains_nocase(text, "season is over")
            || kbo_foreign_injury_ascii_contains_nocase(text, "out for the season")
            || kbo_foreign_injury_ascii_contains_nocase(text, "miss the rest of the season")
            || kbo_foreign_injury_ascii_contains_nocase(text, "misses the rest of the season")
            || kbo_foreign_injury_ascii_contains_nocase(text, "out the rest of the year")
            || kbo_foreign_injury_ascii_contains_nocase(text, "miss the rest of the year")
            || kbo_foreign_injury_ascii_contains_nocase(text, "misses the rest of the year")
            || kbo_foreign_injury_ascii_contains_nocase(text, "season-ending")) {
        best_days = min_days > 180 ? min_days : 180;
    }
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            continue;
        }

        int value = 0;
        const char* n = p;
        while (*n >= '0' && *n <= '9') {
            value = value * 10 + (*n - '0');
            ++n;
            if (value > 10000) {
                value = 10000;
                break;
            }
        }
        p = n - 1;
        while (*n == ' ' || *n == '\t' || *n == '\r' || *n == '\n') {
            ++n;
        }

        int days = 0;
        const char* unit_end = NULL;
        if ((unit_end = kbo_foreign_injury_ascii_match_unit(n, "day")) != NULL) {
            days = value;
        } else if ((unit_end = kbo_foreign_injury_ascii_match_unit(n, "week")) != NULL) {
            days = value * 7;
        } else if ((unit_end = kbo_foreign_injury_ascii_match_unit(n, "month")) != NULL) {
            days = value * 30;
        } else if ((unit_end = kbo_foreign_injury_ascii_match_unit(n, "year")) != NULL) {
            days = value * 365;
        }

        if (days > 0 && kbo_foreign_injury_ascii_unit_followed_by_old(unit_end)) {
            days = 0;
        }
        if (days > best_days) {
            best_days = days;
        }
    }

    if (out_days != NULL) {
        *out_days = best_days;
    }
    return best_days >= min_days;
}
