#include "ui_fa_cases_view_internal.h"

#include <stdio.h>
#include <string.h>

enum {
    KBO_FA_MARKET_FILTER_NONE = 0,
    KBO_FA_MARKET_FILTER_OFFICIAL = 1,
    KBO_FA_MARKET_FILTER_COMPENSABLE = 2,
    KBO_FA_MARKET_FILTER_DOMESTIC = 3,
    KBO_FA_MARKET_FILTER_FOREIGN = 4,
    KBO_FA_MARKET_FILTER_GRADE_A = 5,
    KBO_FA_MARKET_FILTER_GRADE_B = 6,
    KBO_FA_MARKET_FILTER_RIGHTS = 7
};

enum {
    KBO_FA_MARKET_POSITION_ALL = 0,
    KBO_FA_MARKET_POSITION_PITCHERS = 1,
    KBO_FA_MARKET_POSITION_STARTERS = 2,
    KBO_FA_MARKET_POSITION_RELIEVERS = 3,
    KBO_FA_MARKET_POSITION_CATCHERS = 4,
    KBO_FA_MARKET_POSITION_INFIELDERS = 5,
    KBO_FA_MARKET_POSITION_OUTFIELDERS = 6,
    KBO_FA_MARKET_POSITION_1B = 7,
    KBO_FA_MARKET_POSITION_2B = 8,
    KBO_FA_MARKET_POSITION_3B = 9,
    KBO_FA_MARKET_POSITION_SS = 10,
    KBO_FA_MARKET_POSITION_LF = 11,
    KBO_FA_MARKET_POSITION_CF = 12,
    KBO_FA_MARKET_POSITION_RF = 13,
    KBO_FA_MARKET_POSITION_DH = 14
};

static const char* kbo_fa_market_filter_label(int filter)
{
    switch (filter) {
    case KBO_FA_MARKET_FILTER_OFFICIAL: return "Official FA";
    case KBO_FA_MARKET_FILTER_COMPENSABLE: return "Compensation";
    case KBO_FA_MARKET_FILTER_DOMESTIC: return "Domestic";
    case KBO_FA_MARKET_FILTER_FOREIGN: return "Foreign";
    case KBO_FA_MARKET_FILTER_GRADE_A: return "Grade A";
    case KBO_FA_MARKET_FILTER_GRADE_B: return "Grade B";
    case KBO_FA_MARKET_FILTER_RIGHTS: return "Rights";
    default: return "None";
    }
}

static const char* kbo_fa_market_position_filter_label(int position_filter)
{
    switch (position_filter) {
    case KBO_FA_MARKET_POSITION_PITCHERS: return "Pitchers";
    case KBO_FA_MARKET_POSITION_STARTERS: return "Starters";
    case KBO_FA_MARKET_POSITION_RELIEVERS: return "Relievers";
    case KBO_FA_MARKET_POSITION_CATCHERS: return "Catchers";
    case KBO_FA_MARKET_POSITION_INFIELDERS: return "Infielders";
    case KBO_FA_MARKET_POSITION_OUTFIELDERS: return "Outfielders";
    case KBO_FA_MARKET_POSITION_1B: return "1B";
    case KBO_FA_MARKET_POSITION_2B: return "2B";
    case KBO_FA_MARKET_POSITION_3B: return "3B";
    case KBO_FA_MARKET_POSITION_SS: return "SS";
    case KBO_FA_MARKET_POSITION_LF: return "LF";
    case KBO_FA_MARKET_POSITION_CF: return "CF";
    case KBO_FA_MARKET_POSITION_RF: return "RF";
    case KBO_FA_MARKET_POSITION_DH: return "DH";
    default: return "All Players";
    }
}

