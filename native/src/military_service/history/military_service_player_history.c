#include "military_service_player_history.h"

#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/logging/core_log.h"
#include "../../core/sql/history_transactions/core_sql_history_transactions.h"
#include "../../team/lookup/team_lookup.h"
#include "../selection/news/military_selection_news.h"

static int kbo_military_history_date_parts(
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

static uint32_t kbo_military_history_team_id_for_csv(const char* csv_id)
{
    uint8_t* team = csv_id != NULL ? find_kbo_team_by_csv_id_any_league(csv_id, 0) : NULL;
    return team != NULL ? *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
}

static void kbo_military_history_copy_service_team_name(
    uint32_t service_team_id,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    uint32_t sang_id = kbo_military_history_team_id_for_csv("SANG");
    uint32_t kpb_id = kbo_military_history_team_id_for_csv("KPB");
    const char* fallback = "military service team";
    if (service_team_id != 0u && service_team_id == sang_id) {
        fallback = "Sangmu Baseball Team";
    } else if (service_team_id != 0u && service_team_id == kpb_id) {
        fallback = "Korean Police Baseball Team";
    }

    uint8_t* service_team = service_team_id != 0u
        ? find_kbo_team_by_numeric_id_any_league(service_team_id, 0)
        : NULL;
    kbo_military_copy_team_history_name(service_team, out, out_size, fallback);
}

static void kbo_military_history_copy_original_team_name(
    uint32_t original_team_id,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    uint8_t* original_team = original_team_id != 0u
        ? find_kbo_team_by_numeric_id_any_league(original_team_id, 0)
        : NULL;
    kbo_military_copy_team_history_name(
        original_team,
        out,
        out_size,
        "his original KBO organization");
}

static int kbo_record_military_player_history_text(
    uint32_t player_id,
    uint32_t event_yyyymmdd,
    const char* history_text,
    const char* source)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (player_id == 0u || history_text == NULL || history_text[0] == '\0'
            || !kbo_military_history_date_parts(event_yyyymmdd, &year, &month, &day)) {
        return 0;
    }
    return insert_kbo_player_history_sql(
        player_id,
        year,
        month,
        day,
        history_text,
        source != NULL ? source : "military_service_player_history");
}

static int kbo_record_military_selection_player_history(
    uint32_t player_id,
    uint32_t service_team_id,
    uint32_t original_team_id,
    uint32_t event_yyyymmdd,
    const char* source)
{
    char service_team_name[96] = {0};
    char original_team_name[96] = {0};
    char history_text[256] = {0};

    kbo_military_history_copy_service_team_name(
        service_team_id,
        service_team_name,
        sizeof(service_team_name));
    kbo_military_history_copy_original_team_name(
        original_team_id,
        original_team_name,
        sizeof(original_team_name));
    snprintf(
        history_text,
        sizeof(history_text),
        "Selected for military service with %s from %s.",
        service_team_name,
        original_team_name);
    return kbo_record_military_player_history_text(
        player_id,
        event_yyyymmdd,
        history_text,
        source != NULL ? source : "military_selection_player_history");
}

int kbo_record_military_selection_player_history_batch(
    uint32_t event_yyyymmdd,
    uint32_t service_team_id,
    KboMilitarySelectionNewsEntry* entries,
    int entry_count,
    const char* source)
{
    if (event_yyyymmdd == 0u || service_team_id == 0u || entries == NULL || entry_count <= 0) {
        return 0;
    }

    int recorded = 0;
    for (int i = 0; i < entry_count; i++) {
        KboMilitarySelectionNewsEntry* entry = &entries[i];
        if (entry->player_id == 0u) {
            continue;
        }
        recorded += kbo_record_military_selection_player_history(
            entry->player_id,
            service_team_id,
            entry->original_team_id,
            event_yyyymmdd,
            source);
    }

    kbo_log_runtimef(
        "KBO military selection player history source=%s date=%u service_team=%u entries=%d recorded=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        service_team_id,
        entry_count,
        recorded);
    return recorded;
}

int kbo_record_military_seed_assignment_player_history(
    uint32_t player_id,
    uint32_t service_team_id,
    uint32_t original_team_id,
    uint32_t event_yyyymmdd,
    const char* source)
{
    if (player_id == 0u || service_team_id == 0u || event_yyyymmdd == 0u) {
        return 0;
    }

    char service_team_name[96] = {0};
    char original_team_name[96] = {0};
    char history_text[256] = {0};
    kbo_military_history_copy_service_team_name(
        service_team_id,
        service_team_name,
        sizeof(service_team_name));
    kbo_military_history_copy_original_team_name(
        original_team_id,
        original_team_name,
        sizeof(original_team_name));
    snprintf(
        history_text,
        sizeof(history_text),
        "Assigned to military service with %s from %s.",
        service_team_name,
        original_team_name);

    int recorded = kbo_record_military_player_history_text(
        player_id,
        event_yyyymmdd,
        history_text,
        source != NULL ? source : "military_seed_assignment_player_history");
    kbo_log_runtimef(
        "KBO military seed assignment player history source=%s player=%u date=%u service_team=%u original_team=%u recorded=%d",
        source != NULL ? source : "",
        player_id,
        event_yyyymmdd,
        service_team_id,
        original_team_id,
        recorded);
    return recorded;
}
