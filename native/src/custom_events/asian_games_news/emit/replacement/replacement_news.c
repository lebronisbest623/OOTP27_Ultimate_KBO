#include "../../../runtime/common/custom_events_common.h"
#include "../emit.h"

#include <stdio.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/news/live/core_live_news.h"
#include "../../../../core/news/templates/core_news_templates.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../body/body.h"
#include "../../links/links.h"

static int kbo_asian_games_replacement_news_uses_korean(void)
{
    const char* language_dir = kbo_custom_news_language_dir();
    return language_dir == NULL || strcmp(language_dir, "en") != 0;
}

int kbo_emit_asian_games_replacement_news(
    uint32_t event_yyyymmdd,
    const KboAsianGamesRosterEntry* old_entry,
    const KboAsianGamesRosterEntry* new_entry,
    const char* source)
{
    if (event_yyyymmdd == 0u || old_entry == NULL || new_entry == NULL) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        return 0;
    }

    char old_player[96] = {0};
    char new_player[96] = {0};
    char old_team[128] = {0};
    char new_team[128] = {0};
    kbo_copy_asian_games_player_link(old_entry, old_player, sizeof(old_player));
    kbo_copy_asian_games_player_link(new_entry, new_player, sizeof(new_player));
    kbo_copy_asian_games_team_link(old_entry->original_team_id, old_team, sizeof(old_team));
    kbo_copy_asian_games_team_link(new_entry->original_team_id, new_team, sizeof(new_team));

    int use_korean = kbo_asian_games_replacement_news_uses_korean();
    const char* team_prefix = use_korean ? " / " : " of ";
    char body[2048] = {0};
    char title[160] = {0};
    char roster_size_text[16] = {0};
    char roster_year_text[16] = {0};
    char host_city[64] = {0};
    char host_country[64] = {0};
    char host_place[128] = {0};
    char tournament_label[192] = {0};
    char tournament_phrase[224] = {0};
    snprintf(roster_size_text, sizeof(roster_size_text), "%d", kbo_asian_games_policy_roster_size());
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
    KboNewsTemplateVar vars[] = {
        { "old_player", old_player },
        { "old_team_prefix", old_team[0] != '\0' ? team_prefix : "" },
        { "old_team", old_team },
        { "new_player", new_player },
        { "new_team_prefix", new_team[0] != '\0' ? team_prefix : "" },
        { "new_team", new_team },
        { "roster_size", roster_size_text },
        { "roster_year", roster_year_text },
        { "host_city", host_city },
        { "host_country", host_country },
        { "host_place", host_place },
        { "tournament_label", tournament_label },
        { "tournament_phrase", tournament_phrase },
    };
    if (!kbo_news_template_render_key(
            "asian_games.replacement.title",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            title,
            sizeof(title),
            source)
            || !kbo_news_template_render_key(
                "asian_games.replacement.body",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                source)) {
        kbo_log_runtimef(
            "KBO Asian Games replacement news skipped source=%s date=%u old_player=%u new_player=%u reason=template_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd,
            old_entry->player_id,
            new_entry->player_id);
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
        "KBO Asian Games replacement news source=%s date=%u old_player=%u new_player=%u created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        old_entry->player_id,
        new_entry->player_id,
        created);
    return created;
}

int kbo_emit_asian_games_replacement_news_batch(
    uint32_t event_yyyymmdd,
    const KboAsianGamesRosterEntry* old_entries,
    const KboAsianGamesRosterEntry* new_entries,
    int entry_count,
    const char* source)
{
    if (event_yyyymmdd == 0u || old_entries == NULL || new_entries == NULL || entry_count <= 0) {
        return 0;
    }
    if (entry_count == 1) {
        return kbo_emit_asian_games_replacement_news(
            event_yyyymmdd,
            &old_entries[0],
            &new_entries[0],
            source);
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        return 0;
    }

    int use_korean = kbo_asian_games_replacement_news_uses_korean();
    const char* team_prefix = use_korean ? " / " : " of ";
    char replacement_lines[6144] = {0};
    int rendered_count = 0;
    for (int i = 0; i < entry_count && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        const KboAsianGamesRosterEntry* old_entry = &old_entries[i];
        const KboAsianGamesRosterEntry* new_entry = &new_entries[i];
        if (old_entry->player_id == 0u || new_entry->player_id == 0u) {
            continue;
        }

        char old_player[96] = {0};
        char new_player[96] = {0};
        char old_team[128] = {0};
        char new_team[128] = {0};
        kbo_copy_asian_games_player_link(old_entry, old_player, sizeof(old_player));
        kbo_copy_asian_games_player_link(new_entry, new_player, sizeof(new_player));
        kbo_copy_asian_games_team_link(old_entry->original_team_id, old_team, sizeof(old_team));
        kbo_copy_asian_games_team_link(new_entry->original_team_id, new_team, sizeof(new_team));

        KboNewsTemplateVar line_vars[] = {
            { "old_player", old_player },
            { "old_team_prefix", old_team[0] != '\0' ? team_prefix : "" },
            { "old_team", old_team },
            { "new_player", new_player },
            { "new_team_prefix", new_team[0] != '\0' ? team_prefix : "" },
            { "new_team", new_team },
        };
        char line[512] = {0};
        if (!kbo_news_template_render_key(
                "asian_games.replacement.line",
                line_vars,
                (int)(sizeof(line_vars) / sizeof(line_vars[0])),
                line,
                sizeof(line),
                source)) {
            kbo_log_runtimef(
                "KBO Asian Games replacement batch news skipped source=%s date=%u reason=line_template_unavailable",
                source != NULL ? source : "",
                event_yyyymmdd);
            return 0;
        }
        kbo_news_text_append(replacement_lines, sizeof(replacement_lines), line);
        rendered_count++;
    }

    if (rendered_count <= 0) {
        return 0;
    }

    char title[160] = {0};
    char body[8192] = {0};
    char roster_size_text[16] = {0};
    char replacement_count_text[16] = {0};
    char roster_year_text[16] = {0};
    char host_city[64] = {0};
    char host_country[64] = {0};
    char host_place[128] = {0};
    char tournament_label[192] = {0};
    char tournament_phrase[224] = {0};
    snprintf(roster_size_text, sizeof(roster_size_text), "%d", kbo_asian_games_policy_roster_size());
    snprintf(replacement_count_text, sizeof(replacement_count_text), "%d", rendered_count);
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
    KboNewsTemplateVar vars[] = {
        { "replacement_count", replacement_count_text },
        { "replacement_lines", replacement_lines },
        { "roster_size", roster_size_text },
        { "roster_year", roster_year_text },
        { "host_city", host_city },
        { "host_country", host_country },
        { "host_place", host_place },
        { "tournament_label", tournament_label },
        { "tournament_phrase", tournament_phrase },
    };
    if (!kbo_news_template_render_key(
            "asian_games.replacement.title",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            title,
            sizeof(title),
            source)
            || !kbo_news_template_render_key(
                "asian_games.replacement.body.multi",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                source)) {
        kbo_log_runtimef(
            "KBO Asian Games replacement batch news skipped source=%s date=%u count=%d reason=template_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd,
            rendered_count);
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
        "KBO Asian Games replacement batch news source=%s date=%u count=%d league_id=%u created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        rendered_count,
        league_id,
        created);
    return created;
}
