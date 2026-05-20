#include "../internal/no_minor_patch_internal.h"

int install_kbo_no_minor_contract_offer_ui_patches(HMODULE exe)
{
    const uint8_t expected_offer_ui_minor_item[3] = {0x45, 0x33, 0xC9};
    const uint8_t patch_offer_ui_minor_item[3] = {0xEB, 0x22, 0x90};
    const uint8_t expected_offer_ui_option_item[3] = {0x45, 0x33, 0xC9};
    const uint8_t patch_offer_ui_option_item[3] = {0xEB, 0x28, 0x90};
    const uint8_t expected_offer_ui_selection_value[5] = {0xBA, 0x02, 0x00, 0x00, 0x00};
    const uint8_t patch_offer_ui_selection_value[5] = {0xBA, 0x01, 0x00, 0x00, 0x00};
    const uint8_t expected_offer_ui_selection_branch[2] = {0x75, 0x04};
    const uint8_t patch_offer_ui_selection_branch[2] = {0xEB, 0x04};
    const uint8_t expected_offer_report_minor_item[5] = {0x45, 0x33, 0xC9, 0x4C, 0x8D};
    const uint8_t patch_offer_report_minor_item[5] = {0xE9, 0xD6, 0x3A, 0x00, 0x00};
    const uint8_t expected_offer_report_option_item[5] = {0x45, 0x33, 0xC9, 0x4C, 0x8D};
    const uint8_t patch_offer_report_option_item[5] = {0xE9, 0xBA, 0x00, 0x00, 0x00};
    const uint8_t expected_contract_type_minor_item[5] = {0x45, 0x33, 0xC0, 0x48, 0x8D};
    const uint8_t patch_contract_type_minor_item[5] = {0xE9, 0x1D, 0x00, 0x00, 0x00};
    const uint8_t expected_minor_action_case[9] = {0x83, 0xFF, 0x35, 0x0F, 0x85, 0x33, 0x01, 0x00, 0x00};
    const uint8_t patch_minor_action_case[9] = {0xE9, 0x37, 0x01, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90};
    const uint8_t expected_minor_extension_action_case[9] = {0x83, 0xFF, 0x36, 0x0F, 0x85, 0x43, 0x01, 0x00, 0x00};
    const uint8_t patch_minor_extension_action_case[9] = {0xE9, 0x47, 0x01, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90};
    const uint8_t context_minor_extension_action_case[64] = {
        0x4E, 0x08, 0xE8, 0xE7, 0xFD, 0x7D, 0xFF, 0x41,
        0x0F, 0xB6, 0xC6, 0xE9, 0x95, 0xFB, 0xFF, 0xFF,
        0x83, 0xFF, 0x36, 0x0F, 0x85, 0x43, 0x01, 0x00,
        0x00, 0x4C, 0x8B, 0x1D, 0xDE, 0x74, 0x17, 0x02,
        0x49, 0x8B, 0x83, 0xE0, 0x02, 0x00, 0x00, 0x8B,
        0x50, 0x58, 0x49, 0x8B, 0xCB, 0xE8, 0xBC, 0x2C,
        0x3C, 0xFF, 0x48, 0x8B, 0xF8, 0x48, 0x85, 0xC0,
        0x0F, 0x84, 0x65, 0xFB, 0xFF, 0xFF, 0x41, 0x83
    };
    const uint8_t context_minor_extension_action_case_mask[64] = {
        1,1,1,0,0,0,0,1, 1,1,1,1,0,0,0,0,
        1,1,1,1,1,0,0,0, 0,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,0,0,
        0,0,1,1,1,1,1,1, 1,1,0,0,0,0,1,1
    };
    const uint8_t context_offer_ui_minor_item_17a93df[64] = {
        0xF8, 0x02, 0x74, 0x0C, 0x80, 0x7E, 0x08, 0x00,
        0x75, 0x2A, 0x80, 0x7E, 0x0A, 0x00, 0x75, 0x24,
        0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05, 0xEF, 0xAC,
        0x3E, 0x01, 0x41, 0x8D, 0x51, 0x01, 0xE8, 0x6E,
        0x17, 0xA2, 0xFF, 0x48, 0x8B, 0xD0, 0x45, 0x33,
        0xC9, 0x45, 0x33, 0xC0, 0x49, 0x8B, 0xCC, 0xE8,
        0x5D, 0xAB, 0x3F, 0xFF, 0x41, 0x80, 0xBE, 0xD1,
        0x00, 0x00, 0x00, 0x00, 0x0F, 0x85, 0x80, 0x00
    };
    const uint8_t context_offer_ui_minor_item_17a93df_mask[64] = {
        1,1,1,0,1,1,0,1,
        1,0,1,1,0,1,1,0,
        1,1,1,1,1,1,0,0,
        0,0,1,1,1,1,1,0,
        0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        0,0,0,0,1,1,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_offer_ui_option_item_17a9467[64] = {
        0x8B, 0x50, 0x58, 0x49, 0x8B, 0xCD, 0xE8, 0xAE,
        0x3D, 0x0A, 0xFF, 0x83, 0xF8, 0x02, 0x75, 0x2A,
        0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05, 0x8F, 0x31,
        0x45, 0x01, 0xBB, 0x01, 0x00, 0x00, 0x00, 0x8B,
        0xD3, 0xE8, 0xE3, 0x16, 0xA2, 0xFF, 0x48, 0x8B,
        0xD0, 0x45, 0x33, 0xC9, 0x44, 0x8D, 0x43, 0x01,
        0x49, 0x8B, 0xCC, 0xE8, 0xD1, 0xAA, 0x3F, 0xFF,
        0xEB, 0x05, 0xBB, 0x01, 0x00, 0x00, 0x00, 0x41
    };
    const uint8_t context_offer_ui_option_item_17a9467_mask[64] = {
        1,1,1,1,1,1,1,0,
        0,0,0,1,1,1,1,0,
        1,1,1,1,1,1,0,0,
        0,0,1,1,1,1,1,1,
        1,1,0,0,0,0,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_offer_ui_selection_value_17a94a0[64] = {
        0x05, 0xBB, 0x01, 0x00, 0x00, 0x00, 0x41, 0xB0,
        0x01, 0x49, 0x8B, 0xCC, 0x80, 0x7E, 0x0A, 0x00,
        0xBA, 0x02, 0x00, 0x00, 0x00, 0x75, 0x04, 0x0F,
        0xB6, 0x56, 0x08, 0xE8, 0x10, 0x6B, 0x6C, 0x00,
        0x44, 0x8B, 0xC3, 0x8B, 0xD3, 0x4D, 0x8B, 0xCC,
        0x48, 0x8B, 0xCF, 0xE8, 0x90, 0xCC, 0x6A, 0x00,
        0x41, 0xBC, 0x02, 0x00, 0x00, 0x00, 0xEB, 0x03,
        0x49, 0x8B, 0xDC, 0x0F, 0xB6, 0x46, 0x08, 0x84
    };
    const uint8_t context_offer_ui_selection_value_17a94a0_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,0,1,
        1,1,1,1,1,1,0,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_offer_ui_selection_branch_17a94a5[64] = {
        0x00, 0x41, 0xB0, 0x01, 0x49, 0x8B, 0xCC, 0x80,
        0x7E, 0x0A, 0x00, 0xBA, 0x02, 0x00, 0x00, 0x00,
        0x75, 0x04, 0x0F, 0xB6, 0x56, 0x08, 0xE8, 0x10,
        0x6B, 0x6C, 0x00, 0x44, 0x8B, 0xC3, 0x8B, 0xD3,
        0x4D, 0x8B, 0xCC, 0x48, 0x8B, 0xCF, 0xE8, 0x90,
        0xCC, 0x6A, 0x00, 0x41, 0xBC, 0x02, 0x00, 0x00,
        0x00, 0xEB, 0x03, 0x49, 0x8B, 0xDC, 0x0F, 0xB6,
        0x46, 0x08, 0x84, 0xC0, 0x75, 0x05, 0x38, 0x46
    };
    const uint8_t context_offer_ui_selection_branch_17a94a5_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,0,1,1,1,1,1,1,
        1,0,1,1,1,1,1,0,
        0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,1,0,
        0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,0,1,1
    };
    const uint8_t context_offer_report_minor_item_17da0ec[64] = {
        0x48, 0x89, 0x7D, 0x00, 0x80, 0x7E, 0x08, 0x00,
        0x75, 0x78, 0x80, 0x7E, 0x0A, 0x00, 0x75, 0x72,
        0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05, 0xE2, 0x9F,
        0x3B, 0x01, 0xE8, 0x65, 0x0A, 0x9F, 0xFF, 0x48,
        0x8B, 0xD0, 0x48, 0x8D, 0x4C, 0x24, 0x48, 0xE8,
        0xF8, 0xDE, 0x4D, 0x00, 0xB9, 0x00, 0x0D, 0x00,
        0x00, 0xE8, 0xCA, 0x48, 0xFA, 0x00, 0x48, 0x89,
        0x85, 0x88, 0x00, 0x00, 0x00, 0x45, 0x33, 0xC9
    };
    const uint8_t context_offer_report_minor_item_17da0ec_mask[64] = {
        1,1,1,0,1,1,0,1,
        1,0,1,1,0,1,1,0,
        1,1,1,1,1,1,0,0,
        0,0,1,0,0,0,0,1,
        1,1,1,1,1,1,1,1,
        0,0,0,0,1,1,1,1,
        1,1,0,0,0,0,1,1,
        1,1,1,1,1,1,1,1
    };
    const uint8_t context_offer_report_option_item_17da2c6[64] = {
        0x04, 0x0A, 0x66, 0x41, 0xFF, 0xC0, 0x66, 0x45,
        0x3B, 0x86, 0x08, 0x01, 0x00, 0x00, 0x7C, 0xDA,
        0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05, 0x70, 0x76,
        0x3E, 0x01, 0x41, 0x8D, 0x51, 0x01, 0xE8, 0x87,
        0x08, 0x9F, 0xFF, 0x48, 0x8B, 0xD8, 0x48, 0x85,
        0xC0, 0x74, 0x75, 0x48, 0x8B, 0xCF, 0x48, 0xFF,
        0xC1, 0x40, 0x38, 0x34, 0x08, 0x75, 0xF7, 0x83,
        0xC1, 0x02, 0x8B, 0x7C, 0x24, 0x5C, 0x3B, 0xCF
    };
    const uint8_t context_offer_report_option_item_17da2c6_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,0,
        1,1,1,1,1,1,0,0,
        0,0,1,1,1,1,1,0,
        0,0,0,1,1,1,1,1,
        1,1,0,1,1,1,1,1,
        1,1,1,1,1,1,0,1,
        1,1,1,1,0,1,1,1
    };
    const uint8_t context_contract_type_minor_item_182666a[64] = {
        0x8B, 0xD0, 0x45, 0x33, 0xC9, 0x44, 0x8B, 0xC6,
        0x48, 0x8B, 0xCF, 0xE8, 0xF6, 0xD8, 0x37, 0xFF,
        0x45, 0x33, 0xC0, 0x48, 0x8D, 0x15, 0x64, 0xDA,
        0x36, 0x01, 0x8B, 0xCE, 0xE8, 0xE5, 0x3F, 0x9A,
        0xFF, 0x48, 0x8B, 0xD0, 0x45, 0x33, 0xC9, 0x45,
        0x33, 0xC0, 0x48, 0x8B, 0xCF, 0xE8, 0xD4, 0xD8,
        0x37, 0xFF, 0x41, 0x8B, 0xD4, 0x45, 0x38, 0xA7,
        0xA8, 0x08, 0x00, 0x00, 0x0F, 0x95, 0xC2, 0x41
    };
    const uint8_t context_contract_type_minor_item_182666a_mask[64] = {
        1,1,1,1,1,1,1,1,
        1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,0,0,
        0,0,1,1,1,0,0,0,
        0,1,1,1,1,1,1,1,
        1,1,1,1,1,1,0,0,
        0,0,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1
    };

    int ok = 0;
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract offer UI skip minor item 17A93DF",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_MINOR_ITEM_17A93DF_RVA,
        expected_offer_ui_minor_item,
        patch_offer_ui_minor_item,
        sizeof(expected_offer_ui_minor_item),
        context_offer_ui_minor_item_17a93df,
        context_offer_ui_minor_item_17a93df_mask,
        sizeof(context_offer_ui_minor_item_17a93df),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract offer UI skip option item 17A9467",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_OPTION_ITEM_17A9467_RVA,
        expected_offer_ui_option_item,
        patch_offer_ui_option_item,
        sizeof(expected_offer_ui_option_item),
        context_offer_ui_option_item_17a9467,
        context_offer_ui_option_item_17a9467_mask,
        sizeof(context_offer_ui_option_item_17a9467),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract offer UI force selected major 17A94A0",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_SELECTION_VALUE_17A94A0_RVA,
        expected_offer_ui_selection_value,
        patch_offer_ui_selection_value,
        sizeof(expected_offer_ui_selection_value),
        context_offer_ui_selection_value_17a94a0,
        context_offer_ui_selection_value_17a94a0_mask,
        sizeof(context_offer_ui_selection_value_17a94a0),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract offer UI force selection branch 17A94A5",
        OOTP27_NO_MINOR_CONTRACT_OFFER_UI_SELECTION_BRANCH_17A94A5_RVA,
        expected_offer_ui_selection_branch,
        patch_offer_ui_selection_branch,
        sizeof(expected_offer_ui_selection_branch),
        context_offer_ui_selection_branch_17a94a5,
        context_offer_ui_selection_branch_17a94a5_mask,
        sizeof(context_offer_ui_selection_branch_17a94a5),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract report skip minor item 17DA0EC",
        OOTP27_NO_MINOR_CONTRACT_OFFER_REPORT_MINOR_ITEM_17DA0EC_RVA,
        expected_offer_report_minor_item,
        patch_offer_report_minor_item,
        sizeof(expected_offer_report_minor_item),
        context_offer_report_minor_item_17da0ec,
        context_offer_report_minor_item_17da0ec_mask,
        sizeof(context_offer_report_minor_item_17da0ec),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract report skip option item 17DA2C6",
        OOTP27_NO_MINOR_CONTRACT_OFFER_REPORT_OPTION_ITEM_17DA2C6_RVA,
        expected_offer_report_option_item,
        patch_offer_report_option_item,
        sizeof(expected_offer_report_option_item),
        context_offer_report_option_item_17da2c6,
        context_offer_report_option_item_17da2c6_mask,
        sizeof(context_offer_report_option_item_17da2c6),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract contract type skip minor item 182666A",
        OOTP27_NO_MINOR_CONTRACT_CONTRACT_TYPE_MINOR_ITEM_182666A_RVA,
        expected_contract_type_minor_item,
        patch_contract_type_minor_item,
        sizeof(expected_contract_type_minor_item),
        context_contract_type_minor_item_182666a,
        context_contract_type_minor_item_182666a_mask,
        sizeof(context_contract_type_minor_item_182666a),
        16u);
    ok |= patch_kbo_no_minor_contract_write_site_by_pattern(
        exe,
        "KBO no-minor-contract block minor action case 1072DF6",
        OOTP27_NO_MINOR_CONTRACT_MINOR_ACTION_CASE_1072DF6_RVA,
        expected_minor_action_case,
        patch_minor_action_case,
        sizeof(expected_minor_action_case));
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract block minor extension action case 1072F32",
        OOTP27_NO_MINOR_CONTRACT_MINOR_EXTENSION_ACTION_CASE_1072F32_RVA,
        expected_minor_extension_action_case,
        patch_minor_extension_action_case,
        sizeof(expected_minor_extension_action_case),
        context_minor_extension_action_case,
        context_minor_extension_action_case_mask,
        sizeof(context_minor_extension_action_case),
        16u);
    return ok;
}
