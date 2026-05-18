#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../paths/ui_image_sources.h"
#include "ui_nation_helpers.h"

const char* kbo_hub_nation_label_for_id(uint32_t nation_id)
{
    switch (nation_id) {
    case OOTP27_KBO_KOREA_NATION_ID: return "Korea";
    case 12u:                        return "Australia";
    case 36u:                        return "Canada";
    case 43u:                        return "Taiwan";
    case 49u:                        return "Cuba";
    case 56u:                        return "Dominican Republic";
    case 98u:                        return "Japan";
    case 124u:                       return "Mexico";
    case 206u:                       return "United States";
    case 210u:                       return "Venezuela";
    default:                         return "Unknown nation";
    }
}

const char* kbo_hub_nation_abbrev_for_id(uint32_t nation_id)
{
    switch (nation_id) {
    case OOTP27_KBO_KOREA_NATION_ID: return "KOR";
    case 12u:                        return "AUS";
    case 36u:                        return "CAN";
    case 43u:                        return "TPE";
    case 49u:                        return "CUB";
    case 56u:                        return "DOM";
    case 98u:                        return "JPN";
    case 124u:                       return "MEX";
    case 206u:                       return "USA";
    case 210u:                       return "VEN";
    default:                         return "---";
    }
}

const char* kbo_hub_nation_flag_file_for_id(uint32_t nation_id)
{
    switch (nation_id) {
    case OOTP27_KBO_KOREA_NATION_ID: return "kor.png";
    case 12u:                        return "aus.png";
    case 36u:                        return "can.png";
    case 43u:                        return "tpe.png";
    case 49u:                        return "cub.png";
    case 56u:                        return "dom.png";
    case 98u:                        return "jpn.png";
    case 124u:                       return "mex.png";
    case 206u:                       return "usa.png";
    case 210u:                       return "ven.png";
    default:                         return "unknown.png";
    }
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
