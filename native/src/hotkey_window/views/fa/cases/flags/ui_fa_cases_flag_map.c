#include "../ui_fa_cases_view_internal.h"
#include "../../../../support/assets/paths/ui_image_sources.h"
#include "../../../../support/text/js/ui_js_string.h"

void kbo_webview_fa_market_add_nation_id(
    uint32_t* nation_ids,
    int* nation_count,
    uint32_t nation_id)
{
    if (nation_ids == NULL || nation_count == NULL || *nation_count >= 64) {
        return;
    }
    for (int i = 0; i < *nation_count; i++) {
        if (nation_ids[i] == nation_id) {
            return;
        }
    }
    nation_ids[*nation_count] = nation_id;
    *nation_count += 1;
}

void kbo_webview_append_fa_market_flag_src(
    KboWindowTextBuffer* buffer,
    uint32_t nation_id)
{
    if (buffer == NULL) {
        return;
    }

    char flag_path[MAX_PATH] = {0};
    char src[65536] = {0};
    KboWindowTextBuffer src_buffer;
    kbo_hub_nation_flag_asset_path(kbo_hub_nation_flag_file_for_id(nation_id), flag_path, sizeof(flag_path));
    if (flag_path[0] == '\0' || GetFileAttributesA(flag_path) == INVALID_FILE_ATTRIBUTES) {
        kbo_hub_nation_flag_asset_path("unknown.png", flag_path, sizeof(flag_path));
    }
    src_buffer.data = src;
    src_buffer.capacity = sizeof(src);
    src_buffer.length = 0;
    kbo_webview_append_image_src(&src_buffer, flag_path);
    if (src[0] == '\0') {
        kbo_webview_copy_file_url(flag_path, src, sizeof(src));
    }
    kbo_webview_append_js_string(buffer, src);
}

void kbo_webview_append_fa_market_flag_map(
    KboWindowTextBuffer* buffer,
    const uint32_t* nation_ids,
    int nation_count)
{
    if (buffer == NULL) {
        return;
    }

    kbo_window_text_appendf(buffer, "var flags={");
    for (int i = 0; i < nation_count; i++) {
        if (i > 0) {
            kbo_window_text_appendf(buffer, ",");
        }
        kbo_window_text_appendf(buffer, "'%u':", nation_ids[i]);
        kbo_webview_append_fa_market_flag_src(buffer, nation_ids[i]);
    }
    if (nation_count > 0) {
        kbo_window_text_appendf(buffer, ",");
    }
    kbo_window_text_appendf(buffer, "unknown:");
    kbo_webview_append_fa_market_flag_src(buffer, 0u);
    kbo_window_text_appendf(buffer, "};");
}
