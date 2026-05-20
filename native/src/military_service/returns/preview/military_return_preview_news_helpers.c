#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/news/ledger/core_news_ledger.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_name_cache.h"
#include "../../calendar/military_service_date.h"
#include "../../players/state/military_player_state.h"
#include "../../selection/events/military_selection_event.h"
#include "../../selection/news/military_selection_news.h"
#include "military_return_preview_news_helpers.h"

#define KBO_MILITARY_RETURN_PREVIEW_NEWS_LEDGER_DOMAIN "military_return_preview"

int kbo_military_return_preview_marker_exists(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return 1;
    }
    char path[MAX_PATH] = {0};
    if (!kbo_custom_news_ledger_path(path, sizeof(path))) {
        return 1;
    }
    return kbo_custom_news_ledger_completed(KBO_MILITARY_RETURN_PREVIEW_NEWS_LEDGER_DOMAIN, key);
}

void kbo_military_return_preview_persist_marker(const char* key, const char* source)
{
    if (key == NULL || key[0] == '\0' || kbo_military_return_preview_marker_exists(key)) {
        return;
    }
    kbo_custom_news_ledger_record_completed(
        KBO_MILITARY_RETURN_PREVIEW_NEWS_LEDGER_DOMAIN,
        key,
        "news_marker_persist",
        source);
}

static int kbo_military_return_preview_compare(
    const KboMilitaryReturnPreviewEntry* a,
    const KboMilitaryReturnPreviewEntry* b)
{
    if (a->score != b->score) {
        return a->score > b->score ? -1 : 1;
    }
    if (a->return_yyyymmdd != b->return_yyyymmdd) {
        return a->return_yyyymmdd < b->return_yyyymmdd ? -1 : 1;
    }
    if (a->player_id == b->player_id) {
        return 0;
    }
    return a->player_id < b->player_id ? -1 : 1;
}

static void kbo_military_return_preview_sort(
    KboMilitaryReturnPreviewEntry* entries,
    int count)
{
    if (entries == NULL || count <= 1) {
        return;
    }

    for (int i = 1; i < count; i++) {
        KboMilitaryReturnPreviewEntry key = entries[i];
        int j = i - 1;
        while (j >= 0 && kbo_military_return_preview_compare(&key, &entries[j]) < 0) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = key;
    }
}

static void kbo_military_return_preview_add_or_replace(
    KboMilitaryReturnPreviewEntry* entries,
    int* count,
    const KboMilitaryReturnPreviewEntry* candidate)
{
    if (entries == NULL || count == NULL || candidate == NULL || candidate->player_id == 0u) {
        return;
    }

    for (int i = 0; i < *count; i++) {
        if (entries[i].player_id == candidate->player_id) {
            entries[i] = *candidate;
            return;
        }
    }

    if (*count < KBO_MILITARY_RETURN_PREVIEW_MAX) {
        entries[*count] = *candidate;
        (*count)++;
        return;
    }

    int worst = 0;
    for (int i = 1; i < *count; i++) {
        if (kbo_military_return_preview_compare(&entries[i], &entries[worst]) > 0) {
            worst = i;
        }
    }
    if (kbo_military_return_preview_compare(candidate, &entries[worst]) < 0) {
        entries[worst] = *candidate;
    }
}

