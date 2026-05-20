#include "foreign_injury_replacement_news_internal.h"

#include "../../common/policy/foreign_player_policy.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/news/templates/core_news_templates.h"

static int kbo_foreign_injury_days_until(uint32_t from_yyyymmdd, uint32_t to_yyyymmdd)
{
    if (from_yyyymmdd == 0u || to_yyyymmdd == 0u) {
        return 0;
    }

    uint32_t from_serial = kbo_date_serial(
        from_yyyymmdd / 10000u,
        (from_yyyymmdd / 100u) % 100u,
        from_yyyymmdd % 100u);
    uint32_t to_serial = kbo_date_serial(
        to_yyyymmdd / 10000u,
        (to_yyyymmdd / 100u) % 100u,
        to_yyyymmdd % 100u);
    if (from_serial == 0u || to_serial == 0u || to_serial <= from_serial) {
        return 0;
    }

    uint32_t diff = to_serial - from_serial;
    return diff > 32767u ? 32767 : (int)diff;
}

void kbo_emit_foreign_injury_replacement_news(
    const KboForeignInjuryReplacement* rec,
    int days_left,
    const char* phase)
{
    uint32_t event_date = 0u;
    if (!kbo_current_date_tick_latest_published_date(&event_date) || event_date == 0u) {
        event_date = rec != NULL ? rec->opened_on_yyyymmdd : 0u;
    }
    kbo_emit_foreign_injury_replacement_news_on_date(rec, days_left, phase, event_date);
}

