#include "../../runtime/common/custom_events_common.h"
#include "emit.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/news/live/core_live_news.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../body/body.h"
#include "../links/links.h"

static const char* kbo_asian_games_emit_plural(int value)
{
    return value == 1 ? "" : "s";
}

static void kbo_asian_games_emit_u32_text(uint32_t value, char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        snprintf(out, out_size, "%u", value);
    }
}

int kbo_emit_asian_games_news(uint32_t event_yyyymmdd, const char* template_prefix, const char* source)
{
    if (event_yyyymmdd == 0u || template_prefix == NULL || template_prefix[0] == '\0') {
        return 0;
    }

    char title_key[128] = {0};
    char lead_key[128] = {0};
    char title[160] = {0};
    char lead[256] = {0};
    snprintf(title_key, sizeof(title_key), "%s.title", template_prefix);
    snprintf(lead_key, sizeof(lead_key), "%s.lead", template_prefix);
    int is_final_gold = strcmp(template_prefix, "asian_games.final") == 0;
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
    char roster_count_text[16] = {0};
    char wildcards_text[16] = {0};
    char departed_text[16] = {0};
    char returned_text[16] = {0};
    char exempted_text[16] = {0};
    char exemption_candidates_text[16] = {0};

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
    kbo_asian_games_emit_u32_text((uint32_t)roster_count, roster_count_text, sizeof(roster_count_text));
    kbo_asian_games_emit_u32_text((uint32_t)wildcards, wildcards_text, sizeof(wildcards_text));
    kbo_asian_games_emit_u32_text((uint32_t)departed, departed_text, sizeof(departed_text));
    kbo_asian_games_emit_u32_text((uint32_t)returned, returned_text, sizeof(returned_text));
    kbo_asian_games_emit_u32_text((uint32_t)exempted, exempted_text, sizeof(exempted_text));
    kbo_asian_games_emit_u32_text(
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
    KboNewsTemplateVar vars[] = {
        { "roster_count", roster_count_text },
        { "wildcards", wildcards_text },
        { "wildcards_plural", kbo_asian_games_emit_plural(wildcards) },
        { "departed", departed_text },
        { "departed_plural", kbo_asian_games_emit_plural(departed) },
        { "returned", returned_text },
        { "returned_plural", kbo_asian_games_emit_plural(returned) },
        { "exempted", exempted_text },
        { "exempted_plural", kbo_asian_games_emit_plural(exempted) },
        { "exemption_candidates", exemption_candidates_text },
        { "exemption_candidates_plural", kbo_asian_games_emit_plural(exemption_candidates) },
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
    if (!kbo_news_template_render_key(title_key, vars, (int)(sizeof(vars) / sizeof(vars[0])), title, sizeof(title), source)
            || !kbo_news_template_render_key(lead_key, vars, (int)(sizeof(vars) / sizeof(vars[0])), lead, sizeof(lead), source)) {
        kbo_log_runtimef(
            "KBO Asian Games news skipped source=%s template=%s reason=template_unavailable",
            source != NULL ? source : "",
            template_prefix);
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        kbo_log_runtimef(
            "KBO Asian Games news skipped source=%s title=%s reason=league_id_unavailable",
            source != NULL ? source : "",
            title);
        return 0;
    }

    char body[8192] = {0};
    kbo_build_asian_games_news_body(body, sizeof(body), event_yyyymmdd, template_prefix, lead, source);
    if (body[0] == '\0') {
        kbo_log_runtimef(
            "KBO Asian Games news skipped source=%s template=%s title=%s reason=body_template_unavailable",
            source != NULL ? source : "",
            template_prefix,
            title);
        return 0;
    }
    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    kbo_log_runtimef(
        "KBO Asian Games news source=%s template=%s title=%s date=%u league_id=%u created=%d",
        source != NULL ? source : "",
        template_prefix,
        title,
        event_yyyymmdd,
        league_id,
        created);
    return created;
}

uint32_t kbo_asian_games_effective_action_date(uint32_t event_yyyymmdd)
{
    return kbo_custom_event_effective_news_date(event_yyyymmdd);
}

