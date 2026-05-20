#include "../info/ui_mod_info_views.h"
#include "../../../runtime/mode/hotkey_window_runtime_mode.h"
#include "../../../support/text/language/ui_language.h"
#include "../../../../core/core_flags/api/flags_api.h"

#include <stdio.h>

static void kbo_webview_append_settings_section_start(KboWindowTextBuffer* buffer, const char* title_ko, const char* title_en)
{
    kbo_window_text_appendf(buffer, "<div class='settingsSection'><div class='settingsSectionHead'><h3>");
    kbo_html_append_escaped(buffer, kbo_hub_text(title_ko, title_en));
    kbo_window_text_appendf(buffer, "</h3></div><div class='settingsRows'>");
}

static void kbo_webview_append_settings_section_end(KboWindowTextBuffer* buffer)
{
    kbo_window_text_appendf(buffer, "</div></div>");
}

static void kbo_webview_append_asian_quota_salary_limit_setting(KboWindowTextBuffer* buffer)
{
    int asian_quota_salary_limit = kbo_get_asian_quota_salary_limit();
    kbo_window_text_appendf(
        buffer,
        "<div class='settingRow'><label class='settingLabel' for='asianQuotaSalaryLimit'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x84\xec\xbf\xbc\xed\x84\xb0 \xec\x97\xb0\xeb\xb4\x89 \xec\x83\x81\xed\x95\x9c", "아시아쿼터 연봉 상한"));
    kbo_window_text_appendf(
        buffer,
        "</label><input id='asianQuotaSalaryLimit' class='ootpSelect salaryInput' type='text' inputmode='numeric' pattern='[0-9]*' data-min='0' data-max='20000000' data-step='1000' value='%d' "
        "onchange=\"location.href='kbo://settings/asian-quota-salary-limit/'+this.value\" "
        "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
        asian_quota_salary_limit);
}

