#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/csv/core_csv.h"
#include "../../custom_events/runtime/dates/custom_event_dates.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/news/templates/core_news_templates.h"
#include "../../core/logging/core_log.h"
#include "../../core/logging/rule_audit.h"
#include "foreign_waiver_announcements.h"
#include "../common/paths/foreign_waiver_paths.h"
#include "../common/policy/foreign_waiver_policy.h"
#include "foreign_waiver_results.h"
#include "../rights/query/foreign_waiver_rights_query.h"

static uint32_t g_kbo_foreign_waiver_last_result_announcement = 0;

static int kbo_build_foreign_waiver_result_body(char* out, size_t out_size, uint32_t today_yyyymmdd, const char* source)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    int retained = 0;
    int skipped = 0;
    int ai_retained = 0;
    int ai_skipped = 0;
    int user_retained = 0;
    int user_skipped = 0;
    int active_rights = 0;
    int decision_breakdown_available = 0;

    kbo_lock_enter(&g_kbo_foreign_waiver_rights_lock);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        if (kbo_is_foreign_waiver_right_active(&g_kbo_foreign_waiver_rights[i], today_yyyymmdd)) {
            active_rights++;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_waiver_rights_lock);

    char decision_path[MAX_PATH] = {0};
    if (get_kbo_foreign_waiver_decisions_path(decision_path, sizeof(decision_path))) {
        KboCsvReader* reader = kbo_csv_reader_open(decision_path);
        if (reader != NULL) {
            while (kbo_csv_reader_next_row(reader)) {
                char fields[10][64];
                int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 10);
                if (field_count < 5 || fields[0][0] < '0' || fields[0][0] > '9') {
                    continue;
                }

                uint32_t window_end = kbo_csv_parse_u32_text(fields[2], 10);
                if (window_end == today_yyyymmdd) {
                    const char* source_name = fields[3];
                    const char* action_name = fields[4];
                    if (_stricmp(action_name, "RETAIN") == 0) {
                        retained++;
                        decision_breakdown_available = 1;
                        if (_stricmp(source_name, "ai") == 0) { ai_retained++; }
                        else if (_stricmp(source_name, "user") == 0) { user_retained++; }
                    } else if (_stricmp(action_name, "SKIP") == 0) {
                        skipped++;
                        decision_breakdown_available = 1;
                        if (_stricmp(source_name, "ai") == 0) { ai_skipped++; }
                        else if (_stricmp(source_name, "user") == 0) { user_skipped++; }
                    }
                }
            }
            kbo_csv_reader_close(reader);
        }
    }

    if (retained == 0 && skipped == 0) {
        char rights_path[MAX_PATH] = {0};
        if (kbo_get_foreign_waiver_rights_path(rights_path, sizeof(rights_path))) {
            KboCsvReader* reader = kbo_csv_reader_open(rights_path);
            if (reader != NULL) {
                while (kbo_csv_reader_next_row(reader)) {
                    char fields[5][64];
                    int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 5);
                    if (field_count < 5 || fields[0][0] < '0' || fields[0][0] > '9') {
                        continue;
                    }

                    uint32_t retained_on = kbo_csv_parse_u32_text(fields[3], 10);
                    uint32_t expires_on = kbo_csv_parse_u32_text(fields[4], 10);
                    if (retained_on <= today_yyyymmdd && expires_on >= today_yyyymmdd) {
                        retained++;
                    }
                }
                kbo_csv_reader_close(reader);
            }
        }
    }

    char retained_text[16] = {0};
    char skipped_text[16] = {0};
    char ai_retained_text[16] = {0};
    char ai_skipped_text[16] = {0};
    char user_retained_text[16] = {0};
    char user_skipped_text[16] = {0};
    char active_rights_text[16] = {0};
    snprintf(retained_text, sizeof(retained_text), "%d", retained);
    snprintf(skipped_text, sizeof(skipped_text), "%d", skipped);
    snprintf(ai_retained_text, sizeof(ai_retained_text), "%d", ai_retained);
    snprintf(ai_skipped_text, sizeof(ai_skipped_text), "%d", ai_skipped);
    snprintf(user_retained_text, sizeof(user_retained_text), "%d", user_retained);
    snprintf(user_skipped_text, sizeof(user_skipped_text), "%d", user_skipped);
    snprintf(active_rights_text, sizeof(active_rights_text), "%d", active_rights);
    KboNewsTemplateVar vars[] = {
        { "retained", retained_text },
        { "skipped", skipped_text },
        { "ai_retained", ai_retained_text },
        { "ai_skipped", ai_skipped_text },
        { "user_retained", user_retained_text },
        { "user_skipped", user_skipped_text },
        { "active_rights", active_rights_text },
    };
    return kbo_news_template_render_key(
        decision_breakdown_available
            ? "foreign_waiver.results.body_with_breakdown"
            : "foreign_waiver.results.body_without_breakdown",
        vars,
        (int)(sizeof(vars) / sizeof(vars[0])),
        out,
        out_size,
        source);
}

