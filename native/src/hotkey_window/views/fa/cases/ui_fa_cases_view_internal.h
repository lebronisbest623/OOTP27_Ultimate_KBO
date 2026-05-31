#ifndef KBOFIX_SRC_HOTKEY_WINDOW_VIEWS_FA_CASES_UI_FA_CASES_VIEW_INTERNAL_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_VIEWS_FA_CASES_UI_FA_CASES_VIEW_INTERNAL_H_

#include "../ui_fa_views_internal.h"

#define KBO_FA_MARKET_UI_MAX_ROWS KBO_FA_MARKET_CLASSIFICATION_MAX
#define KBO_FA_MARKET_UI_RENDER_CHUNK 500

extern int g_kbo_hub_fa_market_filter;
extern int g_kbo_hub_fa_market_position_filter;

int kbo_fa_market_row_matches_filter(const KboFaMarketClassification* row, int filter);
int kbo_fa_market_row_matches_position_filter(
    const KboFaMarketClassification* row,
    int position_filter);
void kbo_webview_append_fa_market_filter_bar(
    KboWindowTextBuffer* buffer,
    int filtered_rows,
    int total_rows);
void kbo_webview_fa_market_add_nation_id(uint32_t* nation_ids, int* nation_count, uint32_t nation_id);
void kbo_webview_append_fa_market_flag_src(KboWindowTextBuffer* buffer, uint32_t nation_id);
void kbo_webview_append_fa_market_flag_map(KboWindowTextBuffer* buffer, const uint32_t* nation_ids, int nation_count);

#endif