static int kbo_fa_market_row_is_compensable(const KboFaMarketClassification* row)
{
    return row != NULL
        && (strcmp(row->case_label, "KBO_FA_APPROVED") == 0
            || strcmp(row->case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
            || strcmp(row->case_label, "KBO_FA_DEFERRED") == 0
            || strcmp(row->case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
            || strcmp(row->case_label, "KBO_FA_CARRYOVER_UNSIGNED") == 0);
}

int kbo_fa_market_row_matches_filter(const KboFaMarketClassification* row, int filter)
{
    if (row == NULL) {
        return 0;
    }
    switch (filter) {
    case KBO_FA_MARKET_FILTER_OFFICIAL:
        return strcmp(row->case_label, "KBO_FA_APPROVED") == 0;
    case KBO_FA_MARKET_FILTER_COMPENSABLE:
        return kbo_fa_market_row_is_compensable(row);
    case KBO_FA_MARKET_FILTER_DOMESTIC:
        return row->foreign_player == 0u;
    case KBO_FA_MARKET_FILTER_FOREIGN:
        return row->foreign_player != 0u;
    case KBO_FA_MARKET_FILTER_GRADE_A:
        return _stricmp(row->grade, "A") == 0;
    case KBO_FA_MARKET_FILTER_GRADE_B:
        return _stricmp(row->grade, "B") == 0;
    case KBO_FA_MARKET_FILTER_RIGHTS:
        return row->rights_team_id != 0u;
    default:
        return 1;
    }
}

int kbo_fa_market_row_matches_position_filter(
    const KboFaMarketClassification* row,
    int position_filter)
{
    if (row == NULL) {
        return 0;
    }
    uint8_t position = row->position_group;
    uint8_t role = row->position_role;
    switch (position_filter) {
    case KBO_FA_MARKET_POSITION_PITCHERS:
        return position == 1u || role == 1u || role == 11u || role == 12u || role == 13u;
    case KBO_FA_MARKET_POSITION_STARTERS:
        return position == 1u && role == 11u;
    case KBO_FA_MARKET_POSITION_RELIEVERS:
        return position == 1u && (role == 12u || role == 13u);
    case KBO_FA_MARKET_POSITION_CATCHERS:
        return position == 2u || role == 2u;
    case KBO_FA_MARKET_POSITION_INFIELDERS:
        return position >= 3u && position <= 6u;
    case KBO_FA_MARKET_POSITION_OUTFIELDERS:
        return position >= 7u && position <= 9u;
    case KBO_FA_MARKET_POSITION_1B:
        return position == 3u || role == 3u;
    case KBO_FA_MARKET_POSITION_2B:
        return position == 4u || role == 4u;
    case KBO_FA_MARKET_POSITION_3B:
        return position == 5u || role == 5u;
    case KBO_FA_MARKET_POSITION_SS:
        return position == 6u || role == 6u;
    case KBO_FA_MARKET_POSITION_LF:
        return position == 7u || role == 7u;
    case KBO_FA_MARKET_POSITION_CF:
        return position == 8u || role == 8u;
    case KBO_FA_MARKET_POSITION_RF:
        return position == 9u || role == 9u;
    case KBO_FA_MARKET_POSITION_DH:
        return position == 10u || role == 10u;
    default:
        return 1;
    }
}

static void kbo_webview_append_fa_market_choice_option(
    KboWindowTextBuffer* buffer,
    const char* href,
    const char* label,
    int selected)
{
    kbo_window_text_appendf(buffer, "<a class='faFilterOption%s' href='", selected ? " selected" : "");
    kbo_html_append_escaped(buffer, href);
    kbo_window_text_appendf(buffer, "'>");
    kbo_html_append_escaped(buffer, label);
    kbo_window_text_appendf(buffer, "</a>");
}

static void kbo_webview_append_fa_market_position_option(
    KboWindowTextBuffer* buffer,
    int value,
    const char* label)
{
    char href[64];
    snprintf(href, sizeof(href), "kbo://fa-market/position/%d", value);
    kbo_webview_append_fa_market_choice_option(
        buffer,
        href,
        label,
        g_kbo_hub_fa_market_position_filter == value);
}

void kbo_webview_append_fa_market_filter_bar(
    KboWindowTextBuffer* buffer,
    int filtered_rows,
    int total_rows)
{
    kbo_window_text_appendf(buffer,
        "<style>"
        ".faMarketView{display:grid!important;grid-template-rows:34px minmax(0,1fr);gap:6px!important;height:100%%!important;min-height:0;overflow:visible}"
        ".faMarketView>.faFilterBar,.faMarketView>.rosterTopBar{min-height:0;flex:none!important}"
        ".faMarketView>.faFilterBar{position:relative;z-index:30}"
        ".faMarketView>.rosterTopBar{height:34px;padding:0 8px;position:relative;z-index:10}"
        ".faMarketView>.faMarketTableWrap{height:auto!important;min-height:0;margin:0!important;align-self:stretch;position:relative;z-index:1;overflow-y:auto;overflow-x:hidden}"
        ".faFilterBar{height:34px;display:flex;align-items:center;gap:8px;padding:0 7px;background:#1f1f1f;border:1px solid #303030;border-radius:4px;overflow:visible;flex:0 0 auto}"
        ".faFilterSelect{position:relative;width:150px;height:26px;color:#f2f2f2;font-family:var(--ui-font);font-size:13px;font-weight:900;line-height:26px}.faFilterSelect.wide{width:194px}"
        ".faFilterSelect summary{height:26px;display:flex;align-items:center;justify-content:space-between;gap:8px;padding:0 24px 0 9px;border:1px solid #5a5f61;border-radius:3px;background:#252829;list-style:none;cursor:pointer;outline:0;text-transform:uppercase}.faFilterSelect summary::-webkit-details-marker{display:none}.faFilterSelect summary:after{content:'';position:absolute;right:9px;top:10px;border-left:5px solid transparent;border-right:5px solid transparent;border-top:6px solid #e6e6e6}.faFilterSelect[open] summary{background:#303436;border-color:#8a8f91}.faFilterSelect[open] summary:after{border-top:0;border-bottom:6px solid #fff}"
        ".faFilterMenu{position:absolute;left:0;right:0;top:29px;z-index:60;max-height:230px;overflow-y:auto;overflow-x:hidden;padding:3px 0;border:1px solid #42474a;border-radius:3px;background:#242729;box-shadow:0 8px 18px rgba(0,0,0,.55)}.faFilterOption{display:block;height:24px;line-height:24px;padding:0 9px;color:#efefef;text-decoration:none;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.faFilterOption:hover,.faFilterOption.selected{background:#30434b;color:#fff}"
        ".faFilterSpacer{flex:1 1 auto}.faFilterSummary{color:#f2f2f2;font-size:13px;font-weight:900;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".faCasesTable .roGrade{width:58px;position:relative;overflow:visible!important}.faGradeValue{display:inline-flex;align-items:center;min-width:24px;font-weight:900;color:#ffb13b}.faGradeTooltip{position:absolute;left:5px;top:24px;z-index:95;display:none;min-width:158px;padding:7px 9px;border:1px solid #454545;border-radius:3px;background:#171717;color:#f4f4f4;box-shadow:0 8px 18px rgba(0,0,0,.62);font-size:12px;font-weight:800;line-height:1.35;pointer-events:none}.faGradeTipRow{display:flex;align-items:center;justify-content:space-between;gap:14px;white-space:nowrap}.faGradeTipRow+ .faGradeTipRow{margin-top:3px}.faGradeTipLabel{color:#aaa;font-weight:800}.faGradeTipValue{color:#fff;font-weight:900}.faCasesTable .roGrade.hasHover:hover .faGradeTooltip{display:block}"
        "</style>"
        "<div class='faFilterBar'><details class='faFilterSelect'><summary>FILTER : ");
    kbo_html_append_escaped(buffer, kbo_fa_market_filter_label(g_kbo_hub_fa_market_filter));
    kbo_window_text_appendf(buffer, "</summary><div class='faFilterMenu'>");
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/0", "None", g_kbo_hub_fa_market_filter == 0);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/1", "Official FA", g_kbo_hub_fa_market_filter == 1);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/2", "Compensation", g_kbo_hub_fa_market_filter == 2);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/3", "Domestic", g_kbo_hub_fa_market_filter == 3);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/4", "Foreign", g_kbo_hub_fa_market_filter == 4);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/5", "Grade A", g_kbo_hub_fa_market_filter == 5);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/6", "Grade B", g_kbo_hub_fa_market_filter == 6);
    kbo_webview_append_fa_market_choice_option(buffer, "kbo://fa-market/filter/7", "Rights", g_kbo_hub_fa_market_filter == 7);
    kbo_window_text_appendf(buffer,
        "</div></details><details class='faFilterSelect wide'><summary>POSITION: ");
    kbo_html_append_escaped(buffer, kbo_fa_market_position_filter_label(g_kbo_hub_fa_market_position_filter));
    kbo_window_text_appendf(buffer, "</summary><div class='faFilterMenu'>");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_ALL, "All Players");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_PITCHERS, "Pitchers");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_STARTERS, "Starters");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_RELIEVERS, "Relievers");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_CATCHERS, "Catchers");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_INFIELDERS, "Infielders");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_OUTFIELDERS, "Outfielders");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_1B, "1B");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_2B, "2B");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_3B, "3B");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_SS, "SS");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_LF, "LF");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_CF, "CF");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_RF, "RF");
    kbo_webview_append_fa_market_position_option(buffer, KBO_FA_MARKET_POSITION_DH, "DH");
    kbo_window_text_appendf(buffer,
        "</div></details><div class='faFilterSpacer'></div><div class='faFilterSummary'>Filter: ");
    kbo_html_append_escaped(buffer, kbo_fa_market_filter_label(g_kbo_hub_fa_market_filter));
    kbo_window_text_appendf(buffer, " - POS: ");
    kbo_html_append_escaped(buffer, kbo_fa_market_position_filter_label(g_kbo_hub_fa_market_position_filter));
    kbo_window_text_appendf(buffer, " - %d / %d Players</div></div>", filtered_rows, total_rows);
}
