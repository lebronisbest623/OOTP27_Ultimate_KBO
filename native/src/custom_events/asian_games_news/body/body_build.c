#include "../../runtime/common/custom_events_common.h"
#include "body.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../asian_games/player_eval/asian_games_player_eval.h"

static const char* kbo_asian_games_plural(int value)
{
    return value == 1 ? "" : "s";
}

static int kbo_asian_games_news_build_uses_korean(void)
{
    const char* language_dir = kbo_custom_news_language_dir();
    return language_dir == NULL || strcmp(language_dir, "en") != 0;
}

static void kbo_asian_games_u32_text(uint32_t value, char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        snprintf(out, out_size, "%u", value);
    }
}

static int kbo_asian_games_role_group_index(uint8_t role)
{
    if (kbo_asian_games_role_is_pitcher(role)) {
        return 0;
    }
    if (kbo_asian_games_role_is_catcher(role)) {
        return 1;
    }
    if (kbo_asian_games_role_is_infielder(role)) {
        return 2;
    }
    if (kbo_asian_games_role_is_outfielder(role)) {
        return 3;
    }
    return 4;
}

static const char* kbo_asian_games_role_group_label(int group_index, int use_korean)
{
    if (use_korean) {
        switch (group_index) {
        case 0: return "\xed\x88\xac\xec\x88\x98";
        case 1: return "\xed\x8f\xac\xec\x88\x98";
        case 2: return "\xeb\x82\xb4\xec\x95\xbc\xec\x88\x98";
        case 3: return "\xec\x99\xb8\xec\x95\xbc\xec\x88\x98";
        default: return "\xea\xb8\xb0\xed\x83\x80";
        }
    }
    switch (group_index) {
    case 0: return "Pitchers";
    case 1: return "Catchers";
    case 2: return "Infielders";
    case 3: return "Outfielders";
    default: return "Other";
    }
}

static int kbo_asian_games_append_roster_group_header(
    char* out,
    size_t out_size,
    size_t* used,
    int group_index)
{
    if (out == NULL || out_size == 0 || used == NULL || *used >= out_size - 1) {
        return 0;
    }

    int use_korean = kbo_asian_games_news_build_uses_korean();
    KboNewsTemplateVar vars[] = {
        { "role_group", kbo_asian_games_role_group_label(group_index, use_korean) },
    };
    char rendered[96] = {0};
    if (!kbo_news_template_render_key(
            "asian_games.roster_group_header",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            rendered,
            sizeof(rendered),
            "asian_games_body")) {
        return 0;
    }
    kbo_news_text_append(out, out_size, rendered);
    *used = strlen(out);
    return 1;
}

