#include "../internal/fa_market_policy_internal.h"
#include "../../core/dates/constants/kbo_date_constants.h"


int kbo_fa_market_row_is_undrafted_domestic(const KboFaMarketClassification* row)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u
            || row->generation_context != 0u
            || row->generation_grade == 0u) {
        return 0;
    }

    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    if (row->draft_league_id == (uint32_t)policy->undrafted_college_league_id
            && row->draft_subtype == (uint8_t)policy->undrafted_college_draft_subtype
            && row->age <= (uint16_t)policy->undrafted_college_age_max) {
        return 1;
    }
    if (row->draft_league_id == (uint32_t)policy->undrafted_high_school_league_id
            && row->age <= (uint16_t)policy->undrafted_high_school_age_max) {
        return 1;
    }
    return 0;
}

static int kbo_fa_market_row_can_be_independent_source(const KboFaMarketClassification* row)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u) {
        return 0;
    }
    return 1;
}

static int kbo_fa_market_classified_team_independent_kind(uint32_t team_id)
{
    return kbo_team_classification_independent_kind_for_team(team_id);
}

int kbo_fa_market_row_independent_source_kind(const KboFaMarketClassification* row)
{
    if (!kbo_fa_market_row_can_be_independent_source(row)) {
        return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE;
    }

    int original_kind = kbo_fa_market_classified_team_independent_kind(row->original_team_id);
    if (original_kind != KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE) {
        return original_kind;
    }
    int active_kind = kbo_fa_market_classified_team_independent_kind(row->active_team_id);
    if (active_kind != KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE) {
        return active_kind;
    }

    uint32_t original_team_league_id = kbo_fa_market_get_team_league_id(row->original_team_id);
    uint32_t active_team_league_id = kbo_fa_market_get_team_league_id(row->active_team_id);
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    uint32_t independent_league_id = (uint32_t)policy->independent_league_id;
    if (independent_league_id != 0u
            && (original_team_league_id == independent_league_id
                || active_team_league_id == independent_league_id
                || row->original_league_id == independent_league_id
                || row->current_league_id == independent_league_id
                || row->draft_league_id == independent_league_id)) {
        return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE;
    }
    return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE;
}

int kbo_fa_market_row_is_independent_league_fa(const KboFaMarketClassification* row)
{
    return kbo_fa_market_row_independent_source_kind(row)
        == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE;
}

uint32_t kbo_fa_market_history_date_u32(const KboFaMarketHistoryCase* history)
{
    if (history == NULL || history->history_date[0] == '\0') {
        return 0u;
    }
    uint32_t date = 0u;
    if (kbo_parse_yyyymmdd(history->history_date, &date)) {
        return date;
    }
    return kbo_fa_filing_parse_u32(history->history_date);
}

static uint32_t kbo_fa_market_filing_season_from_date(uint32_t yyyymmdd)
{
    uint32_t season = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (season < KBO_SEASON_YEAR_MIN || season > KBO_RECORD_YEAR_MAX || month == 0u || month > 12u || day == 0u || day > 31u) {
        return 0u;
    }
    return season;
}

void kbo_fa_market_apply_history_filing_metadata(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history)
{
    if (row == NULL || history == NULL || !history->found || !history->became_free_agent) {
        return;
    }

    uint32_t filing_date = kbo_fa_market_history_date_u32(history);
    uint32_t filing_season = kbo_fa_market_filing_season_from_date(filing_date);
    if (row->fa_filing_date == 0u && filing_date != 0u) {
        row->fa_filing_date = filing_date;
    }
    if (row->fa_filing_season == 0u && filing_season != 0u) {
        row->fa_filing_season = filing_season;
    }
}

int kbo_fa_market_history_is_carryover_unsigned(
    const KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history,
    uint32_t today_yyyymmdd)
{
    if (row == NULL || history == NULL || !history->found || !history->became_free_agent) {
        return 0;
    }

    uint32_t filing_season = row->fa_filing_season;
    if (filing_season == 0u) {
        filing_season = kbo_fa_market_filing_season_from_date(kbo_fa_market_history_date_u32(history));
    }
    uint32_t current_year = today_yyyymmdd / 10000u;
    if (filing_season < KBO_SEASON_YEAR_MIN || filing_season > KBO_RECORD_YEAR_MAX || current_year < KBO_SEASON_YEAR_MIN || current_year > KBO_RECORD_YEAR_MAX) {
        return 0;
    }
    return filing_season < current_year;
}

void kbo_fa_market_set_history_reason(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history,
    const char* prefix)
{
    if (row == NULL) {
        return;
    }
    if (history != NULL && history->history_date[0] != '\0' && history->history_text[0] != '\0') {
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s history=%s %.96s",
            prefix != NULL ? prefix : "player history match",
            history->history_date,
            history->history_text);
        return;
    }
    snprintf(row->reason, sizeof(row->reason), "%s", prefix != NULL ? prefix : "player history match");
}

int kbo_fa_market_apply_history_case(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history)
{
    if (row == NULL || history == NULL || !history->found) {
        return 0;
    }

    if (history->undrafted_free_agent) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_UNDRAFTED_FREE_AGENT");
        kbo_fa_market_set_history_reason(row, history, "player history says undrafted free agent");
        return 1;
    }

    if (history->released) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_RELEASED_NON_FA");
        kbo_fa_market_set_history_reason(row, history, "player history says released, not official FA");
        return 1;
    }

    if (history->became_free_agent) {
        kbo_fa_market_apply_history_filing_metadata(row, history);

        int independent_kind = kbo_fa_market_row_independent_source_kind(row);
        if (independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES) {
            snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_INDEPENDENT_FUTURES_FA");
            kbo_fa_market_set_history_reason(row, history, "player history says free agent from independent futures-team context");
            return 1;
        }
        if (independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE) {
            snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_INDEPENDENT_LEAGUE_FA");
            kbo_fa_market_set_history_reason(row, history, "player history says free agent from true independent-league context");
            return 1;
        }

        snprintf(row->case_label, sizeof(row->case_label), "KBO_FA_BY_HISTORY_UNGRADED");
        kbo_fa_market_set_history_reason(row, history, "player history says became a free agent; grade pending salary snapshot or seed");
        return 1;
    }

    return 0;
}


void kbo_fa_market_mark_history_case(KboFaMarketHistoryCase* history)
{
    if (history == NULL || history->history_text[0] == '\0') {
        return;
    }
    history->became_free_agent = strcmp(history->history_text, "[G]Became a free agent.") == 0;
    history->undrafted_free_agent =
        strstr(history->history_text, "Was not drafted and became a free agent") != NULL;
    history->released = strncmp(history->history_text, "[G]Released by ", 15) == 0;
}

