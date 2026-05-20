#include "../internal/intl_established_fa_postscan_internal.h"
#include "../api/intl_established_fa_postscan.h"

#include "../../../core/news/templates/core_news_templates.h"
#include "../../../core/news/live/core_live_news.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../custom_events/runtime/dates/custom_event_dates.h"
#include "../../../team/names/team_name_cache.h"

#define KBO_INTL_FA_NEWS_TOP_MAX 8

typedef struct KboIntlFaNewsCandidate {
    uint32_t player_id;
    uint32_t nation_id;
    uint8_t position_group;
    uint8_t position_role;
    uint16_t age;
    int asian_quota;
    int32_t score;
    char player_name[96];
} KboIntlFaNewsCandidate;

static const char* kbo_intl_fa_position_label(uint8_t position_group, uint8_t position_role)
{
    if (position_group == 1u) {
        if (position_role == 11u) {
            return "SP";
        }
        return "RP";
    }
    switch (position_role) {
    case 2u: return "C";
    case 3u: return "1B";
    case 4u: return "2B";
    case 5u: return "3B";
    case 6u: return "SS";
    case 7u: return "LF";
    case 8u: return "CF";
    case 9u: return "RF";
    case 10u: return "DH";
    default: return "B";
    }
}

static void kbo_intl_fa_candidate_fill(KboIntlFaNewsCandidate* out, uint8_t* player)
{
    memset(out, 0, sizeof(*out));
    out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    out->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    out->position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    out->position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);
    out->age = (uint16_t)(*(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET) & 0xffffu);
    out->asian_quota = kbo_nation_is_asian_quota_candidate(out->nation_id);
    out->score = kbo_foreign_waiver_value_score(player);
    kbo_copy_player_display_name(player, out->player_name, sizeof(out->player_name));
    if (out->player_name[0] == '\0' || strcmp(out->player_name, "Unknown player") == 0) {
        snprintf(out->player_name, sizeof(out->player_name), "Player #%u", out->player_id);
    }
}

static void kbo_intl_fa_candidate_insert_sorted(
    KboIntlFaNewsCandidate* top,
    int* top_count,
    const KboIntlFaNewsCandidate* candidate)
{
    if (top == NULL || top_count == NULL || candidate == NULL || candidate->player_id == 0u) {
        return;
    }

    int slot = *top_count;
    if (slot < KBO_INTL_FA_NEWS_TOP_MAX) {
        (*top_count)++;
    } else if (candidate->score <= top[KBO_INTL_FA_NEWS_TOP_MAX - 1].score) {
        return;
    } else {
        slot = KBO_INTL_FA_NEWS_TOP_MAX - 1;
    }

    while (slot > 0 && candidate->score > top[slot - 1].score) {
        top[slot] = top[slot - 1];
        slot--;
    }
    top[slot] = *candidate;
}

static int kbo_intl_fa_collect_news_candidates(
    const KboIntlEstablishedFaPostscanState* batch,
    KboIntlFaNewsCandidate* top,
    int* out_top_count,
    int* out_total)
{
    if (out_top_count != NULL) {
        *out_top_count = 0;
    }
    if (out_total != NULL) {
        *out_total = 0;
    }
    if (batch == NULL || top == NULL || out_top_count == NULL || out_total == NULL) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_intl_established_fa_postscan_candidate_matches(batch, i, player_count, player)) {
            continue;
        }
        if (!kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint8_t retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
        uint8_t draft_eligible = player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET];
        if (current_team_id != 0u || active_team_id != 0u || retired_flag != 0u || draft_eligible != 0u) {
            continue;
        }

        KboIntlFaNewsCandidate candidate;
        kbo_intl_fa_candidate_fill(&candidate, player);
        (*out_total)++;
        kbo_intl_fa_candidate_insert_sorted(top, out_top_count, &candidate);
    }

    return 1;
}

static void kbo_intl_fa_build_list(const KboIntlFaNewsCandidate* top, int top_count, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (top == NULL || top_count <= 0) {
        kbo_news_template_render_key("intl_established_fa.none", NULL, 0, out, out_size, "intl_established_fa_news");
        if (out[0] == '\0') {
            snprintf(out, out_size, "- No listed players");
        }
        return;
    }

    for (int i = 0; i < top_count; i++) {
        char player_link[160] = {0};
        char line[256] = {0};
        char age[16] = {0};
        char score[16] = {0};
        snprintf(player_link, sizeof(player_link), "<%s:player#%u>", top[i].player_name, top[i].player_id);
        snprintf(age, sizeof(age), "%u", (unsigned)top[i].age);
        snprintf(score, sizeof(score), "%d", top[i].score);
        const char* lang = kbo_custom_news_language_dir();
        const char* quota = top[i].asian_quota
            ? (lang != NULL && strcmp(lang, "en") == 0 ? "Asian" : "아시아쿼터")
            : (lang != NULL && strcmp(lang, "en") == 0 ? "Non-Asian" : "비아시아");
        KboNewsTemplateVar vars[] = {
            { "rank", "" },
            { "player_link", player_link },
            { "position", kbo_intl_fa_position_label(top[i].position_group, top[i].position_role) },
            { "age", age },
            { "quota", quota },
            { "score", score }
        };
        char rank[12] = {0};
        snprintf(rank, sizeof(rank), "%d", i + 1);
        vars[0].value = rank;
        if (!kbo_news_template_render_key(
                "intl_established_fa.player_line",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                line,
                sizeof(line),
                "intl_established_fa_news")) {
            snprintf(line, sizeof(line), "%d. %s, %s, age %s, %s", i + 1, player_link, kbo_intl_fa_position_label(top[i].position_group, top[i].position_role), age, quota);
        }
        kbo_news_text_append(out, out_size, line);
        if (!kbo_news_text_ends_with_newline(out)) {
            kbo_news_text_append(out, out_size, "\n");
        }
    }
}