void kbo_webview_append_settings_view(KboWindowTextBuffer* buffer, int selected_settings_subview)
{
    if (buffer == NULL) {
        return;
    }
    (void)selected_settings_subview;

    int multiplier = kbo_get_intl_established_fa_multiplier();
    const int presets[] = {1, 2, 5, 10, 20};
    int preset_has_current = 0;
    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        if (presets[i] == multiplier) {
            preset_has_current = 1;
            break;
        }
    }

    const char* baseline_labels_ko[9] = {
        "\xec\xb5\x9c\xec\xa0\x80 \xec\x97\xb0\xeb\xb4\x89",
        "\xed\x95\x98\xea\xb8\x89",
        "\xeb\xb3\xb4\xed\x86\xb5 \xec\x9d\xb4\xed\x95\x98",
        "\xed\x8f\x89\xea\xb7\xa0 \xec\x9d\xb4\xed\x95\x98",
        "\xed\x8f\x89\xea\xb7\xa0",
        "\xed\x8f\x89\xea\xb7\xa0 \xec\x9d\xb4\xec\x83\x81",
        "\xec\xa4\x80\xec\xb2\x99\xea\xb8\x89",
        "\xec\x8a\xa4\xed\x83\x80\xea\xb8\x89",
        "\xec\x8a\x88\xed\x8d\xbc\xec\x8a\xa4\xed\x83\x80\xea\xb8\x89"
    };
    const char* baseline_labels_en[9] = {
        "최저 연봉",
        "하급",
        "보통 이하",
        "평균 이하",
        "평균",
        "평균 이상",
        "준척급",
        "스타급",
        "슈퍼스타급"
    };
    const char* non_asian_quality_cap_labels_ko[KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_COUNT] = {
        "\xec\x84\xa0\xeb\xb0\x9c \xed\x88\xac\xec\x88\x98",
        "\xeb\xb6\x88\xed\x8e\x9c \xed\x88\xac\xec\x88\x98",
        "\xed\x88\xac\xec\x88\x98(\xec\x97\xad\xed\x95\xa0 \xeb\xaf\xb8\xec\x83\x81)",
        "\xec\x95\xbc\xec\x88\x98",
        "\xed\x8f\xac\xec\x88\x98"
    };
    const char* non_asian_quality_cap_labels_en[KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_COUNT] = {
        "선발 투수",
        "불펜 투수",
        "투수(역할 미상)",
        "야수",
        "포수"
    };
    kbo_window_text_appendf(
        buffer,
        "<div class='rights settingsGrid'>"
        "<section class='card modCard settingsCard leagueSettingsCard'><h2 class='cardTitle'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x84\xa4\xec\xa0\x95", "리그 설정"));
    kbo_window_text_appendf(buffer, "</h2>");

    if (!kbo_hub_current_mode_is_developer()) {
        kbo_webview_append_settings_section_start(buffer, "\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98 \xec\x9a\xb4\xec\x98\x81", "외국인 선수 운영");
        kbo_webview_append_asian_quota_salary_limit_setting(buffer);
        kbo_webview_append_settings_section_end(buffer);
        kbo_window_text_appendf(buffer, "</section></div>");
        return;
    }

    kbo_webview_append_settings_section_start(buffer, "\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98 \xec\x9a\xb4\xec\x98\x81", "외국인 선수 운영");
    kbo_window_text_appendf(buffer, "<div class='settingRow'><label class='settingLabel' for='intlFaMultiplierSelect'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 FA \xec\x83\x9d\xec\x84\xb1", "외국인 FA 생성"));
    char current_choice_label[32] = {0};
    snprintf(current_choice_label, sizeof(current_choice_label), "%dx", multiplier);
    kbo_window_text_appendf(buffer, "</label>");
    kbo_webview_begin_ootp_choice(buffer, "intlFaMultiplierSelect", current_choice_label);
    if (!preset_has_current) {
        char href[96] = {0};
        snprintf(href, sizeof(href), "kbo://settings/intl-fa-multiplier/%d", multiplier);
        kbo_webview_append_ootp_choice_option(buffer, href, current_choice_label, 1);
    }

    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        int value = presets[i];
        char href[96] = {0};
        char label[32] = {0};
        snprintf(href, sizeof(href), "kbo://settings/intl-fa-multiplier/%d", value);
        snprintf(label, sizeof(label), "%dx", value);
        kbo_webview_append_ootp_choice_option(buffer, href, label, value == multiplier);
    }
    kbo_webview_end_ootp_choice(buffer);
    kbo_window_text_appendf(buffer, "</div>");

    int quality_cap_enabled = kbo_get_foreign_fa_quality_cap_enabled_setting();
    int asian_games_no_gold_odds = kbo_get_asian_games_no_gold_odds_denominator();
    int32_t independent_foreign_cash_cost = kbo_get_independent_acquisition_foreign_cash_cost();
    int32_t independent_domestic_cash_cost = kbo_get_independent_acquisition_domestic_cash_cost();
    kbo_window_text_appendf(buffer, "<div class='settingRow'><label class='settingLabel' for='foreignFaQualityCapSelect'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x83\x9d\xec\x84\xb1 \xed\x92\x88\xec\xa7\x88 \xec\xba\xa1", "외국인 생성 품질 캡"));
    kbo_window_text_appendf(buffer, "</label>");
    kbo_webview_begin_ootp_choice(buffer, "foreignFaQualityCapSelect", quality_cap_enabled ? "켬" : "끔");
    kbo_webview_append_ootp_choice_option(buffer, "kbo://settings/foreign-fa-quality-cap/on", "켬", quality_cap_enabled);
    kbo_webview_append_ootp_choice_option(buffer, "kbo://settings/foreign-fa-quality-cap/off", "끔", !quality_cap_enabled);
    kbo_webview_end_ootp_choice(buffer);
    kbo_window_text_appendf(buffer, "</div>");

    kbo_webview_append_asian_quota_salary_limit_setting(buffer);

    kbo_window_text_appendf(
        buffer,
        "<div class='settingRow'><label class='settingLabel' for='asianGamesNoGoldOdds'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84 \xec\x9a\xb0\xec\x8a\xb9 \xec\x8b\xa4\xed\x8c\xa8 \xed\x99\x95\xeb\xa5\xa0(1/N)", "Asian Games no-gold odds (1/N)"));
    kbo_window_text_appendf(
        buffer,
        "</label><input id='asianGamesNoGoldOdds' class='ootpSelect salaryInput' type='text' inputmode='numeric' pattern='[0-9]*' data-min='%d' data-max='%d' data-step='1' value='%d' "
        "onchange=\"location.href='kbo://settings/asian-games-no-gold-odds/'+this.value\" "
        "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
        KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_MIN,
        KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_MAX,
        asian_games_no_gold_odds);

    kbo_webview_append_settings_section_end(buffer);
    kbo_webview_append_settings_section_start(buffer, "2\xea\xb5\xb0 \xeb\x8f\x85\xeb\xa6\xbd \xea\xb5\xac\xeb\x8b\xa8 \xec\x98\x81\xec\x9e\x85", "2군 독립 구단 영입");

    kbo_window_text_appendf(
        buffer,
        "<div class='settingRow'><label class='settingLabel' for='independentForeignCashCost'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98", "외국인 선수"));
    kbo_window_text_appendf(
        buffer,
        "</label><input id='independentForeignCashCost' class='ootpSelect salaryInput' type='text' inputmode='numeric' pattern='[0-9]*' data-min='%d' data-max='%d' data-step='10000' value='%d' "
        "onchange=\"location.href='kbo://settings/independent-acquisition-foreign-cash-cost/'+this.value\" "
        "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
        KBO_INDEPENDENT_ACQUISITION_CASH_COST_MIN,
        KBO_INDEPENDENT_ACQUISITION_CASH_COST_MAX,
        independent_foreign_cash_cost);

    kbo_window_text_appendf(
        buffer,
        "<div class='settingRow'><label class='settingLabel' for='independentDomesticCashCost'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xeb\x82\xb4\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98", "내국인 선수"));
    kbo_window_text_appendf(
        buffer,
        "</label><input id='independentDomesticCashCost' class='ootpSelect salaryInput' type='text' inputmode='numeric' pattern='[0-9]*' data-min='%d' data-max='%d' data-step='10000' value='%d' "
        "onchange=\"location.href='kbo://settings/independent-acquisition-domestic-cash-cost/'+this.value\" "
        "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
        KBO_INDEPENDENT_ACQUISITION_CASH_COST_MIN,
        KBO_INDEPENDENT_ACQUISITION_CASH_COST_MAX,
        independent_domestic_cash_cost);

    kbo_webview_append_settings_section_end(buffer);
    kbo_webview_append_settings_section_start(buffer, "\xeb\xb9\x84\xec\x95\x84\xec\x8b\x9c\xec\x95\x84 \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xed\x92\x88\xec\xa7\x88 \xec\xba\xa1", "비아시아 외국인 품질 캡");

    for (int i = 0; i < KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_COUNT; i++) {
        int current = kbo_get_foreign_fa_non_asian_quality_cap_value(i);
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='nonAsianQualityCap%d'>",
            i);
        kbo_html_append_escaped(buffer, kbo_hub_text(non_asian_quality_cap_labels_ko[i], non_asian_quality_cap_labels_en[i]));
        kbo_window_text_appendf(
            buffer,
            "</label><input id='nonAsianQualityCap%d' class='ootpSelect salaryInput' type='text' inputmode='numeric' pattern='[0-9]*' data-min='0' data-max='%d' data-step='500' value='%d' "
            "onchange=\"location.href='kbo://settings/foreign-fa-non-asian-quality-cap/%d/'+this.value\" "
            "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
            i,
            KBO_FOREIGN_FA_QUALITY_CAP_MAX,
            current,
            i);
    }

    kbo_webview_append_settings_section_end(buffer);
    kbo_webview_append_settings_section_start(buffer, "\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x97\xb0\xeb\xb4\x89 \xeb\xb2\xa0\xec\x9d\xb4\xec\x8a\xa4\xeb\x9d\xbc\xec\x9d\xb8", "외국인 연봉 베이스라인");

    for (int i = 0; i < 9; i++) {
        int current = kbo_get_foreign_fa_demand_baseline_value(i);
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='foreignBaseline%d'>",
            i);
        kbo_html_append_escaped(buffer, kbo_hub_text(baseline_labels_ko[i], baseline_labels_en[i]));
        kbo_window_text_appendf(
            buffer,
            "</label><input id='foreignBaseline%d' class='ootpSelect salaryInput' type='text' inputmode='numeric' pattern='[0-9]*' data-min='0' data-max='20000000' data-step='1000' value='%d' "
            "onchange=\"location.href='kbo://settings/foreign-fa-baseline/%d/'+this.value\" "
            "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
            i,
            current,
            i);
    }

    kbo_webview_append_settings_section_end(buffer);
    kbo_webview_append_settings_section_start(buffer, "\xec\x95\x84\xec\x8b\x9c\xec\x95\x84\xec\xbf\xbc\xed\x84\xb0 \xec\x97\xb0\xeb\xb4\x89 \xeb\xb2\xa0\xec\x9d\xb4\xec\x8a\xa4\xeb\x9d\xbc\xec\x9d\xb8", "아시아쿼터 연봉 베이스라인");

    for (int i = 0; i < 9; i++) {
        int current = kbo_get_asian_quota_fa_demand_baseline_value(i);
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='asianQuotaBaseline%d'>",
            i);
        kbo_html_append_escaped(buffer, kbo_hub_text(baseline_labels_ko[i], baseline_labels_en[i]));
        kbo_window_text_appendf(
            buffer,
            "</label><input id='asianQuotaBaseline%d' class='ootpSelect salaryInput' type='text' inputmode='numeric' pattern='[0-9]*' data-min='0' data-max='20000000' data-step='1000' value='%d' "
            "onchange=\"location.href='kbo://settings/asian-quota-fa-baseline/%d/'+this.value\" "
            "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
            i,
            current,
            i);
    }

    kbo_webview_append_settings_section_end(buffer);
    kbo_window_text_appendf(buffer, "</section></div>");
}
