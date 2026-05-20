#ifndef NATIVE_SRC_PATCH_INSTALLERS_NO_MINOR_CONTRACTS_CORE_NO_MINOR_PATCH_INTERNAL_H
#define NATIVE_SRC_PATCH_INSTALLERS_NO_MINOR_CONTRACTS_CORE_NO_MINOR_PATCH_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/signability/state/submit_offer_probe_state.h"
#include "../../callbacks/contract/no_minor_contract_callback_patch.h"
#include "../../demand_floors/write/no_minor_demand_write_floor_patches.h"
#include "../api/no_minor_patch.h"
#include "../../demand_floors/baseline/no_minor_foreign_fa_baseline_patch.h"
#include "../../callbacks/offer/no_minor_offer_callback_patch.h"
#include "../../demand_floors/offer/no_minor_offer_demand_floor_patch.h"
#include "../../common/no_minor_patch_helpers.h"
#include "../../callbacks/player_action/no_minor_player_action_patch.h"
#include "../../demand_floors/submit/no_minor_submit_salary_floor_patch.h"

int install_kbo_no_minor_contract_base_patches(HMODULE exe);
int install_kbo_no_minor_contract_offer_major_flag_patches(HMODULE exe);
int install_kbo_no_minor_contract_offer_ui_patches(HMODULE exe);

#endif