void kbo_build_asian_games_news_body(
    char* out,
    size_t out_size,
    uint32_t event_yyyymmdd,
    const char* template_prefix,
    const char* lead,
    const char* source)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }

    int departed = 0;
    int returned = 0;
    int exempted = 0;
    int exemption_candidates = 0;
    int wildcards = 0;
    for (LONG i = 0; i < roster_count; i++) {
        if (g_kbo_asian_games_roster[i].departed) { departed++; }
        if (g_kbo_asian_games_roster[i].returned) { returned++; }
        if (g_kbo_asian_games_roster[i].exempted) { exempted++; }
        if (g_kbo_asian_games_roster[i].military_unserved) { exemption_candidates++; }
        if (g_kbo_asian_games_roster[i].wildcard) { wildcards++; }
    }
    KboAsianGamesRosterEntry* captain = kbo_asian_games_choose_captain();
    char captain_link[96] = {0};
    if (captain != NULL) {
        kbo_copy_asian_games_player_link(captain, captain_link, sizeof(captain_link));
    }
    int is_selection = template_prefix != NULL && strcmp(template_prefix, "asian_games.selection") == 0;
    int is_departure = template_prefix != NULL && strcmp(template_prefix, "asian_games.departure") == 0;
    int is_final_gold = template_prefix != NULL && strcmp(template_prefix, "asian_games.final") == 0;
    int is_final_failure = template_prefix != NULL && strcmp(template_prefix, "asian_games.final.failure") == 0;
    int is_final = is_final_gold || is_final_failure;
    int include_roster = is_selection;
    int include_samples = 0;

    kbo_news_text_append(out, out_size, lead != NULL && lead[0] != '\0' ? lead : "");
    if (out[0] != '\0') {
        kbo_news_text_append(out, out_size, " ");
    }

    size_t used = strlen(out);
    (void)used;

    char roster_count_text[16] = {0};
    char wildcards_text[16] = {0};
    char departed_text[16] = {0};
    char returned_text[16] = {0};
    char exempted_text[16] = {0};
    char exemption_candidates_text[16] = {0};
    char roster_year_text[16] = {0};
    char host_city[64] = {0};
    char host_country[64] = {0};
    char host_place[128] = {0};
    char tournament_label[192] = {0};
    char tournament_phrase[224] = {0};
    char final_opponent[64] = {0};
    char final_score[16] = {0};
    char korea_score[16] = {0};
    char opponent_score[16] = {0};
    kbo_asian_games_u32_text((uint32_t)roster_count, roster_count_text, sizeof(roster_count_text));
    kbo_asian_games_u32_text((uint32_t)wildcards, wildcards_text, sizeof(wildcards_text));
    kbo_asian_games_u32_text((uint32_t)departed, departed_text, sizeof(departed_text));
    kbo_asian_games_u32_text((uint32_t)returned, returned_text, sizeof(returned_text));
    kbo_asian_games_u32_text((uint32_t)exempted, exempted_text, sizeof(exempted_text));
    kbo_asian_games_u32_text(
        (uint32_t)exemption_candidates,
        exemption_candidates_text,
        sizeof(exemption_candidates_text));
    kbo_asian_games_build_news_context(
        event_yyyymmdd,
        roster_year_text,
        sizeof(roster_year_text),
        host_city,
        sizeof(host_city),
        host_country,
        sizeof(host_country),
        host_place,
        sizeof(host_place),
        tournament_label,
        sizeof(tournament_label),
        tournament_phrase,
        sizeof(tournament_phrase));
    kbo_asian_games_build_final_matchup(
        event_yyyymmdd,
        is_final_gold,
        final_opponent,
        sizeof(final_opponent),
        final_score,
        sizeof(final_score),
        korea_score,
        sizeof(korea_score),
        opponent_score,
        sizeof(opponent_score));

    KboNewsTemplateVar body_vars[] = {
        { "roster_count", roster_count_text },
        { "wildcards", wildcards_text },
        { "wildcards_plural", kbo_asian_games_plural(wildcards) },
        { "departed", departed_text },
        { "departed_plural", kbo_asian_games_plural(departed) },
        { "returned", returned_text },
        { "returned_plural", kbo_asian_games_plural(returned) },
        { "exempted", exempted_text },
        { "exempted_plural", kbo_asian_games_plural(exempted) },
        { "exemption_candidates", exemption_candidates_text },
        { "exemption_candidates_plural", kbo_asian_games_plural(exemption_candidates) },
        { "roster_year", roster_year_text },
        { "host_city", host_city },
        { "host_country", host_country },
        { "host_place", host_place },
        { "tournament_label", tournament_label },
        { "tournament_phrase", tournament_phrase },
        { "final_opponent", final_opponent },
        { "final_score", final_score },
        { "korea_score", korea_score },
        { "opponent_score", opponent_score },
    };
    const char* body_key = is_selection
        ? "asian_games.selection.body"
        : (is_departure
            ? "asian_games.departure.body"
            : (is_final_failure
                ? "asian_games.final.failure.body"
                : (is_final ? "asian_games.final.body" : "asian_games.default.body")));
    char rendered[2048] = {0};
    if (!kbo_news_template_render_key(
            body_key,
            body_vars,
            (int)(sizeof(body_vars) / sizeof(body_vars[0])),
            rendered,
            sizeof(rendered),
            source)) {
        out[0] = '\0';
        return;
    }
    kbo_news_text_append(out, out_size, rendered);
    used = strlen(out);

    if (include_samples) {
        LONG sample_count = roster_count < 6 ? roster_count : 6;
        for (LONG i = 0; i < sample_count && used + 160u < out_size; i++) {
            LONG roster_index = (i == sample_count - 1 && roster_count > sample_count)
                ? roster_count - 1
                : i;
            kbo_asian_games_append_player_blurb(
                out,
                out_size,
                &used,
                &g_kbo_asian_games_roster[roster_index],
                i,
                sample_count);
        }
    }

    if (used < out_size - 1) {
        KboNewsTemplateVar suffix_vars[] = {
            { "period", include_samples ? "." : "" },
            { "roster_year", roster_year_text },
            { "host_city", host_city },
            { "host_country", host_country },
            { "host_place", host_place },
            { "tournament_label", tournament_label },
            { "tournament_phrase", tournament_phrase },
        };
        char suffix[512] = {0};
        if (kbo_news_template_render_key(
                "asian_games.samples_suffix",
                suffix_vars,
                (int)(sizeof(suffix_vars) / sizeof(suffix_vars[0])),
                suffix,
                sizeof(suffix),
                source)) {
            kbo_news_text_append(out, out_size, suffix);
            used = strlen(out);
        }
    }

    if (include_roster && captain_link[0] != '\0' && used < out_size - 1) {
        KboNewsTemplateVar quote_vars[] = {
            { "captain_link", captain_link },
        };
        char quote[768] = {0};
        if (kbo_news_template_render_key(
                "asian_games.captain_quote",
                quote_vars,
                (int)(sizeof(quote_vars) / sizeof(quote_vars[0])),
                quote,
                sizeof(quote),
                source)) {
            kbo_news_text_append(out, out_size, quote);
            used = strlen(out);
        }
    }

    if (include_roster && used < out_size - 1) {
        char header[128] = {0};
        if (kbo_news_template_load("asian_games.complete_roster_header", header, sizeof(header), NULL, 0u, source)) {
            kbo_news_text_append(out, out_size, header);
            used = strlen(out);
        }
    }

    if (include_roster) {
        for (int group = 0; group < 5 && used + 140u < out_size; group++) {
            int group_count = 0;
            for (LONG i = 0; i < roster_count; i++) {
                if (kbo_asian_games_role_group_index(g_kbo_asian_games_roster[i].role) == group) {
                    group_count++;
                }
            }
            if (group_count <= 0) {
                continue;
            }
            if (!kbo_asian_games_append_roster_group_header(out, out_size, &used, group)) {
                continue;
            }
            LONG listed_in_group = 0;
            for (LONG i = 0; i < roster_count && used + 140u < out_size; i++) {
                if (kbo_asian_games_role_group_index(g_kbo_asian_games_roster[i].role) != group) {
                    continue;
                }
                kbo_asian_games_append_roster_line(
                    out,
                    out_size,
                    &used,
                    listed_in_group,
                    &g_kbo_asian_games_roster[i]);
                listed_in_group++;
            }
        }
    }
}