int kbo_military_return_preview_collect(
    uint32_t today_serial,
    uint32_t return_year,
    KboMilitaryReturnPreviewEntry* entries,
    int max_entries)
{
    if (today_serial == 0u || return_year == 0u || entries == NULL || max_entries <= 0) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb = find_kbo_team_by_csv_id_any_league("KPB", 0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
    uint32_t kpb_id = kpb != NULL ? *(uint32_t*)(kpb + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
    if (sang_id == 0u && kpb_id == 0u) {
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        uint32_t service_team_id = 0u;
        if (current_team_id == sang_id || current_team_id == kpb_id) {
            service_team_id = current_team_id;
        } else if (loan_team_id == sang_id || loan_team_id == kpb_id) {
            service_team_id = loan_team_id;
        }
        if (service_team_id == 0u) {
            continue;
        }

        int32_t days_left = kbo_military_effective_days_left(player);
        if (days_left <= 0) {
            continue;
        }

        uint32_t return_yyyymmdd = kbo_military_effective_return_yyyymmdd(player);
        if (return_yyyymmdd == 0u || return_yyyymmdd / 10000u != return_year) {
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        kbo_military_resolve_original_team(
            player,
            service_team_id,
            sang_id,
            kpb_id,
            &original_team_id,
            &original_league_id);
        (void)original_league_id;

        KboMilitaryReturnPreviewEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        entry.original_team_id = original_team_id;
        entry.return_yyyymmdd = return_yyyymmdd;
        entry.days_left = days_left;
        entry.score = kbo_military_draft_candidate_score(player);
        entry.player_ptr = player_ptr;
        kbo_military_return_preview_add_or_replace(entries, &count, &entry);
    }

    kbo_military_return_preview_sort(entries, count);
    return count;
}

void kbo_military_return_preview_player_name(
    const KboMilitaryReturnPreviewEntry* entry,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (entry == NULL || entry->player_id == 0u) {
        return;
    }
    if (kbo_player_pointer_plausible(entry->player_ptr)) {
        kbo_copy_player_display_name((uint8_t*)entry->player_ptr, out, out_size);
    }
    if (out[0] == '\0') {
        snprintf(out, out_size, "Player #%u", entry->player_id);
    }
}

int kbo_military_return_preview_render_body(
    const KboMilitaryReturnPreviewEntry* entries,
    int entry_count,
    char* out,
    size_t out_size,
    const char* source)
{
    if (entries == NULL || entry_count <= 0 || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    const KboMilitaryReturnPreviewEntry* lead = &entries[0];
    char lead_player_link[96] = {0};
    char lead_team_link[128] = {0};
    char lead_return_date[16] = {0};
    char days_left_text[16] = {0};
    char return_count_text[16] = {0};
    kbo_military_copy_player_link(lead->player_id, lead->player_ptr, lead_player_link, sizeof(lead_player_link));
    kbo_military_copy_team_link(lead->original_team_id, lead_team_link, sizeof(lead_team_link));
    kbo_military_format_yyyymmdd(lead->return_yyyymmdd, lead_return_date, sizeof(lead_return_date));
    snprintf(days_left_text, sizeof(days_left_text), "%d", (int)lead->days_left);
    snprintf(return_count_text, sizeof(return_count_text), "%d", entry_count);

    KboNewsTemplateVar intro_vars[] = {
        { "lead_player_link", lead_player_link },
        { "lead_team_link", lead_team_link },
        { "return_date", lead_return_date },
        { "days_left", days_left_text },
        { "return_count", return_count_text },
        { "return_count_plural", entry_count == 1 ? "" : "s" },
    };
    if (!kbo_news_template_render_key(
            "military.return_preview.intro",
            intro_vars,
            (int)(sizeof(intro_vars) / sizeof(intro_vars[0])),
            out,
            out_size,
            source)) {
        return 0;
    }

    char line_template[256] = {0};
    if (!kbo_news_template_load("military.return_preview.line", line_template, sizeof(line_template), NULL, 0u, source)) {
        return 0;
    }

    int list_count = entry_count < KBO_MILITARY_RETURN_PREVIEW_LIST_MAX
        ? entry_count
        : KBO_MILITARY_RETURN_PREVIEW_LIST_MAX;
    for (int i = 0; i < list_count && strlen(out) + 240u < out_size; i++) {
        char index_text[16] = {0};
        char player_link[96] = {0};
        char team_link[128] = {0};
        char return_date[16] = {0};
        snprintf(index_text, sizeof(index_text), "%d", i + 1);
        kbo_military_copy_player_link(entries[i].player_id, entries[i].player_ptr, player_link, sizeof(player_link));
        kbo_military_copy_team_link(entries[i].original_team_id, team_link, sizeof(team_link));
        kbo_military_format_yyyymmdd(entries[i].return_yyyymmdd, return_date, sizeof(return_date));
        KboNewsTemplateVar line_vars[] = {
            { "index", index_text },
            { "player_link", player_link },
            { "team_separator", team_link[0] != '\0' ? ", " : "" },
            { "team_link", team_link },
            { "return_date", return_date },
        };
        char rendered_line[256] = {0};
        kbo_news_template_render(
            line_template,
            line_vars,
            (int)(sizeof(line_vars) / sizeof(line_vars[0])),
            rendered_line,
            sizeof(rendered_line));
        kbo_news_text_append(out, out_size, rendered_line);
    }

    int more_count = entry_count - list_count;
    if (more_count > 0 && strlen(out) + 96u < out_size) {
        char more_template[160] = {0};
        char more_count_text[16] = {0};
        snprintf(more_count_text, sizeof(more_count_text), "%d", more_count);
        if (kbo_news_template_load("military.return_preview.more_line", more_template, sizeof(more_template), NULL, 0u, source)) {
            KboNewsTemplateVar more_vars[] = {
                { "more_count", more_count_text },
                { "more_count_plural", more_count == 1 ? "" : "s" },
            };
            char rendered_more[192] = {0};
            kbo_news_template_render(
                more_template,
                more_vars,
                (int)(sizeof(more_vars) / sizeof(more_vars[0])),
                rendered_more,
                sizeof(rendered_more));
            kbo_news_text_append(out, out_size, rendered_more);
        }
    }

    if (strlen(out) + 256u < out_size) {
        char outro[384] = {0};
        if (kbo_news_template_render_key("military.return_preview.outro", NULL, 0, outro, sizeof(outro), source)) {
            kbo_news_text_append(out, out_size, outro);
        }
    }
    return out[0] != '\0';
}
