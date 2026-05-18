#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../paths/ui_image_sources.h"
#include "ui_nation_helpers.h"
#include "ui_nation_table.h"

const char* kbo_hub_nation_label_for_id(uint32_t nation_id)
{
    return kbo_nation_table_label(nation_id);
}

const char* kbo_hub_nation_abbrev_for_id(uint32_t nation_id)
{
    return kbo_nation_table_abbrev(nation_id);
}

const char* kbo_hub_nation_flag_file_for_id(uint32_t nation_id)
{
    return kbo_nation_table_flag_file(nation_id);
}

static void kbo_webview_append_nation_flag_image(
    KboWindowTextBuffer* buffer,
    uint32_t nation_id,
    KboHubNationFlagAssetPathFn flag_asset_path)
{
    if (buffer == NULL || flag_asset_path == NULL) {
        return;
    }

    char flag_path[MAX_PATH] = {0};
    flag_asset_path(kbo_hub_nation_flag_file_for_id(nation_id), flag_path, sizeof(flag_path));
    if (flag_path[0] == '\0' || GetFileAttributesA(flag_path) == INVALID_FILE_ATTRIBUTES) {
        flag_asset_path("unknown.png", flag_path, sizeof(flag_path));
    }

    kbo_window_text_appendf(buffer, "<img class='roNatFlag' alt='");
    kbo_html_append_escaped(buffer, kbo_hub_nation_abbrev_for_id(nation_id));
    kbo_window_text_appendf(buffer, "' title='");
    kbo_html_append_escaped(buffer, kbo_hub_nation_label_for_id(nation_id));
    kbo_window_text_appendf(buffer, " nation#%u' src='", nation_id);
    kbo_webview_append_image_src(buffer, flag_path);
    kbo_window_text_appendf(buffer, "'>");
}

void kbo_webview_append_roster_nation_cell(
    KboWindowTextBuffer* buffer,
    uint32_t nation_id,
    KboHubNationFlagAssetPathFn flag_asset_path)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<td class='roNat' title='");
    kbo_html_append_escaped(buffer, kbo_hub_nation_label_for_id(nation_id));
    kbo_window_text_appendf(buffer, " nation#%u'><span class='roNatWrap'>", nation_id);
    kbo_webview_append_nation_flag_image(buffer, nation_id, flag_asset_path);
    kbo_window_text_appendf(buffer, "<span class='roNatText'>");
    kbo_html_append_escaped(buffer, kbo_hub_nation_abbrev_for_id(nation_id));
    kbo_window_text_appendf(buffer, "</span></span></td>");
}
