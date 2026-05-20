#include "../internal/no_minor_patch_internal.h"

int install_kbo_no_minor_contract_offer_major_flag_patches(HMODULE exe)
{
    const uint8_t expected_offer_major_flag_copy[8] = {0x88, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x84, 0xC0};
    const uint8_t patch_offer_major_flag_copy[8] = {0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x01, 0x90};
    const uint8_t expected_offer_major_flag_reset[7] = {0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x00};
    const uint8_t patch_offer_major_flag_reset[7] = {0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x01};
    const uint8_t context_offer_major_flag_reset[64] = {
        0x00, 0x00, 0x8B, 0x45, 0xC0, 0x89, 0x87, 0xC8,
        0x00, 0x00, 0x00, 0xE9, 0xAF, 0x06, 0x00, 0x00,
        0xC6, 0x87, 0xCC, 0x00, 0x00, 0x00, 0x00, 0xE9,
        0xA3, 0x06, 0x00, 0x00, 0x49, 0x83, 0xF8, 0x08,
        0x75, 0x61, 0x48, 0x8B, 0x05, 0x39, 0xA0, 0xE3,
        0x01, 0x48, 0x8B, 0x88, 0xD8, 0x02, 0x00, 0x00,
        0x48, 0x8B, 0x80, 0xE0, 0x02, 0x00, 0x00, 0x8B,
        0x50, 0x48, 0xE8, 0xC3, 0x37, 0x7D, 0xFF, 0x48
    };
    const uint8_t context_offer_major_flag_reset_mask[64] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1, 0,0,0,0,1,1,1,1,
        1,0,1,1,1,0,0,0, 0,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1, 1,1,1,0,0,0,0,1
    };

    int ok = 0;
    ok |= patch_kbo_no_minor_contract_write_site_by_pattern(
        exe,
        "KBO no-minor-contract offer major flag copy 13B0235",
        OOTP27_NO_MINOR_CONTRACT_OFFER_MAJOR_FLAG_COPY_13B0235_RVA,
        expected_offer_major_flag_copy,
        patch_offer_major_flag_copy,
        sizeof(expected_offer_major_flag_copy));
    ok |= patch_kbo_no_minor_contract_write_site_by_masked_context(
        exe,
        "KBO no-minor-contract offer major flag reset 13B03CE",
        OOTP27_NO_MINOR_CONTRACT_OFFER_MAJOR_FLAG_RESET_13B03CE_RVA,
        expected_offer_major_flag_reset,
        patch_offer_major_flag_reset,
        sizeof(expected_offer_major_flag_reset),
        context_offer_major_flag_reset,
        context_offer_major_flag_reset_mask,
        sizeof(context_offer_major_flag_reset),
        16u);
    return ok;
}
