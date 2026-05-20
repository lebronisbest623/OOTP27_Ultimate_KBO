#include "../internal/cbt_internal.h"

#include "../../core/news/templates/core_news_templates.h"
#include "../../core/dates/constants/kbo_date_constants.h"

static void kbo_cbt_format_usd(char* out, size_t out_size, int32_t value)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    if (value <= 0) {
        snprintf(out, out_size, "$0");
        return;
    }
    if (value >= 1000000) {
        int m = value / 1000000;
        int tenth = (value % 1000000) / 100000;
        if (tenth == 0) {
            snprintf(out, out_size, "$%dM", m);
        } else {
            snprintf(out, out_size, "$%d.%dM", m, tenth);
        }
    } else if (value >= 1000) {
        snprintf(out, out_size, "$%dk", value / 1000);
    } else {
        snprintf(out, out_size, "$%d", value);
    }
}

int kbo_cbt_news_date(
    uint32_t season,
    uint32_t news_yyyymmdd,
    uint32_t* out_year,
    uint32_t* out_month,
    uint32_t* out_day)
{
    uint32_t year = news_yyyymmdd / 10000u;
    uint32_t month = (news_yyyymmdd / 100u) % 100u;
    uint32_t day = news_yyyymmdd % 100u;
    if (year < KBO_SEASON_YEAR_MIN || year > KBO_SIM_YEAR_MAX || month < 1u || month > 12u || day < 1u || day > 31u) {
        year = season;
        month = 4u;
        day = 1u;
    }
    if (out_year != NULL) {
        *out_year = year;
    }
    if (out_month != NULL) {
        *out_month = month;
    }
    if (out_day != NULL) {
        *out_day = day;
    }
    return 1;
}

void kbo_cbt_copy_team_name(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        static const uint32_t string_offsets[] = { 0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u };
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, 0x10u, city, sizeof(city));
        copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));
        if (full_name[0] != '\0'
                && (strchr(full_name, ' ') != NULL || kbo_ootp_text_has_non_ascii(full_name))) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (city[0] != '\0' && nickname[0] != '\0' && !ascii_equals_ignore_case(city, nickname)) {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
            char text[96] = {0};
            if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text)) && text[0] != '\0') {
                snprintf(out, out_size, "%s", text);
                return;
            }
        }
    }

    snprintf(out, out_size, "Team #%u", team_id);
}

void kbo_cbt_insert_violation_news_v2(
    uint32_t league_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const KboCbtRecord* rec,
    const KboCbtRules* rules)
{
    if (league_id == 0u || rec == NULL || rules == NULL) {
        return;
    }

    const char* team_name = rec->team_name[0] != '\0' ? rec->team_name : "Team";

    int draft_penalty = (int)rec->consecutive_count >= (int)rules->draft_penalty_min_consecutive;
    char season_text[16] = {0};
    char payroll_text[32] = {0};
    char threshold_text[32] = {0};
    char overage_text[32] = {0};
    char tax_rate_text[16] = {0};
    char tax_amount_text[32] = {0};
    char consecutive_text[16] = {0};
    char draft_penalty_stages_text[16] = {0};
    snprintf(season_text, sizeof(season_text), "%u", rec->season);
    snprintf(payroll_text, sizeof(payroll_text), "%d", rec->payroll);
    snprintf(threshold_text, sizeof(threshold_text), "%d", rec->threshold);
    snprintf(overage_text, sizeof(overage_text), "%d", rec->overage);
    snprintf(tax_rate_text, sizeof(tax_rate_text), "%u", rec->tax_rate_pct);
    snprintf(tax_amount_text, sizeof(tax_amount_text), "%d", rec->tax_amount);
    snprintf(consecutive_text, sizeof(consecutive_text), "%u", rec->consecutive_count);
    snprintf(draft_penalty_stages_text, sizeof(draft_penalty_stages_text), "%u", rules->draft_penalty_stages);

    char title[256] = {0};
    char penalty_text[192] = {0};
    if (draft_penalty) {
        KboNewsTemplateVar penalty_vars[] = {
            { "draft_penalty_stages", draft_penalty_stages_text },
        };
        kbo_news_template_render_key(
            "cbt.violation.draft_penalty",
            penalty_vars,
            (int)(sizeof(penalty_vars) / sizeof(penalty_vars[0])),
            penalty_text,
            sizeof(penalty_text),
            "cbt_violation");
    }

    KboNewsTemplateVar vars[] = {
        { "team_name", team_name },
        { "season", season_text },
        { "payroll", payroll_text },
        { "threshold", threshold_text },
        { "overage", overage_text },
        { "tax_rate_pct", tax_rate_text },
        { "tax_amount", tax_amount_text },
        { "consecutive_count", consecutive_text },
        { "draft_penalty_text", penalty_text },
    };
    char body[2048] = {0};
    if (!kbo_news_template_render_key(
            "cbt.violation.title",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            title,
            sizeof(title),
            "cbt_violation")
            || !kbo_news_template_render_key(
                "cbt.violation.body",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                "cbt_violation")) {
        kbo_log_runtimef("KBO CBT violation news skipped season=%u team=%u reason=template_unavailable", rec->season, rec->team_id);
        return;
    }

    insert_kbo_league_news_sql(year, month, day, league_id, 2u, title, body, "cbt_violation");
    insert_kbo_league_news_table_sql(year, month, day, league_id, title, body, "cbt_violation");
}

