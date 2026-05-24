#ifndef KBOFIX_SRC_PATCH_INSTALLERS_FOREIGN_AI_FA_STATUS_INTERNAL_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_FOREIGN_AI_FA_STATUS_INTERNAL_H_

#include "patch_installers_foreign_ai_fa_status.h"

#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../hook_stubs/foreign/ai_status/hook_stubs_foreign_ai_status.h"
#include "../../../hook_stubs/military/hook_stubs_military.h"
#include "../../../patch_helpers/patch_helpers.h"

int kbo_install_foreign_ai_offer_build_probe_patch(HMODULE exe);
int kbo_install_foreign_ai_offer_terms_build_probe_patch(HMODULE exe);
int kbo_install_foreign_ai_offer_final_gate_probe_patch(HMODULE exe);

#endif
