#include "../hotkey_window_runtime_content_internal.h"
#include "../../hotkey_window_domain_contract.h"
#include "../../../../team/lookup/team_lookup.h"

#define KBO_FA_MARKET_TEXT_MAX_ROWS KBO_FA_MARKET_CLASSIFICATION_MAX

void kbo_build_fa_cases_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;

    KboFaMarketClassification* rows = (KboFaMarketClassification*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_TEXT_MAX_ROWS * sizeof(KboFaMarketClassification));
    if (rows == NULL) {
        kbo_window_text_appendf(&buffer, "FA MARKET\r\n\r\nCould not allocate classification buffer.\r\n");
        return;
    }

    KboFaMarketScanSummary summary = {0};
    int count = kbo_collect_fa_market_classifications(
        g_kbo_hub_selected_league_id,
        rows,
        KBO_FA_MARKET_TEXT_MAX_ROWS,
        &summary,
        0,
        "f2_text_all_players");

    char league_name[128] = {0};
    kbo_hub_copy_league_display_name(summary.league_id, league_name, sizeof(league_name));

    kbo_window_text_appendf(&buffer, "FA MARKET\r\n");
    kbo_window_text_appendf(&buffer, "%s\r\n", league_name[0] != '\0' ? league_name : "Selected league");
    kbo_window_text_appendf(
        &buffer,
        "Scanned %d players / %d active teamless candidates / showing all %d rows. Seed rows: %d. CSV path: %s\r\n\r\n",
        summary.scanned,
        summary.candidates,
        count,
        summary.seed_count,
        summary.csv_path[0] != '\0' ? summary.csv_path : "-");
    kbo_window_text_appendf(&buffer, "PLAYER                   CASE            GRADE    PREV SALARY TEAM AGE RIGHTS\r\n");
    kbo_window_text_appendf(&buffer, "----------------------------------------------------------------------------------\r\n");

    for (int i = 0; i < count; i++) {
        KboFaMarketClassification* row = &rows[i];
        char team_abbrev[16] = "-";
        char rights_abbrev[16] = "-";
        char salary_text[32] = "-";
        kbo_fa_market_format_salary(row->fa_grade_salary, salary_text, sizeof(salary_text));
        kbo_hub_copy_team_abbrev_by_id(
            kbo_fa_market_display_team_id(row),
            team_abbrev,
            sizeof(team_abbrev),
            "-");
        kbo_hub_copy_team_abbrev_by_id(row->rights_team_id, rights_abbrev, sizeof(rights_abbrev), "-");
        kbo_window_text_appendf(
            &buffer,
            "%-24.24s %-15.15s %-5.5s %14s %-4.4s %3u %-6.6s\r\n",
            row->player_name,
            kbo_fa_market_display_case_label(row->case_label),
            kbo_fa_market_display_grade(row->grade),
            salary_text,
            team_abbrev,
            (uint32_t)row->age,
            rights_abbrev);
    }
    if (count == 0) {
        kbo_window_text_appendf(&buffer, "No active players without a current team found.\r\n");
    }

    HeapFree(GetProcessHeap(), 0, rows);
}

void kbo_build_fa_compensation_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    if (records == NULL) {
        kbo_window_text_appendf(&buffer, "FA COMPENSATION\r\n\r\nCould not allocate compensation buffer.\r\n");
        return;
    }

    char path[MAX_PATH] = {0};
    int count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));

    kbo_window_text_appendf(&buffer, "FA COMPENSATION\r\n");
    kbo_window_text_appendf(&buffer, "Rows: %d. CSV: %s\r\n\r\n", count, path[0] != '\0' ? path : "-");
    kbo_window_text_appendf(&buffer, "DATE     PLAYER                   GRADE FROM TO   PREV SALARY  CASH+PLAYER  CASH ONLY   PROT STATUS\r\n");
    kbo_window_text_appendf(&buffer, "------------------------------------------------------------------------------------------------\r\n");

    for (int i = 0; i < count && i < 600; i++) {
        KboFaCompensationRecord* rec = &records[i];
        char original_team[16] = "-";
        char signing_team[16] = "-";
        char previous_salary[32] = "-";
        char cash_with_player[32] = "-";
        char cash_only[32] = "-";
        kbo_hub_copy_team_abbrev_by_id(rec->original_team_id, original_team, sizeof(original_team), "-");
        kbo_hub_copy_team_abbrev_by_id(rec->signing_team_id, signing_team, sizeof(signing_team), "-");
        kbo_fa_market_format_salary(rec->previous_salary, previous_salary, sizeof(previous_salary));
        kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_with_player, sizeof(cash_with_player));
        kbo_fa_market_format_salary((int32_t)rec->cash_only, cash_only, sizeof(cash_only));
        kbo_window_text_appendf(
            &buffer,
            "%8u %-24.24s %-5.5s %-4.4s %-4.4s %12s %12s %12s %4u %-8.8s\r\n",
            rec->signed_on_yyyymmdd,
            rec->player_name,
            kbo_fa_market_display_grade(rec->grade),
            original_team,
            signing_team,
            previous_salary,
            cash_with_player,
            cash_only,
            rec->protect_count,
            kbo_fa_compensation_status_label(rec->status));
    }
    if (count == 0) {
        kbo_window_text_appendf(&buffer, "No KBO FA compensation obligations recorded.\r\n");
    } else if (count > 600) {
        kbo_window_text_appendf(&buffer, "\r\nOutput truncated. Open the CSV for the full ledger.\r\n");
    }

    HeapFree(GetProcessHeap(), 0, records);
}