int kbo_cbt_insert_opening_day_summary_news(
    uint32_t league_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t season,
    const KboCbtRecord* records,
    int record_count,
    int team_count,
    const KboCbtRules* rules)
{
    if (league_id == 0u || records == NULL || record_count <= 0 || rules == NULL) {
        return 0;
    }

    int reviewed = 0;
    int violations = 0;
    int32_t total_tax = 0;
    for (int i = 0; i < record_count; i++) {
        if (records[i].season != season) {
            continue;
        }
        reviewed++;
        if (records[i].overage > 0) {
            violations++;
            total_tax += records[i].tax_amount;
        }
    }
    if (reviewed == 0) {
        return 0;
    }

    char threshold_text[32] = {0};
    char total_tax_text[32] = {0};
    kbo_cbt_format_usd(threshold_text, sizeof(threshold_text), rules->threshold_override > 0 ? rules->threshold_override : kbo_cbt_get_threshold(rules, season));
    kbo_cbt_format_usd(total_tax_text, sizeof(total_tax_text), total_tax);

    char title[256] = {0};
    char body[8192] = {0};
    char season_text[16] = {0};
    char top_player_count_text[16] = {0};
    char clubs_reviewed_text[16] = {0};
    char violations_text[16] = {0};
    snprintf(season_text, sizeof(season_text), "%u", season);
    snprintf(top_player_count_text, sizeof(top_player_count_text), "%u", rules->top_player_count);
    snprintf(clubs_reviewed_text, sizeof(clubs_reviewed_text), "%d", team_count > 0 ? team_count : reviewed);
    snprintf(violations_text, sizeof(violations_text), "%d", violations);

    KboNewsTemplateVar summary_vars[] = {
        { "season", season_text },
        { "threshold_text", threshold_text },
        { "top_player_count", top_player_count_text },
        { "clubs_reviewed", clubs_reviewed_text },
        { "violations", violations_text },
        { "total_tax_text", total_tax_text },
    };
    if (!kbo_news_template_render_key(
            "cbt.summary.title",
            summary_vars,
            (int)(sizeof(summary_vars) / sizeof(summary_vars[0])),
            title,
            sizeof(title),
            "cbt_summary")
            || !kbo_news_template_render_key(
                "cbt.summary.intro",
                summary_vars,
                (int)(sizeof(summary_vars) / sizeof(summary_vars[0])),
                body,
                sizeof(body),
                "cbt_summary")) {
        kbo_log_runtimef("KBO CBT opening-day news skipped season=%u reason=template_unavailable", season);
        return 0;
    }

    if (violations <= 0) {
        char no_violations[1024] = {0};
        if (!kbo_news_template_render_key(
                "cbt.summary.no_violations",
                summary_vars,
                (int)(sizeof(summary_vars) / sizeof(summary_vars[0])),
                no_violations,
                sizeof(no_violations),
                "cbt_summary")) {
            kbo_log_runtimef("KBO CBT opening-day news skipped season=%u reason=no_violations_template_unavailable", season);
            return 0;
        }
        kbo_news_text_append(body, sizeof(body), no_violations);
    } else {
        char header[512] = {0};
        char line_template[1024] = {0};
        if (!kbo_news_template_load("cbt.summary.violations_header", header, sizeof(header), NULL, 0u, "cbt_summary")
                || !kbo_news_template_load("cbt.summary.violation_line", line_template, sizeof(line_template), NULL, 0u, "cbt_summary")) {
            kbo_log_runtimef("KBO CBT opening-day news skipped season=%u reason=violation_list_template_unavailable", season);
            return 0;
        }
        kbo_news_text_append(body, sizeof(body), header);
        for (int i = 0; i < record_count; i++) {
            const KboCbtRecord* rec = &records[i];
            if (rec->season != season || rec->overage <= 0) {
                continue;
            }

            char team_name[96] = {0};
            if (rec->team_name[0] != '\0') {
                snprintf(team_name, sizeof(team_name), "%s", rec->team_name);
            } else {
                kbo_cbt_copy_team_name(rec->team_id, team_name, sizeof(team_name));
            }

            char payroll_text[32] = {0};
            char overage_text[32] = {0};
            char tax_text[32] = {0};
            char tax_rate_text[16] = {0};
            char consecutive_text[16] = {0};
            kbo_cbt_format_usd(payroll_text, sizeof(payroll_text), rec->payroll);
            kbo_cbt_format_usd(overage_text, sizeof(overage_text), rec->overage);
            kbo_cbt_format_usd(tax_text, sizeof(tax_text), rec->tax_amount);
            snprintf(tax_rate_text, sizeof(tax_rate_text), "%u", rec->tax_rate_pct);
            snprintf(consecutive_text, sizeof(consecutive_text), "%u", rec->consecutive_count);
            KboNewsTemplateVar line_vars[] = {
                { "team_name", team_name },
                { "payroll_text", payroll_text },
                { "overage_text", overage_text },
                { "tax_text", tax_text },
                { "tax_rate_pct", tax_rate_text },
                { "consecutive_count", consecutive_text },
            };
            char rendered_line[1024] = {0};
            kbo_news_template_render(
                line_template,
                line_vars,
                (int)(sizeof(line_vars) / sizeof(line_vars[0])),
                rendered_line,
                sizeof(rendered_line));
            kbo_news_text_append(body, sizeof(body), rendered_line);
        }

        if (rules->draft_penalty_min_consecutive > 0u) {
            char min_consecutive_text[16] = {0};
            char stages_text[16] = {0};
            char penalty[1024] = {0};
            snprintf(min_consecutive_text, sizeof(min_consecutive_text), "%u", rules->draft_penalty_min_consecutive);
            snprintf(stages_text, sizeof(stages_text), "%u", rules->draft_penalty_stages);
            KboNewsTemplateVar penalty_vars[] = {
                { "draft_penalty_min_consecutive", min_consecutive_text },
                { "draft_penalty_stages", stages_text },
            };
            if (!kbo_news_template_render_key(
                    "cbt.summary.draft_penalty",
                    penalty_vars,
                    (int)(sizeof(penalty_vars) / sizeof(penalty_vars[0])),
                    penalty,
                    sizeof(penalty),
                    "cbt_summary")) {
                kbo_log_runtimef("KBO CBT opening-day news skipped season=%u reason=draft_penalty_template_unavailable", season);
                return 0;
            }
            kbo_news_text_append(body, sizeof(body), penalty);
        }
    }

    int created = create_kbo_native_live_news_with_body(year, month, day, league_id, 2u, title, body);
    kbo_log_runtimef(
        "KBO CBT opening-day news season=%u date=%04u%02u%02u league_id=%u reviewed=%d violations=%d created=%d",
        season,
        year,
        month,
        day,
        league_id,
        reviewed,
        violations,
        created);
    return created;
}