int kbo_handle_intl_established_fa_event(uint32_t event_yyyymmdd, const char* source)
{
    if (!kbo_fix_enabled() || event_yyyymmdd == 0u) {
        return 0;
    }

    uint32_t today = 0u;
    if (kbo_current_date_tick_latest_published_date(&today)
            && kbo_intl_established_fa_event_is_stale(event_yyyymmdd, today)) {
        kbo_log_runtimef(
            "international established FA event skipped stale source=%s event_date=%u today=%u reason=missed_event_day",
            source != NULL ? source : "",
            event_yyyymmdd,
            today);
        return 1;
    }

    LONG pending = InterlockedCompareExchange(&g_kbo_intl_established_fa_postscan.pending, 0, 0);
    if (pending != KBO_INTL_FA_POSTSCAN_IDLE) {
        if (pending == KBO_INTL_FA_POSTSCAN_PENDING
                && kbo_intl_established_fa_postscan_run_pending_now(event_yyyymmdd, source)) {
            pending = KBO_INTL_FA_POSTSCAN_IDLE;
        } else {
            kbo_log_runtimef(
                "international established FA event deferred source=%s date=%u reason=postscan_pending pending=%ld",
                source != NULL ? source : "",
                event_yyyymmdd,
                (long)pending);
            return 0;
        }
    }

    KboIntlEstablishedFaPostscanState batch = g_kbo_intl_established_fa_postscan;
    int loaded_batch = 0;
    if (!kbo_intl_established_fa_postscan_batch_matches_event_date(&batch, event_yyyymmdd)) {
        KboIntlEstablishedFaPostscanState persisted = {0};
        if (kbo_intl_established_fa_postscan_load_state(&persisted, source)
                && kbo_intl_established_fa_postscan_batch_matches_event_date(&persisted, event_yyyymmdd)) {
            batch = persisted;
            g_kbo_intl_established_fa_postscan = persisted;
            loaded_batch = 1;
        }
    }
    if (!kbo_intl_established_fa_postscan_batch_matches_event_date(&batch, event_yyyymmdd)) {
        kbo_log_runtimef(
            "international established FA event deferred source=%s date=%u reason=batch_unavailable scheduled=%u expected=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            batch.scheduled_date,
            batch.expected_count);
        return 0;
    }
    if (loaded_batch) {
        kbo_log_runtimef(
            "international established FA event loaded postscan state source=%s event_date=%u generation_date=%u expected=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            batch.scheduled_date,
            batch.expected_count);
    }

    KboIntlFaNewsCandidate top[KBO_INTL_FA_NEWS_TOP_MAX];
    memset(top, 0, sizeof(top));
    int top_count = 0;
    int total = 0;
    if (!kbo_intl_fa_collect_news_candidates(&batch, top, &top_count, &total)) {
        return 0;
    }

    char season[16] = {0};
    char total_text[16] = {0};
    char expected_text[16] = {0};
    char multiplier_text[16] = {0};
    char player_list[2400] = {0};
    snprintf(season, sizeof(season), "%u", event_yyyymmdd / 10000u);
    snprintf(total_text, sizeof(total_text), "%d", total);
    snprintf(expected_text, sizeof(expected_text), "%d", batch.expected_count);
    snprintf(multiplier_text, sizeof(multiplier_text), "%d", batch.multiplier);
    kbo_intl_fa_build_list(top, top_count, player_list, sizeof(player_list));

    KboNewsTemplateVar vars[] = {
        { "season", season },
        { "total_count", total_text },
        { "expected_count", expected_text },
        { "multiplier", multiplier_text },
        { "player_list", player_list }
    };

    char title[256] = {0};
    char body[4096] = {0};
    if (!kbo_news_template_render_key(
            "intl_established_fa.summary.title",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            title,
            sizeof(title),
            source != NULL ? source : "intl_established_fa_event")
            || !kbo_news_template_render_key(
                "intl_established_fa.summary.body",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                source != NULL ? source : "intl_established_fa_event")) {
        snprintf(title, sizeof(title), "%u International FA Class Generated", event_yyyymmdd / 10000u);
        snprintf(body, sizeof(body), "A new international established FA class is available.\n\n%s", player_list);
    }

    uint32_t news_date = kbo_custom_event_effective_news_date(event_yyyymmdd);
    uint32_t league_id = batch.primary_league_id != 0u ? batch.primary_league_id : batch.fallback_league_id;
    if (league_id == 0u) {
        league_id = OOTP27_KBO_MAIN_LEAGUE_ID;
    }
    create_kbo_native_live_news_with_body(
        news_date / 10000u,
        (news_date / 100u) % 100u,
        news_date % 100u,
        league_id,
        3u,
        title,
        body);

    kbo_log_runtimef(
        "international established FA event news source=%s date=%u generation=%u news=%u league=%u total=%d top=%d expected=%d multiplier=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        batch.scheduled_date,
        news_date,
        league_id,
        total,
        top_count,
        batch.expected_count,
        batch.multiplier);
    return 1;
}
