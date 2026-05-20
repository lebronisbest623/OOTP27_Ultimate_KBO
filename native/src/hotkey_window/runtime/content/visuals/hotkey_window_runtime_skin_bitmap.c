#include "../hotkey_window_runtime_content.h"

#include <stdio.h>
#include <string.h>
void kbo_hub_delete_bitmap(HBITMAP* bitmap)
{
    if (bitmap != NULL && *bitmap != NULL) {
        DeleteObject(*bitmap);
        *bitmap = NULL;
    }
}

HBITMAP kbo_hub_load_png_hbitmap_wic(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    WCHAR wide_path[MAX_PATH];
    if (!kbo_utf8_to_wide(path, wide_path, (int)(sizeof(wide_path) / sizeof(wide_path[0])))) {
        return NULL;
    }

    IWICImagingFactory* factory = NULL;
    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;
    HBITMAP bitmap = NULL;

    HRESULT hr = CoCreateInstance(
        &CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory,
        (void**)&factory);
    if (FAILED(hr) || factory == NULL) {
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateDecoderFromFilename(
        factory, wide_path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || decoder == NULL) {
        goto cleanup;
    }

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr) || frame == NULL) {
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (FAILED(hr) || converter == NULL) {
        goto cleanup;
    }

    hr = IWICFormatConverter_Initialize(
        converter, (IWICBitmapSource*)frame,
        &GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        goto cleanup;
    }

    UINT width = 0;
    UINT height = 0;
    hr = IWICBitmapSource_GetSize((IWICBitmapSource*)converter, &width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        goto cleanup;
    }

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = (LONG)width;
    bmi.bmiHeader.biHeight      = -(LONG)height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pixels = NULL;
    bitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (bitmap == NULL || pixels == NULL) {
        bitmap = NULL;
        goto cleanup;
    }

    UINT stride = width * 4u;
    UINT buffer_size = stride * height;
    hr = IWICBitmapSource_CopyPixels((IWICBitmapSource*)converter, NULL, stride, buffer_size, (BYTE*)pixels);
    if (FAILED(hr)) {
        DeleteObject(bitmap);
        bitmap = NULL;
    }

cleanup:
    if (converter != NULL) { IWICFormatConverter_Release(converter); }
    if (frame     != NULL) { IWICBitmapFrameDecode_Release(frame);   }
    if (decoder   != NULL) { IWICBitmapDecoder_Release(decoder);     }
    if (factory   != NULL) { IWICImagingFactory_Release(factory);    }
    return bitmap;
}

void kbo_hub_load_skin_assets(void)
{
    if (InterlockedCompareExchange(&g_kbo_hub_skin_assets_loaded, 1, 0) != 0) {
        return;
    }

    char path[MAX_PATH];

    kbo_hub_local_asset_path("github-mark.png", path, sizeof(path));
    g_kbo_hub_asset_github = kbo_hub_load_png_hbitmap_wic(path);
    kbo_log_runtimef("KBO F2 hub github asset path=%s loaded=%d", path, g_kbo_hub_asset_github != NULL);

    kbo_hub_skin_image_path("menu_arrow_right.png", path, sizeof(path));
    g_kbo_hub_asset_menu_arrow = kbo_hub_load_png_hbitmap_wic(path);

    kbo_hub_skin_scrollbar_image_path("sb_bar_top.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_top = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_bar_mid.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_mid = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_bar_bottom.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_bottom = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_less_up.png", path, sizeof(path));
    g_kbo_hub_asset_sb_less = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_more_up.png", path, sizeof(path));
    g_kbo_hub_asset_sb_more = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_top.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_top = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_mid.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_mid = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_bottom.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_bottom = kbo_hub_load_png_hbitmap_wic(path);
}

void kbo_hub_delete_skin_assets(void)
{
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_github);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_menu_arrow);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_top);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_mid);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_bottom);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_less);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_more);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_top);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_mid);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_bottom);
    g_kbo_hub_skin_assets_loaded   = 0;
}

