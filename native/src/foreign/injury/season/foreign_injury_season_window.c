#include "../internal/foreign_injury_internal.h"
#include "../../../core/season/phase/capture/season_phase_capture.h"
#include "../../../core/season/phase/season_phase.h"

static LONG g_kbo_foreign_injury_season_window_block_log_count = 0;

typedef struct KboForeignInjurySeasonWindowCacheEntry {
    uint32_t league_id;
    uint32_t today_yyyymmdd;
    LONG phase_capture_sequence;
    DWORD tick;
    uint8_t result;
    uint8_t valid;
    KboSeasonPhaseInfo phase_info;
} KboForeignInjurySeasonWindowCacheEntry;

enum {
    KBO_FOREIGN_INJURY_SEASON_WINDOW_CACHE_SIZE = 32
};

static KboForeignInjurySeasonWindowCacheEntry
    g_kbo_foreign_injury_season_window_cache[KBO_FOREIGN_INJURY_SEASON_WINDOW_CACHE_SIZE];
static KboForeignInjurySeasonWindowCacheEntry
    g_kbo_foreign_injury_close_window_cache[KBO_FOREIGN_INJURY_SEASON_WINDOW_CACHE_SIZE];

static uint32_t kbo_foreign_injury_season_window_cache_slot(uint32_t league_id, uint32_t today_yyyymmdd)
{
    uint32_t h = league_id * 2654435761u;
    h ^= today_yyyymmdd * 2246822519u;
    h ^= h >> 16;
    return h & (KBO_FOREIGN_INJURY_SEASON_WINDOW_CACHE_SIZE - 1u);
}

static int kbo_foreign_injury_season_window_cache_get(
    KboForeignInjurySeasonWindowCacheEntry* entries,
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    int* out_result)
{
    if (out_result != NULL) {
        *out_result = 0;
    }
    if (league_id == 0u || today_yyyymmdd == 0u || out_result == NULL) {
        return 0;
    }

    LONG phase_capture_sequence = InterlockedCompareExchange(
        &g_kbo_season_phase_capture_event_published_sequence,
        0,
        0);
    KboForeignInjurySeasonWindowCacheEntry* entry =
        &entries[kbo_foreign_injury_season_window_cache_slot(league_id, today_yyyymmdd)];
    if (!entry->valid
            || entry->league_id != league_id
            || entry->today_yyyymmdd != today_yyyymmdd
            || entry->phase_capture_sequence != phase_capture_sequence
            || entry->tick == 0u) {
        return 0;
    }
    *out_result = entry->result ? 1 : 0;
    return 1;
}

static void kbo_foreign_injury_season_window_cache_store(
    KboForeignInjurySeasonWindowCacheEntry* entries,
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    int result,
    const KboSeasonPhaseInfo* phase_info)
{
    if (league_id == 0u || today_yyyymmdd == 0u) {
        return;
    }

    KboForeignInjurySeasonWindowCacheEntry* entry =
        &entries[kbo_foreign_injury_season_window_cache_slot(league_id, today_yyyymmdd)];
    entry->valid = 0u;
    entry->league_id = league_id;
    entry->today_yyyymmdd = today_yyyymmdd;
    entry->phase_capture_sequence = InterlockedCompareExchange(
        &g_kbo_season_phase_capture_event_published_sequence,
        0,
        0);
    entry->tick = GetTickCount();
    entry->result = result ? 1u : 0u;
    if (phase_info != NULL) {
        entry->phase_info = *phase_info;
    } else {
        memset(&entry->phase_info, 0, sizeof(entry->phase_info));
    }
    entry->valid = 1u;
}

int kbo_foreign_injury_replacement_in_season_window(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    const char* source,
    const char* context)
{
    KBO_PROFILE_BEGIN(profile_foreign_injury_season_window);
    if (league_id == 0u || today_yyyymmdd == 0u) {
        KBO_PROFILE_END(profile_foreign_injury_season_window, "foreign_injury.season_window.invalid");
        return 0;
    }

    int cached_result = 0;
    if (kbo_foreign_injury_season_window_cache_get(
            g_kbo_foreign_injury_season_window_cache,
            league_id,
            today_yyyymmdd,
            &cached_result)) {
        KBO_PROFILE_END(profile_foreign_injury_season_window, cached_result
            ? "foreign_injury.season_window.cache_hit_allowed"
            : "foreign_injury.season_window.cache_hit_blocked");
        return cached_result;
    }

    KboSeasonPhaseInfo phase_info;
    memset(&phase_info, 0, sizeof(phase_info));
    if (!kbo_season_phase_resolve(league_id, today_yyyymmdd, 0u, &phase_info)) {
        kbo_foreign_injury_season_window_cache_store(
            g_kbo_foreign_injury_season_window_cache,
            league_id,
            today_yyyymmdd,
            0,
            &phase_info);
        LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_season_window_block_log_count);
        if (log_slot <= 40 || (log_slot % 200) == 0) {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.season_window",
                "block",
                "season_phase_unavailable",
                source,
                &audit_fields);
            kbo_log_runtimef(
                "foreign injury replacement: blocked outside in-season context=%s source=%s league=%u date=%u reason=season_phase_unavailable",
                context != NULL ? context : "",
                source != NULL ? source : "",
                league_id,
                today_yyyymmdd);
        }
        KBO_PROFILE_END(profile_foreign_injury_season_window, "foreign_injury.season_window.unavailable");
        return 0;
    }

    if (kbo_foreign_injury_replacement_phase_allows_signing(phase_info.effective_phase)) {
        kbo_foreign_injury_season_window_cache_store(
            g_kbo_foreign_injury_season_window_cache,
            league_id,
            today_yyyymmdd,
            1,
            &phase_info);
        KBO_PROFILE_END(profile_foreign_injury_season_window, "foreign_injury.season_window.allowed");
        return 1;
    }

    kbo_foreign_injury_season_window_cache_store(
        g_kbo_foreign_injury_season_window_cache,
        league_id,
        today_yyyymmdd,
        0,
        &phase_info);
    LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_season_window_block_log_count);
    if (log_slot <= 40 || (log_slot % 200) == 0) {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", today_yyyymmdd);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_u32(&audit_fields, "effective_phase", (uint32_t)phase_info.effective_phase);
        kbo_log_field_u32(&audit_fields, "raw_phase", (uint32_t)phase_info.raw_phase);
        kbo_log_field_u32(&audit_fields, "opening_day", phase_info.opening_day);
        kbo_rule_audit_emit_fields(
            "foreign_injury.replacement.season_window",
            "block",
            kbo_season_phase_label(phase_info.effective_phase),
            source,
            &audit_fields);
        kbo_log_runtimef(
            "foreign injury replacement: blocked outside in-season context=%s source=%s league=%u date=%u phase=%s raw=%u opening_day=%u",
            context != NULL ? context : "",
            source != NULL ? source : "",
            league_id,
            today_yyyymmdd,
            kbo_season_phase_label(phase_info.effective_phase),
            (uint32_t)phase_info.raw_phase,
            phase_info.opening_day);
    }
    KBO_PROFILE_END(profile_foreign_injury_season_window, "foreign_injury.season_window.blocked");
    return 0;
}

