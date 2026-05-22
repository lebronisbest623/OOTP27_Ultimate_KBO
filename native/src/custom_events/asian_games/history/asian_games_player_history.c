#include "asian_games_player_history.h"

#include <stdio.h>

#include "../../../core/logging/core_log.h"
#include "../../../core/sql/history_transactions/core_sql_history_transactions.h"

static int kbo_asian_games_history_date_parts(
    uint32_t event_yyyymmdd,
    uint32_t* out_year,
    uint32_t* out_month,
    uint32_t* out_day)
{
    if (event_yyyymmdd == 0u || out_year == NULL || out_month == NULL || out_day == NULL) {
        return 0;
    }
    *out_year = event_yyyymmdd / 10000u;
    *out_month = (event_yyyymmdd / 100u) % 100u;
    *out_day = event_yyyymmdd % 100u;
    return *out_year != 0u && *out_month >= 1u && *out_month <= 12u && *out_day >= 1u && *out_day <= 31u;
}

static int kbo_record_asian_games_player_history_text(
    uint32_t player_id,
    uint32_t event_yyyymmdd,
    const char* history_text,
    const char* source)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (player_id == 0u || history_text == NULL || history_text[0] == '\0'
            || !kbo_asian_games_history_date_parts(event_yyyymmdd, &year, &month, &day)) {
        return 0;
    }
    return insert_kbo_player_history_sql(
        player_id,
        year,
        month,
        day,
        history_text,
        source != NULL ? source : "asian_games_player_history");
}

int kbo_record_asian_games_selection_history(
    const KboAsianGamesRosterEntry* entry,
    uint32_t event_yyyymmdd,
    const char* source)
{
    if (entry == NULL || entry->player_id == 0u) {
        return 0;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    char history_text[256] = {0};
    if (entry->wildcard != 0u) {
        snprintf(
            history_text,
            sizeof(history_text),
            "Selected to Korea's %u Asian Games roster as a wild card.",
            year);
    } else {
        snprintf(
            history_text,
            sizeof(history_text),
            "Selected to Korea's %u Asian Games roster.",
            year);
    }
    return kbo_record_asian_games_player_history_text(
        entry->player_id,
        event_yyyymmdd,
        history_text,
        source != NULL ? source : "asian_games_selection_history");
}

int kbo_record_asian_games_replacement_history(
    const KboAsianGamesRosterEntry* old_entry,
    const KboAsianGamesRosterEntry* new_entry,
    uint32_t event_yyyymmdd,
    const char* source)
{
    uint32_t year = event_yyyymmdd / 10000u;
    int recorded = 0;
    if (old_entry != NULL && old_entry->player_id != 0u) {
        char old_text[256] = {0};
        snprintf(
            old_text,
            sizeof(old_text),
            "Removed from Korea's %u Asian Games roster after becoming unavailable before departure.",
            year);
        recorded += kbo_record_asian_games_player_history_text(
            old_entry->player_id,
            event_yyyymmdd,
            old_text,
            source != NULL ? source : "asian_games_replacement_history");
    }
    if (new_entry != NULL && new_entry->player_id != 0u) {
        char new_text[256] = {0};
        snprintf(
            new_text,
            sizeof(new_text),
            "Added to Korea's %u Asian Games roster as an injury replacement.",
            year);
        recorded += kbo_record_asian_games_player_history_text(
            new_entry->player_id,
            event_yyyymmdd,
            new_text,
            source != NULL ? source : "asian_games_replacement_history");
    }
    return recorded;
}

int kbo_record_asian_games_final_history(
    const KboAsianGamesRosterEntry* entry,
    uint32_t event_yyyymmdd,
    uint8_t final_result,
    const char* source)
{
    if (entry == NULL || entry->player_id == 0u) {
        return 0;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    char history_text[256] = {0};
    if (final_result == KBO_ASIAN_GAMES_RESULT_GOLD && entry->exempted != 0u) {
        snprintf(
            history_text,
            sizeof(history_text),
            "Won the %u Asian Games gold medal with Korea and received military-service exemption.",
            year);
    } else if (final_result == KBO_ASIAN_GAMES_RESULT_GOLD) {
        snprintf(
            history_text,
            sizeof(history_text),
            "Won the %u Asian Games gold medal with Korea.",
            year);
    } else {
        snprintf(
            history_text,
            sizeof(history_text),
            "Returned from Korea's %u Asian Games roster after the tournament.",
            year);
    }

    int ok = kbo_record_asian_games_player_history_text(
        entry->player_id,
        event_yyyymmdd,
        history_text,
        source != NULL ? source : "asian_games_final_history");
    if (!ok) {
        kbo_log_runtimef(
            "KBO Asian Games final player history failed player_id=%u date=%u result=%u",
            entry->player_id,
            event_yyyymmdd,
            (uint32_t)final_result);
    }
    return ok;
}
