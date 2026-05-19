#include "../flags_api.h"

#include "../../localappdata/localappdata_reader.h"
#include "economic/economic_defaults.h"
#include "../../../logging/core_log.h"

#include <windows.h>

#define KBO_FOREIGN_FA_QUALITY_CAP_ENABLED_KEY "foreign_fa_quality_cap_enabled"

int kbo_get_foreign_fa_quality_cap_enabled_setting(void)
{
    int value = kbo_economic_default_foreign_fa_quality_cap_enabled();
    if (!kbo_read_localappdata_setting_flag_value(
            KBO_FOREIGN_FA_QUALITY_CAP_ENABLED_KEY,
            &value)) {
        if (read_kbo_localappdata_flag_file("disable_intl_established_fa_quality_shaping.txt")) {
            value = 0;
        } else {
            value = kbo_economic_default_foreign_fa_quality_cap_enabled();
        }
    }
    return value ? 1 : 0;
}

int kbo_set_foreign_fa_quality_cap_enabled_setting(int enabled)
{
    return kbo_write_localappdata_setting_int_value(
        KBO_FOREIGN_FA_QUALITY_CAP_ENABLED_KEY,
        enabled ? 1 : 0);
}

#define KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_KEY "asian_games_no_gold_odds_denominator"

int kbo_clamp_asian_games_no_gold_odds_denominator(int value)
{
    if (value < KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_MIN) {
        return KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_MIN;
    }
    if (value > KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_MAX) {
        return KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_MAX;
    }
    return value;
}

int kbo_get_asian_games_no_gold_odds_denominator(void)
{
    int value = kbo_economic_default_asian_games_no_gold_odds_denominator();
    if (!kbo_read_localappdata_setting_int_value(
            KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_KEY,
            &value)) {
        value = kbo_economic_default_asian_games_no_gold_odds_denominator();
    }
    return kbo_clamp_asian_games_no_gold_odds_denominator(value);
}

int kbo_set_asian_games_no_gold_odds_denominator(int value)
{
    return kbo_write_localappdata_setting_int_value(
        KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_KEY,
        kbo_clamp_asian_games_no_gold_odds_denominator(value));
}

int kbo_get_profiler_enabled_setting(void)
{
    int value = 0;
    if (!kbo_read_localappdata_json_flag_value("enable_kbo_profiler", &value)) {
        value = 0;
    }
    return value ? 1 : 0;
}

int kbo_set_profiler_enabled_setting(int enabled)
{
    return kbo_write_localappdata_json_int_value("enable_kbo_profiler", enabled ? 1 : 0);
}

#define KBO_ALLOW_ALL_UI_TEAM_ACTIONS_KEY "allow_all_ui_team_actions"

int kbo_get_allow_all_ui_team_actions_setting(void)
{
    int value = 1;
    if (!kbo_read_localappdata_setting_flag_value(
            KBO_ALLOW_ALL_UI_TEAM_ACTIONS_KEY,
            &value)) {
        value = 1;
    }
    return value ? 1 : 0;
}

int kbo_set_allow_all_ui_team_actions_setting(int enabled)
{
    return kbo_write_localappdata_setting_int_value(KBO_ALLOW_ALL_UI_TEAM_ACTIONS_KEY, enabled ? 1 : 0);
}

#define KBO_INDEPENDENT_ACQUISITION_FOREIGN_CASH_COST_KEY "independent_acquisition_foreign_cash_cost"
#define KBO_INDEPENDENT_ACQUISITION_DOMESTIC_CASH_COST_KEY "independent_acquisition_domestic_cash_cost"

int32_t kbo_clamp_independent_acquisition_cash_cost(int32_t value)
{
    if (value < KBO_INDEPENDENT_ACQUISITION_CASH_COST_MIN) {
        return KBO_INDEPENDENT_ACQUISITION_CASH_COST_MIN;
    }
    if (value > KBO_INDEPENDENT_ACQUISITION_CASH_COST_MAX) {
        return KBO_INDEPENDENT_ACQUISITION_CASH_COST_MAX;
    }
    return value;
}

int32_t kbo_get_independent_acquisition_foreign_cash_cost(void)
{
    int default_value = kbo_economic_default_independent_acquisition_foreign_cash_cost();
    int value = default_value;
    if (!kbo_read_localappdata_setting_int_value(
            KBO_INDEPENDENT_ACQUISITION_FOREIGN_CASH_COST_KEY,
            &value)
            || value <= 0) {
        value = default_value;
    }
    return kbo_clamp_independent_acquisition_cash_cost(value);
}

int32_t kbo_get_independent_acquisition_domestic_cash_cost(void)
{
    int default_value = kbo_economic_default_independent_acquisition_domestic_cash_cost();
    int value = default_value;
    if (!kbo_read_localappdata_setting_int_value(
            KBO_INDEPENDENT_ACQUISITION_DOMESTIC_CASH_COST_KEY,
            &value)
            || value <= 0) {
        value = default_value;
    }
    return kbo_clamp_independent_acquisition_cash_cost(value);
}

int kbo_set_independent_acquisition_foreign_cash_cost(int32_t value)
{
    return kbo_write_localappdata_setting_int_value(
        KBO_INDEPENDENT_ACQUISITION_FOREIGN_CASH_COST_KEY,
        kbo_clamp_independent_acquisition_cash_cost(value));
}

int kbo_set_independent_acquisition_domestic_cash_cost(int32_t value)
{
    return kbo_write_localappdata_setting_int_value(
        KBO_INDEPENDENT_ACQUISITION_DOMESTIC_CASH_COST_KEY,
        kbo_clamp_independent_acquisition_cash_cost(value));
}

int kbo_fix_enabled(void)
{
    /* The DLL is purpose-injected by KBOLauncher, so it is enabled by default.
       Set enable_kbo_fix=false in kbo_flags.json to opt out. */
    static LONG cached = -1;
    LONG value = cached;
    if (value != -1) {
        return value == 1;
    }

    int enabled = 1;
    int configured = 0;
    if (kbo_read_localappdata_json_flag_value("enable_kbo_fix", &configured)) {
        enabled = configured ? 1 : 0;
    }

    InterlockedCompareExchange(&cached, enabled ? 1 : 0, -1);
    kbo_log_runtimef("KBO fix opt-in=%d", enabled ? 1 : 0);
    return enabled;
}
