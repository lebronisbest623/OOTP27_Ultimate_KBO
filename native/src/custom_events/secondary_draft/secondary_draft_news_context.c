#include "secondary_draft_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../core/news/templates/core_news_templates.h"

#define KBO_SECONDARY_DRAFT_NEWS_TEAM_LINE_LIMIT 3

typedef struct KboSecondaryDraftNewsTeamTally {
    uint32_t team_id;
    int count;
    char team_name[96];
} KboSecondaryDraftNewsTeamTally;

static const char* kbo_secondary_draft_news_name_or_fallback(
    const char* name,
    const char* fallback)
{
    return (name != NULL && name[0] != '\0') ? name : fallback;
}

static void kbo_secondary_draft_news_format_link(
    const char* name,
    const char* type,
    uint32_t id,
    const char* fallback,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    const char* label = kbo_secondary_draft_news_name_or_fallback(name, fallback);
    if (id == 0u || type == NULL || type[0] == '\0') {
        snprintf(out, out_size, "%s", label);
        return;
    }
    snprintf(out, out_size, "<%s:%s#%u>", label, type, id);
}

static void kbo_secondary_draft_news_add_team_tally(
    KboSecondaryDraftNewsTeamTally* tallies,
    int* tally_count,
    int max_tallies,
    uint32_t team_id,
    const char* team_name)
{
    if (tallies == NULL || tally_count == NULL || max_tallies <= 0) {
        return;
    }
    for (int i = 0; i < *tally_count; i++) {
        if (tallies[i].team_id == team_id) {
            tallies[i].count++;
            return;
        }
    }
    if (*tally_count >= max_tallies) {
        return;
    }
    KboSecondaryDraftNewsTeamTally* tally = &tallies[*tally_count];
    tally->team_id = team_id;
    tally->count = 1;
    snprintf(
        tally->team_name,
        sizeof(tally->team_name),
        "%s",
        kbo_secondary_draft_news_name_or_fallback(team_name, "Team"));
    (*tally_count)++;
}

static void kbo_secondary_draft_news_append_top_teams(
    const KboSecondaryDraftNewsTeamTally* tallies,
    int tally_count,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (tallies == NULL || tally_count <= 0) {
        kbo_news_text_append(out, out_size, "- none");
        return;
    }

    int used[KBO_SECONDARY_DRAFT_TEAM_MAX] = {0};
    int emitted = 0;
    while (emitted < KBO_SECONDARY_DRAFT_NEWS_TEAM_LINE_LIMIT && emitted < tally_count) {
        int best = -1;
        for (int i = 0; i < tally_count; i++) {
            if (used[i]) {
                continue;
            }
            if (best < 0 || tallies[i].count > tallies[best].count) {
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        used[best] = 1;
        char team_link[160] = {0};
        kbo_secondary_draft_news_format_link(
            tallies[best].team_name,
            "team",
            tallies[best].team_id,
            "Team",
            team_link,
            sizeof(team_link));
        kbo_news_text_appendf(out, out_size, "- %s / %d\n", team_link, tallies[best].count);
        emitted++;
    }
}

static void kbo_secondary_draft_news_build_round_lines(
    const int* round_counts,
    int max_round,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    int emitted = 0;
    for (int round = 1; round <= max_round; round++) {
        if (round_counts[round] <= 0) {
            continue;
        }
        kbo_news_text_appendf(out, out_size, "- R%d / %d\n", round, round_counts[round]);
        emitted++;
    }
    if (emitted == 0) {
        kbo_news_text_append(out, out_size, "- none");
    }
}

void kbo_secondary_draft_build_news_context(
    const KboSecondaryDraftPick* picks,
    int pick_count,
    KboSecondaryDraftNewsContext* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (picks == NULL || pick_count <= 0) {
        kbo_news_text_append(out->lead_pick_line, sizeof(out->lead_pick_line), "- none");
        kbo_news_text_append(out->round_summary_lines, sizeof(out->round_summary_lines), "- none");
        kbo_news_text_append(out->incoming_team_lines, sizeof(out->incoming_team_lines), "- none");
        kbo_news_text_append(out->source_team_lines, sizeof(out->source_team_lines), "- none");
        return;
    }

    char player_link[160] = {0};
    char from_link[160] = {0};
    char to_link[160] = {0};
    kbo_secondary_draft_news_format_link(
        picks[0].player_name,
        "player",
        picks[0].player_id,
        "Player",
        player_link,
        sizeof(player_link));
    kbo_secondary_draft_news_format_link(
        picks[0].from_team_name,
        "team",
        picks[0].from_team_id,
        "Team",
        from_link,
        sizeof(from_link));
    kbo_secondary_draft_news_format_link(
        picks[0].to_team_name,
        "team",
        picks[0].to_team_id,
        "Team",
        to_link,
        sizeof(to_link));
    kbo_news_text_appendf(
        out->lead_pick_line,
        sizeof(out->lead_pick_line),
        "- R%u #%u / %s / %s -> %s",
        picks[0].round,
        picks[0].pick_no,
        player_link,
        from_link,
        to_link);

    int round_counts[KBO_SECONDARY_DRAFT_ROUNDS + 1] = {0};
    KboSecondaryDraftNewsTeamTally incoming[KBO_SECONDARY_DRAFT_TEAM_MAX] = {0};
    KboSecondaryDraftNewsTeamTally source[KBO_SECONDARY_DRAFT_TEAM_MAX] = {0};
    int incoming_count = 0;
    int source_count = 0;
    for (int i = 0; i < pick_count; i++) {
        if (picks[i].round > 0u && picks[i].round <= KBO_SECONDARY_DRAFT_ROUNDS) {
            round_counts[picks[i].round]++;
        }
        kbo_secondary_draft_news_add_team_tally(
            incoming,
            &incoming_count,
            KBO_SECONDARY_DRAFT_TEAM_MAX,
            picks[i].to_team_id,
            picks[i].to_team_name);
        kbo_secondary_draft_news_add_team_tally(
            source,
            &source_count,
            KBO_SECONDARY_DRAFT_TEAM_MAX,
            picks[i].from_team_id,
            picks[i].from_team_name);
    }

    kbo_secondary_draft_news_build_round_lines(
        round_counts,
        KBO_SECONDARY_DRAFT_ROUNDS,
        out->round_summary_lines,
        sizeof(out->round_summary_lines));
    kbo_secondary_draft_news_append_top_teams(
        incoming,
        incoming_count,
        out->incoming_team_lines,
        sizeof(out->incoming_team_lines));
    kbo_secondary_draft_news_append_top_teams(
        source,
        source_count,
        out->source_team_lines,
        sizeof(out->source_team_lines));
}
