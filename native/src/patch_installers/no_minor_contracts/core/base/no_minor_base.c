#include "../internal/no_minor_patch_internal.h"

int install_kbo_no_minor_contract_base_patches(HMODULE exe)
{
    const uint8_t expected_fa_offer_demand_floor[14] = {
        0x66, 0x89, 0x75, 0x98,                         /* mov [rbp-0x68],si */
        0x49, 0x89, 0xB6, 0xD8, 0x00, 0x00, 0x00,       /* mov [r14+0xd8],rsi */
        0x8D, 0x4E, 0x04                                /* lea ecx,[rsi+4] */
    };
    const uint8_t expected_fa_offer_alt_demand_floor[20] = {
        0x45, 0x38, 0xBE, 0xC3, 0x0D, 0x00, 0x00,       /* cmp r15b,[r14+0xdc3] */
        0x74, 0x0B,                                     /* je +0xb */
        0x41, 0xC7, 0x85, 0x1C, 0x03, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00                          /* mov [r13+0x31c],8 */
    };

    int ok = 0;
    ok |= install_kbo_no_minor_contract_offer_callback_probe_patch(exe);
    ok |= install_kbo_no_minor_contract_contract_callback_probe_patch(exe);
    ok |= install_kbo_no_minor_contract_player_action_eligibility_patch(exe);
    ok |= install_kbo_no_minor_contract_offer_major_flag_patches(exe);
    ok |= install_kbo_no_minor_contract_offer_player_demand_floor_patch(
        exe,
        "KBO no-minor demand floor FA offer build 17A79BB",
        OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17A79BB_RVA,
        expected_fa_offer_demand_floor,
        sizeof(expected_fa_offer_demand_floor),
        0);
    ok |= install_kbo_no_minor_contract_offer_player_demand_floor_patch(
        exe,
        "KBO no-minor demand floor FA offer alt build 17B50B4",
        OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17B50B4_RVA,
        expected_fa_offer_alt_demand_floor,
        sizeof(expected_fa_offer_alt_demand_floor),
        1);
    ok |= install_kbo_no_minor_contract_demand_write_floor_aab739_patch(exe);
    ok |= install_kbo_no_minor_contract_demand_write_floor_1077952_patch(exe);
    kbo_log_runtime_line("KBO no-minor demand write-floor patches enabled: demand salary only, no player contract-level writes");
    kbo_log_runtime_line("KBO foreign FA demand baseline prepare patch retired from no-minor safe profile");
    ok |= install_kbo_no_minor_contract_submit_salary_floor_patch(exe);
    return ok;
}