void kbo_build_settings_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xec\x84\xa4\xec\xa0\x95", "설정"));
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x84\xa4\xec\xa0\x95", "리그 설정"));
    int multiplier = kbo_get_intl_established_fa_multiplier();
    kbo_window_text_appendf(&buffer, "%s\r\n", "INTERNATIONAL ESTABLISHED FA");
    kbo_window_text_appendf(
        &buffer,
        "  Multiplier: %dx (OOTP A Lot 12 -> about %d)\r\n",
        multiplier,
        12 * multiplier);
}

void kbo_build_hub_window_text(char* out, size_t out_size)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY: kbo_build_military_service_window_text(out, out_size); return;
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: kbo_build_foreign_rights_window_text(out, out_size); return;
    case KBO_HUB_VIEW_ASIAN_QUOTA:
        if (g_kbo_hub_selected_foreign_subview == KBO_HUB_FOREIGN_SUBVIEW_RIGHTS) {
            kbo_build_foreign_rights_window_text(out, out_size);
        } else {
            kbo_build_foreign_policy_hub_text(out, out_size);
        }
        return;
    case KBO_HUB_VIEW_ASIAN_GAMES: kbo_build_asian_games_hub_text(out, out_size);       return;
    case KBO_HUB_VIEW_UPCOMING_FA:
    case KBO_HUB_VIEW_FA_CASES:
        if (g_kbo_hub_selected_fa_subview == KBO_HUB_FA_SUBVIEW_COMPENSATION) {
            kbo_build_fa_compensation_hub_text(out, out_size);
        } else {
            kbo_build_fa_cases_hub_text(out, out_size);
        }
        return;
    case KBO_HUB_VIEW_MOD_INFO: kbo_build_mod_info_hub_text(out, out_size);            return;
    case KBO_HUB_VIEW_SETTINGS: kbo_build_settings_hub_text(out, out_size);            return;
    default:                    kbo_build_mod_info_hub_text(out, out_size);             return;
    }
}

void kbo_refresh_hotkey_window(void)
{
    if (g_kbo_hotkey_edit == NULL) {
        return;
    }

    const SIZE_T text_cap = 1048576u;
    char* text = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, text_cap);
    if (text == NULL) {
        SetWindowTextA(g_kbo_hotkey_edit, "Ultimate KBO\r\n\r\nCould not prepare this page.");
        return;
    }

    kbo_build_hub_window_text(text, text_cap);
    HFONT content_font = (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA
            || g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES)
        ? g_kbo_hub_font_mono : g_kbo_hub_font_body;
    if (content_font != NULL) {
        SendMessageA(g_kbo_hotkey_edit, WM_SETFONT, (WPARAM)content_font, TRUE);
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wide_len > 0) {
        WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_len) > 0) {
                SetWindowTextW(g_kbo_hotkey_edit, wide);
            } else {
                SetWindowTextA(g_kbo_hotkey_edit, text);
            }
            HeapFree(GetProcessHeap(), 0, wide);
        } else {
            SetWindowTextA(g_kbo_hotkey_edit, text);
        }
    } else {
        SetWindowTextA(g_kbo_hotkey_edit, text);
    }
    HeapFree(GetProcessHeap(), 0, text);
}