int kbo_foreign_injury_replacement_close_decision_allowed(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    const char* source,
    const char* context)
{
    KBO_PROFILE_BEGIN(profile_foreign_injury_season_window);
    if (league_id == 0u || today_yyyymmdd == 0u) {
        KBO_PROFILE_END(profile_foreign_injury_season_window, "foreign_injury.close_window.invalid");
        return 0;
    }

    int cached_result = 0;
    if (kbo_foreign_injury_season_window_cache_get(
            g_kbo_foreign_injury_close_window_cache,
            league_id,
            today_yyyymmdd,
            &cached_result)) {
        KBO_PROFILE_END(profile_foreign_injury_season_window, cached_result
            ? "foreign_injury.close_window.cache_hit_allowed"
            : "foreign_injury.close_window.cache_hit_blocked");
        return cached_result;
    }

    KboSeasonPhaseInfo phase_info;
    memset(&phase_info, 0, sizeof(phase_info));
    if (!kbo_season_phase_resolve(league_id, today_yyyymmdd, 0u, &phase_info)) {
        kbo_foreign_injury_season_window_cache_store(
            g_kbo_foreign_injury_close_window_cache,
            league_id,
            today_yyyymmdd,
            0,
            &phase_info);
        LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_season_window_block_log_count);
        if (log_slot <= 40 || (log_slot % 200) == 0) {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", today_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_rule_audit_emit_fields(
                "foreign_injury.replacement.close_window",
                "block",
                "season_phase_unavailable",
                source,
                &audit_fields);
            kbo_log_runtimef(
                "foreign injury replacement: blocked close decision context=%s source=%s league=%u date=%u reason=season_phase_unavailable",
                context != NULL ? context : "",
                source != NULL ? source : "",
                league_id,
                today_yyyymmdd);
        }
        KBO_PROFILE_END(profile_foreign_injury_season_window, "foreign_injury.close_window.unavailable");
        return 0;
    }

    if (kbo_foreign_injury_replacement_phase_allows_close(phase_info.effective_phase)) {
        kbo_foreign_injury_season_window_cache_store(
            g_kbo_foreign_injury_close_window_cache,
            league_id,
            today_yyyymmdd,
            1,
            &phase_info);
        KBO_PROFILE_END(profile_foreign_injury_season_window, "foreign_injury.close_window.allowed");
        return 1;
    }

    kbo_foreign_injury_season_window_cache_store(
        g_kbo_foreign_injury_close_window_cache,
        league_id,
        today_yyyymmdd,
        0,
        &phase_info);
    LONG log_slot = InterlockedIncrement(&g_kbo_foreign_injury_season_window_block_log_count);
    if (log_slot <= 40 || (log_slot % 200) == 0) {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", today_yyyymmdd);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_u32(&audit_fields, "effective_phase", (uint32_t)phase_info.effective_phase);
        kbo_log_field_u32(&audit_fields, "raw_phase", (uint32_t)phase_info.raw_phase);
        kbo_log_field_u32(&audit_fields, "opening_day", phase_info.opening_day);
        kbo_rule_audit_emit_fields(
            "foreign_injury.replacement.close_window",
            "block",
            kbo_season_phase_label(phase_info.effective_phase),
            source,
            &audit_fields);
        kbo_log_runtimef(
            "foreign injury replacement: blocked close decision context=%s source=%s league=%u date=%u phase=%s raw=%u opening_day=%u",
            context != NULL ? context : "",
            source != NULL ? source : "",
            league_id,
            today_yyyymmdd,
            kbo_season_phase_label(phase_info.effective_phase),
            (uint32_t)phase_info.raw_phase,
            phase_info.opening_day);
    }
    KBO_PROFILE_END(profile_foreign_injury_season_window, "foreign_injury.close_window.blocked");
    return 0;
}
