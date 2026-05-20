#ifndef KBO_HOTKEY_WINDOW_UI_MOD_INFO_VIEWS_INTERNAL_H
#define KBO_HOTKEY_WINDOW_UI_MOD_INFO_VIEWS_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../../../../build_verify/build_verify.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../../../core/product/ootp_product.h"
#include "../../../support/assets/paths/ui_asset_paths.h"
#include "../../../support/assets/paths/ui_image_sources.h"
#include "../../../support/text/language/ui_language.h"
#include "ui_mod_info_views.h"
#include "../../../support/text/buffer/ui_text_buffer.h"

void kbo_webview_append_mod_tester_credits(KboWindowTextBuffer* buffer);

#endif
