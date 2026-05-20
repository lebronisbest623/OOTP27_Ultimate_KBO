#include "player_hover_manager_probe_internal.h"
#include "../../../../core/product/ootp_product.h"

static void kbo_write_tooltip_capture_debug_file(uint32_t player_id, const char* payload)
{
    if (payload == NULL) {
        return;
    }

    char path[MAX_PATH] = {0};
    const char* local_appdata = getenv("LOCALAPPDATA");
    if (local_appdata == NULL || local_appdata[0] == '\0') {
        return;
    }

    snprintf(path, sizeof(path), "%s\\" KBO_PRODUCT_LOCAL_DATA_DIR "\\player_tooltip_capture_last.txt", local_appdata);
    FILE* fp = fopen(path, "wb");
    if (fp == NULL) {
        return;
    }

    fprintf(fp, "player_id: %u\r\n\r\n", player_id);
    fputs(payload, fp);
    fclose(fp);
}

int kbo_capture_ootp_player_tooltip_payload(uint32_t player_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    if (!kbo_lock_try_enter(&g_kbo_player_tooltip_capture_lock)) {
        kbo_log_runtimef("KBO player tooltip capture skipped reason=capture_busy player=%u", player_id);
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_lock_leave(&g_kbo_player_tooltip_capture_lock);
        return 0;
    }

    OotpOperatorNewFn ootp_new =
        (OotpOperatorNewFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_UI_OPERATOR_NEW_RVA);
    OotpPlayerTooltipFactoryFn factory =
        (OotpPlayerTooltipFactoryFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_FACTORY_RVA);
    OotpPlayerTooltipRenderFn render =
        (OotpPlayerTooltipRenderFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_RENDER_RVA);
    void** current_tooltip =
        (void**)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_CURRENT_GLOBAL_RVA);
    if (ootp_new == NULL || factory == NULL || render == NULL || current_tooltip == NULL) {
        kbo_log_runtimef(
            "KBO player tooltip capture skipped reason=resolve_failed player=%u new=%p factory=%p render=%p current=%p",
            player_id,
            (void*)ootp_new,
            (void*)factory,
            (void*)render,
            (void*)current_tooltip);
        kbo_lock_leave(&g_kbo_player_tooltip_capture_lock);
        return 0;
    }

    void* raw = ootp_new(KBO_TOOLTIP_OBJECT_BYTES);
    if (raw == NULL) {
        kbo_lock_leave(&g_kbo_player_tooltip_capture_lock);
        return 0;
    }

    kbo_tooltip_text_capture_reset();
    LONG64 format_calls_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_call_count, 0, 0);
    LONG64 format_active_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_active_call_count, 0, 0);
    LONG64 format_numeric_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_numeric_call_count, 0, 0);
    LONG64 rating_common_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_common_call_count, 0, 0);
    LONG64 rating_common_active_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_common_active_call_count, 0, 0);
    LONG64 rating_panel_ctor_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_panel_ctor_call_count, 0, 0);
    LONG64 rating_panel_ctor_active_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_panel_ctor_active_call_count, 0, 0);
    InterlockedExchange(&g_kbo_player_tooltip_text_capture_active, 1);
    void* tooltip = factory(raw, player_id, 0u, 0u, NULL, 0u);
    if (tooltip == NULL) {
        InterlockedExchange(&g_kbo_player_tooltip_text_capture_active, 0);
        kbo_log_runtimef("KBO player tooltip capture skipped reason=factory_failed player=%u raw=%p", player_id, raw);
        kbo_lock_leave(&g_kbo_player_tooltip_capture_lock);
        return 0;
    }

    *current_tooltip = tooltip;
    render(tooltip, 8u);
    InterlockedExchange(&g_kbo_player_tooltip_text_capture_active, 0);
    LONG64 format_calls_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_call_count, 0, 0);
    LONG64 format_active_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_active_call_count, 0, 0);
    LONG64 format_numeric_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_numeric_call_count, 0, 0);
    LONG64 rating_common_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_common_call_count, 0, 0);
    LONG64 rating_common_active_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_common_active_call_count, 0, 0);
    LONG64 rating_panel_ctor_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_panel_ctor_call_count, 0, 0);
    LONG64 rating_panel_ctor_active_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_panel_ctor_active_call_count, 0, 0);

    int builder_overall = 0;
    int builder_potential = 0;
    int memory_overall = 0;
    int memory_potential = 0;
    int scan_overall = 0;
    int scan_potential = 0;
    int has_builder_overall_potential =
        kbo_tooltip_extract_overall_potential_from_builder_capture(&builder_overall, &builder_potential);
    int has_format_overall_potential =
        kbo_tooltip_extract_overall_potential_from_format_capture(&memory_overall, &memory_potential);
    int has_scan_overall_potential =
        kbo_tooltip_extract_overall_potential(tooltip, &scan_overall, &scan_potential);
    int display_overall = has_builder_overall_potential ? builder_overall : memory_overall;
    int display_potential = has_builder_overall_potential ? builder_potential : memory_potential;
    int has_display_overall_potential = has_builder_overall_potential || has_format_overall_potential;
    const char* display_rating_source =
        has_builder_overall_potential ? "builder" : (has_format_overall_potential ? "format" : "none");

    size_t pos = 0u;
    kbo_tooltip_appendf(out, out_size, &pos, "OOTP tooltip capture\n");
    kbo_tooltip_appendf(out, out_size, &pos, "player_id: %u\n", player_id);
    kbo_tooltip_appendf(out, out_size, &pos, "tooltip_ptr: %p\n", tooltip);
    kbo_tooltip_appendf(out, out_size, &pos, "\nrender text append capture\n%s", g_kbo_player_tooltip_text_capture);
    if (has_display_overall_potential) {
        kbo_tooltip_appendf(
            out,
            out_size,
            &pos,
            "\nmemory display ratings\nsource %s\nOVR %d\nPOT %d\n",
            display_rating_source,
            display_overall,
            display_potential);
    }

    char debug_payload[24000] = {0};
    size_t debug_pos = 0u;
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "%s", out);
    kbo_tooltip_appendf(
        debug_payload,
        sizeof(debug_payload),
        &debug_pos,
        "\nformat probe counters\ncalls_delta=%lld\nactive_delta=%lld\nnumeric_delta=%lld\n",
        (long long)(format_calls_after - format_calls_before),
        (long long)(format_active_after - format_active_before),
        (long long)(format_numeric_after - format_numeric_before));
    kbo_tooltip_appendf(
        debug_payload,
        sizeof(debug_payload),
        &debug_pos,
        "\nrating builder probe counters\ncommon_calls_delta=%lld\ncommon_active_delta=%lld\npanel_ctor_calls_delta=%lld\npanel_ctor_active_delta=%lld\n",
        (long long)(rating_common_after - rating_common_before),
        (long long)(rating_common_active_after - rating_common_active_before),
        (long long)(rating_panel_ctor_after - rating_panel_ctor_before),
        (long long)(rating_panel_ctor_active_after - rating_panel_ctor_active_before));
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\nrating builder capture\n%s", g_kbo_player_tooltip_builder_capture);
    if (has_scan_overall_potential) {
        kbo_tooltip_appendf(
            debug_payload,
            sizeof(debug_payload),
            &debug_pos,
            "\nmemory scan candidate ratings\nOVR %d\nPOT %d\n",
            scan_overall,
            scan_potential);
    }
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\ninline strings\n");
    kbo_tooltip_scan_inline_strings("obj", (uint8_t*)tooltip, KBO_TOOLTIP_OBJECT_BYTES, debug_payload, sizeof(debug_payload), &debug_pos);
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\npointer strings\n");
    kbo_tooltip_scan_pointer_strings(tooltip, debug_payload, sizeof(debug_payload), &debug_pos);
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\nrating formats\n%s", g_kbo_player_tooltip_rating_capture);
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\nrating panels\n");
    kbo_tooltip_scan_rating_panels(tooltip, debug_payload, sizeof(debug_payload), &debug_pos);
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\nrating child values\n");
    kbo_tooltip_scan_rating_child_values(tooltip, debug_payload, sizeof(debug_payload), &debug_pos);
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\nnested pointer strings\n");
    kbo_tooltip_scan_nested_pointer_strings(tooltip, debug_payload, sizeof(debug_payload), &debug_pos);

    kbo_write_tooltip_capture_debug_file(player_id, debug_payload);
    kbo_log_runtimef(
        "KBO player tooltip captured player=%u tooltip=%p ui_bytes=%u debug_bytes=%u rating_source=%s ovr=%d pot=%d format_calls=%lld active=%lld numeric=%lld rating_common=%lld rating_common_active=%lld panel_ctor=%lld panel_ctor_active=%lld scan_ovr=%d scan_pot=%d",
        player_id,
        tooltip,
        (unsigned int)pos,
        (unsigned int)debug_pos,
        display_rating_source,
        has_display_overall_potential ? display_overall : 0,
        has_display_overall_potential ? display_potential : 0,
        (long long)(format_calls_after - format_calls_before),
        (long long)(format_active_after - format_active_before),
        (long long)(format_numeric_after - format_numeric_before),
        (long long)(rating_common_after - rating_common_before),
        (long long)(rating_common_active_after - rating_common_active_before),
        (long long)(rating_panel_ctor_after - rating_panel_ctor_before),
        (long long)(rating_panel_ctor_active_after - rating_panel_ctor_active_before),
        has_scan_overall_potential ? scan_overall : 0,
        has_scan_overall_potential ? scan_potential : 0);
    kbo_lock_leave(&g_kbo_player_tooltip_capture_lock);
    return pos > 0u;
}