void kbo_emit_foreign_injury_replacement_news_on_date(
    const KboForeignInjuryReplacement* rec,
    int days_left,
    const char* phase,
    uint32_t event_date)
{
    if (rec == NULL || rec->team_id == 0u || rec->injured_player_id == 0u || rec->league_id == 0u) {
        return;
    }

    if (event_date == 0u) {
        event_date = rec->opened_on_yyyymmdd;
    }
    if (event_date == 0u) {
        return;
    }

    char player_name[96] = {0};
    uint8_t* player = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
    if (player != NULL) {
        kbo_copy_player_display_name(player, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", rec->injured_player_id);
    }

    char title[180] = {0};
    char body[2048] = {0};
    char team_link[96] = {0};
    char injured_player_link[144] = {0};
    char replacement_player_link[144] = {0};
    char replacement_player_name[96] = {0};
    char retained_player_link[144] = {0};
    char retained_player_name[96] = {0};
    char released_player_link[144] = {0};
    char released_player_name[96] = {0};
    char days_left_text[16] = {0};
    kbo_foreign_injury_copy_team_link(rec->team_id, team_link, sizeof(team_link));
    snprintf(injured_player_link, sizeof(injured_player_link), "<%s:player#%u>", player_name, rec->injured_player_id);
    if (rec->replacement_player_id != 0u) {
        uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
        if (replacement != NULL) {
            kbo_copy_player_display_name(replacement, replacement_player_name, sizeof(replacement_player_name));
        }
        if (replacement_player_name[0] == '\0') {
            snprintf(replacement_player_name, sizeof(replacement_player_name), "Player #%u", rec->replacement_player_id);
        }
        snprintf(replacement_player_link, sizeof(replacement_player_link), "<%s:player#%u>", replacement_player_name, rec->replacement_player_id);
    } else {
        const int use_korean = kbo_foreign_injury_news_uses_korean();
        snprintf(
            replacement_player_name,
            sizeof(replacement_player_name),
            "%s",
            use_korean ? "\xeb\x8c\x80\xec\xb2\xb4 \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8" : "the temporary replacement");
        snprintf(
            replacement_player_link,
            sizeof(replacement_player_link),
            "%s",
            use_korean ? "\xeb\x8c\x80\xec\xb2\xb4 \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8" : "the temporary replacement");
    }
    int keep_replacement = phase != NULL && strcmp(phase, "closed_keep_replacement") == 0;
    if (keep_replacement) {
        snprintf(retained_player_name, sizeof(retained_player_name), "%s", replacement_player_name);
        snprintf(retained_player_link, sizeof(retained_player_link), "%s", replacement_player_link);
        snprintf(released_player_name, sizeof(released_player_name), "%s", player_name);
        snprintf(released_player_link, sizeof(released_player_link), "%s", injured_player_link);
    } else {
        snprintf(retained_player_name, sizeof(retained_player_name), "%s", player_name);
        snprintf(retained_player_link, sizeof(retained_player_link), "%s", injured_player_link);
        snprintf(released_player_name, sizeof(released_player_name), "%s", replacement_player_name);
        snprintf(released_player_link, sizeof(released_player_link), "%s", replacement_player_link);
    }

    const char* title_key = "foreign_injury.open.title";
    const char* body_key = "foreign_injury.open.body";
    if (phase != NULL && strcmp(phase, "open_roster") == 0) {
        title_key = "foreign_injury.open_roster.title";
        body_key = "foreign_injury.open_roster.body";
    } else if (phase != NULL && strcmp(phase, "active") == 0) {
        title_key = "foreign_injury.active.title";
        body_key = "foreign_injury.active.body";
    } else if (phase != NULL && strcmp(phase, "closed_keep_replacement") == 0) {
        title_key = "foreign_injury.closed_keep_replacement.title";
        body_key = "foreign_injury.closed_keep_replacement.body";
    } else if (phase != NULL && strcmp(phase, "closed_keep_injured") == 0) {
        title_key = "foreign_injury.closed_keep_injured.title";
        body_key = "foreign_injury.closed_keep_injured.body";
    } else if (phase != NULL && strcmp(phase, "closed_invalid") == 0) {
        title_key = "foreign_injury.closed_invalid.title";
        body_key = "foreign_injury.closed_invalid.body";
    } else if (phase != NULL && strcmp(phase, "closed_with_replacement") == 0) {
        title_key = "foreign_injury.closed_with_replacement.title";
        body_key = "foreign_injury.closed_with_replacement.body";
    } else if (phase != NULL && strcmp(phase, "closed_without_replacement") == 0) {
        title_key = "foreign_injury.closed_without_replacement.title";
        body_key = "foreign_injury.closed_without_replacement.body";
    } else if (phase != NULL && strcmp(phase, "closed") == 0) {
        if (rec->replacement_player_id != 0u) {
            title_key = "foreign_injury.closed_with_replacement.title";
            body_key = "foreign_injury.closed_with_replacement.body";
        } else {
            title_key = "foreign_injury.closed_without_replacement.title";
            body_key = "foreign_injury.closed_without_replacement.body";
        }
    } else if (phase != NULL && strcmp(phase, "pending") == 0) {
        title_key = "foreign_injury.pending.title";
        body_key = "foreign_injury.pending.body";
    }

    int display_days_left = days_left;
    if (display_days_left <= 0) {
        display_days_left = kbo_foreign_injury_days_until(event_date, rec->expected_end_yyyymmdd);
    }
    snprintf(days_left_text, sizeof(days_left_text), "%d", display_days_left > 0 ? display_days_left : 0);

    const int days_left_required = strcmp(body_key, "foreign_injury.open.body") == 0;
    if (days_left_required && display_days_left <= 0) {
        kbo_log_runtimef(
            "foreign injury replacement: news skipped phase=%s team=%u injured=%u league=%u reason=nonpositive_days_left days_left=%d expected_end=%u event_date=%u",
            phase != NULL ? phase : "open",
            rec->team_id,
            rec->injured_player_id,
            rec->league_id,
            days_left,
            rec->expected_end_yyyymmdd,
            event_date);
        return;
    }
    if (days_left_required
            && display_days_left < kbo_foreign_player_policy()->injury_replacement_min_days) {
        kbo_log_runtimef(
            "foreign injury replacement: news skipped phase=%s team=%u injured=%u league=%u reason=below_minimum_display_days days_left=%d display_days_left=%d min_days=%d expected_end=%u event_date=%u",
            phase != NULL ? phase : "open",
            rec->team_id,
            rec->injured_player_id,
            rec->league_id,
            days_left,
            display_days_left,
            kbo_foreign_player_policy()->injury_replacement_min_days,
            rec->expected_end_yyyymmdd,
            event_date);
        return;
    }

    KboNewsTemplateVar vars[] = {
        { "team_link", team_link },
        { "injured_player_name", player_name },
        { "injured_player_link", injured_player_link },
        { "replacement_player_name", replacement_player_name },
        { "replacement_player_link", replacement_player_link },
        { "retained_player_name", retained_player_name },
        { "retained_player_link", retained_player_link },
        { "released_player_name", released_player_name },
        { "released_player_link", released_player_link },
        { "days_left", days_left_text },
        { "slot_label", kbo_foreign_injury_slot_label(rec->slot_type) },
    };
    if (!kbo_news_template_render_key(
            title_key,
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            title,
            sizeof(title),
            phase != NULL ? phase : "foreign_injury")
            || !kbo_news_template_render_key(
                body_key,
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                phase != NULL ? phase : "foreign_injury")) {
        kbo_log_runtimef(
            "foreign injury replacement: news skipped phase=%s team=%u injured=%u league=%u reason=template_unavailable",
            phase != NULL ? phase : "open",
            rec->team_id,
            rec->injured_player_id,
            rec->league_id);
        return;
    }

    int created = create_kbo_native_live_news_with_body_live_required(
        event_date / 10000u,
        (event_date / 100u) % 100u,
        event_date % 100u,
        rec->league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    kbo_log_runtimef(
        "foreign injury replacement: news phase=%s team=%u injured=%u league=%u created=%d days_left=%d display_days_left=%d expected_end=%u event_date=%u",
        phase != NULL ? phase : "open",
        rec->team_id,
        rec->injured_player_id,
        rec->league_id,
        created,
        days_left,
        display_days_left,
        rec->expected_end_yyyymmdd,
        event_date);
}