int kbo_announce_foreign_waiver_results(uint32_t event_yyyymmdd, const char* source)
{
    if (event_yyyymmdd == 0u || g_kbo_foreign_waiver_last_result_announcement == event_yyyymmdd
            || kbo_foreign_waiver_announcement_recorded(event_yyyymmdd)) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", event_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "last_date", g_kbo_foreign_waiver_last_result_announcement);
            kbo_rule_audit_emit_fields(
                "foreign_waiver.results",
                "skip",
                "already_announced_or_bad_date",
                source,
                &audit_fields);
        } while (0);
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", event_yyyymmdd);
            kbo_rule_audit_emit_fields(
                "foreign_waiver.results",
                "skip",
                "league_id_unavailable",
                source,
                &audit_fields);
        } while (0);
        return 0;
    }

    uint32_t news_yyyymmdd = kbo_custom_event_effective_news_date(event_yyyymmdd);
    char body[1024] = {0};
    kbo_load_foreign_waiver_rights();
    if (!kbo_build_foreign_waiver_result_body(body, sizeof(body), event_yyyymmdd, source)) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", event_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_rule_audit_emit_fields(
                "foreign_waiver.results",
                "skip",
                "body_template_unavailable",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign reserve rights: result announcement skipped source=%s date=%u reason=body_template_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd);
        return 0;
    }

    char title[160] = {0};
    if (!kbo_news_template_render_key("foreign_waiver.results.title", NULL, 0, title, sizeof(title), source)) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", event_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_rule_audit_emit_fields(
                "foreign_waiver.results",
                "skip",
                "title_template_unavailable",
                source,
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "foreign reserve rights: result announcement skipped source=%s date=%u reason=title_template_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        news_yyyymmdd / 10000u,
        (news_yyyymmdd / 100u) % 100u,
        news_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    kbo_record_foreign_waiver_announcement(event_yyyymmdd);
    kbo_record_foreign_waiver_announcement_body(event_yyyymmdd, source, body);
    g_kbo_foreign_waiver_last_result_announcement = event_yyyymmdd;
    kbo_log_runtimef(
        "foreign reserve rights: result announcement source=%s event_date=%u news_date=%u created=%d mode=file_recorded_native_news_disabled body=%s",
        source != NULL ? source : "",
        event_yyyymmdd,
        news_yyyymmdd,
        created,
        body);
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", event_yyyymmdd);
        kbo_log_field_u32(&audit_fields, "news_date", news_yyyymmdd);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_i32(&audit_fields, "created", created);
        kbo_rule_audit_emit_fields(
            "foreign_waiver.results",
            created ? "emit_news" : "record_only",
            "result_announcement",
            source,
            &audit_fields);
    } while (0);
    return 1;
}
