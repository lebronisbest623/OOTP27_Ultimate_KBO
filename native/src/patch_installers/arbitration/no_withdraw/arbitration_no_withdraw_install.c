#include "arbitration_no_withdraw_patch_module.h"
#include "../../../build_verify/build_verify.h"

static uint8_t* kbo_arbitration_related_target(
    HMODULE exe,
    uint8_t* target,
    uint32_t from_canonical_rva,
    uint32_t to_canonical_rva,
    const char* label)
{
    uint8_t* related = (uint8_t*)kbo_resolve_build_specific_rva_related_ptr(
        exe,
        target,
        from_canonical_rva,
        to_canonical_rva);
    if (related == NULL) {
        kbo_log_runtimef("%s skipped: related target unresolved", label);
    }
    return related;
}

#define KBO_ARBITRATION_RELATED_TARGET(target, from_rva, to_rva, label) \
    kbo_arbitration_related_target(exe, (target), (from_rva), (to_rva), (label))

int install_kbo_salary_arbitration_no_withdraw_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO salary arbitration no-withdraw patch");
        return 0;
    }

    const uint8_t expected_ui_hide_branch[6] = {
        0x0F, 0x85, 0x8D, 0x01, 0x00, 0x00              /* jne 0x1413604c5 */
    };
    const uint8_t patch_ui_hide_branch[6] = {
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90              /* always run OOTP's existing hide/disable path */
    };
    const uint8_t context_ui_hide_branch[48] = {
        0x49, 0x8B, 0xCD, 0xE8, 0x26, 0x5E, 0xAF, 0x00,
        0x41, 0x83, 0xBF, 0x7C, 0x08, 0x00, 0x00, 0x00,
        0x0F, 0x85, 0x8D, 0x01, 0x00, 0x00, 0x80, 0xBB,
        0x35, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x84, 0x80,
        0x01, 0x00, 0x00, 0x80, 0x7B, 0x15, 0x00, 0x74,
        0x0C, 0x48, 0x8B, 0x03, 0x48, 0x8B, 0xCB, 0xFF
    };
    const uint8_t context_ui_hide_branch_mask[48] = {
        1,1,1,1,0,0,0,0, 1,1,1,1,1,1,1,1,
        1,1,0,0,0,0, 1,1,1,1,1,1,1,1,1,0,
        0,0,0,1, 1,1,1,1,1,1,1,1,1,1,1,1
    };

    const uint8_t expected_action_gate[13] = {
        0x49, 0x81, 0xFD, 0x40, 0x42, 0x0F, 0x00,       /* cmp r13, 0xf4240 */
        0x0F, 0x8E, 0xB7, 0x02, 0x00, 0x00              /* jle 0x141360e1c */
    };
    const uint8_t patch_action_gate[13] = {
        0xE9, 0xBF, 0x02, 0x00, 0x00,                   /* jmp 0x141360e1c */
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };

    const uint8_t expected_final_zero_tender_gate[14] = {
        0x41, 0x83, 0xBD, 0x7C, 0x08, 0x00, 0x00, 0x00, /* cmp dword ptr [r13+0x87c], 0 */
        0x0F, 0x8F, 0xD8, 0x00, 0x00, 0x00              /* jg 0x140681c06 */
    };
    const uint8_t context_final_zero_tender_gate[48] = {
        0x49, 0x8B, 0xCE, 0xE8, 0x08, 0x32, 0x3B, 0x00,
        0x84, 0xC0, 0x0F, 0x85, 0xE6, 0x00, 0x00, 0x00,
        0x41, 0x83, 0xBD, 0x7C, 0x08, 0x00, 0x00, 0x00,
        0x0F, 0x8F, 0xD8, 0x00, 0x00, 0x00, 0x49, 0x8B,
        0xD5, 0x49, 0x8B, 0xCC, 0xE8, 0x67, 0x1C, 0xFF,
        0xFF, 0x84, 0xC0, 0x75, 0x20, 0x41, 0x88, 0x85
    };
    const uint8_t context_final_zero_tender_gate_mask[48] = {
        1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1, 1,1,0,0,0,0,1,1,
        1,1,1,1,1,0,0,0, 0,1,1,1,1,1,1,1
    };

    const uint8_t expected_ai_offer_write_681012[14] = {
        0x41, 0x89, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov [r13+0x87c], eax */
        0x66, 0x41, 0x0F, 0x2F, 0xF3,                   /* comisd xmm6, xmm11 */
        0x76, 0x2A                                      /* jbe 0x14068204a */
    };
    const uint8_t context_ai_offer_write_681012[48] = {
        0x78, 0x3B, 0xC8, 0x7D, 0x0B, 0x89, 0x85, 0x28,
        0x01, 0x00, 0x00, 0x8B, 0xC1, 0x89, 0x4D, 0x78,
        0x41, 0x89, 0x85, 0x7C, 0x08, 0x00, 0x00, 0x66,
        0x41, 0x0F, 0x2F, 0xF3, 0x76, 0x2A, 0x49, 0x8D,
        0x9D, 0xA0, 0x08, 0x00, 0x00, 0x48, 0x8D, 0x95,
        0xF0, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xCB, 0xE8
    };

    const uint8_t expected_zero_offer_check_682089[18] = {
        0x41, 0x8B, 0x85, 0x7C, 0x08, 0x00, 0x00,       /* mov eax, [r13+0x87c] */
        0x85, 0xC0,                                     /* test eax, eax */
        0x74, 0x23,                                     /* je 0x1406820b7 */
        0x41, 0x8B, 0x8D, 0x80, 0x08, 0x00, 0x00        /* mov ecx, [r13+0x880] */
    };
    const uint8_t context_zero_offer_check_682089[48] = {
        0x49, 0x8D, 0xBD, 0x14, 0x09, 0x00, 0x00, 0x48,
        0x8B, 0x75, 0xB8, 0xE9, 0x76, 0x01, 0x00, 0x00,
        0x41, 0x8B, 0x85, 0x7C, 0x08, 0x00, 0x00, 0x85,
        0xC0, 0x74, 0x23, 0x41, 0x8B, 0x8D, 0x80, 0x08,
        0x00, 0x00, 0x3B, 0xC1, 0x0F, 0x8C, 0xCD, 0x00,
        0x00, 0x00, 0x48, 0x8D, 0x95, 0xA0, 0x01, 0x00
    };

    const uint8_t expected_high_limit_skip_rsi_branch[6] = {
        0x0F, 0x8F, 0xDE, 0x02, 0x00, 0x00              /* jg 0x1406820b3 */
    };
    const uint8_t patch_high_limit_skip_rsi_branch[6] = {
        0x0F, 0x8F, 0xDA, 0x02, 0x00, 0x00              /* jg 0x1406820af */
    };
    const uint8_t context_high_limit_skip_rsi_branch[48] = {
        0xEB, 0x02, 0x8B, 0xCF, 0x41, 0x8B, 0x84, 0x24,
        0x98, 0x02, 0x00, 0x00, 0x03, 0xC0, 0x3B, 0xC8,
        0x0F, 0x8F, 0xDE, 0x02, 0x00, 0x00, 0x84, 0xDB,
        0x74, 0x47, 0x33, 0xD2, 0x48, 0x8D, 0x4D, 0x40,
        0xE8, 0x0C, 0x34, 0x22, 0x00, 0x66, 0x0F, 0x6E,
        0xC8, 0xF3, 0x0F, 0xE6, 0xC9, 0x66, 0x0F, 0x6E
    };
    const uint8_t context_high_limit_skip_rsi_branch_mask[48] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,0,0,0,0, 1,1,1,1,1,1,1,1,1,1,
        1,0,0,0,0, 1,1,1,1,1,1,1,1,1,1,1
    };

    const uint8_t expected_high_limit_non_tender_gate_6820af[18] = {
        0x48, 0x8B, 0x75, 0xB8,                         /* mov rsi, [rbp-0x48] */
        0x4C, 0x8B, 0x75, 0xC0,                         /* mov r14, [rbp-0x40] */
        0x45, 0x0F, 0xB6, 0xC7,                         /* movzx r8d, r15b */
        0x49, 0x8B, 0xD5,                               /* mov rdx, r13 */
        0x49, 0x8B, 0xCE                                /* mov rcx, r14 */
    };
    const uint8_t context_high_limit_non_tender_gate_6820af[48] = {
        0xCD, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x95, 0xA0,
        0x01, 0x00, 0x00, 0xE9, 0x41, 0x01, 0x00, 0x00,
        0x48, 0x8B, 0x75, 0xB8, 0x4C, 0x8B, 0x75, 0xC0,
        0x45, 0x0F, 0xB6, 0xC7, 0x49, 0x8B, 0xD5, 0x49,
        0x8B, 0xCE, 0xE8, 0xAA, 0x39, 0x3D, 0x00, 0x41,
        0xC6, 0x85, 0xB0, 0x0D, 0x00, 0x00, 0x00, 0x48
    };
    const uint8_t context_high_limit_non_tender_gate_6820af_mask[48] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,0,0,0,0,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,1,0,0,0,0,1, 1,1,1,1,1,1,1,1
    };

    const uint8_t expected_ai_offer_write_6827cd[13] = {
        0x89, 0x83, 0x7C, 0x08, 0x00, 0x00,             /* mov [rbx+0x87c], eax */
        0x48, 0x8D, 0x05, 0xFE, 0x23, 0x43, 0x02        /* lea 0x142ab4bd8, rax */
    };
    const uint8_t context_ai_offer_write_6827cd[48] = {
        0x45, 0x80, 0x48, 0x8B, 0xD3, 0x49, 0x8B, 0xCF,
        0xE8, 0xB6, 0xC7, 0xFF, 0xFF, 0x8B, 0x45, 0xB8,
        0x89, 0x83, 0x7C, 0x08, 0x00, 0x00, 0x48, 0x8D,
        0x05, 0xFE, 0x23, 0x43, 0x02, 0x48, 0x89, 0x45,
        0x80, 0x48, 0x8D, 0x55, 0x18, 0x48, 0x8D, 0x4D,
        0x18, 0xE8, 0x75, 0xFA, 0xEE, 0xFF, 0xE9, 0xEA
    };
    const uint8_t context_ai_offer_write_6827cd_mask[48] = {
        1,1,1,1,1,1,1,1, 1,0,0,0,0,1,1,1,
        1,1,1,1,1,1,1,1, 1,0,0,0,0,1,1,1,
        1,1,1,1,1,1,1,1, 1,1,0,0,0,0,1,1
    };

    int ok = 0;
    uint8_t* ui_hide_branch_target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_ARBITRATION_WITHDRAW_UI_HIDE_BRANCH_RVA,
        expected_ui_hide_branch,
        sizeof(expected_ui_hide_branch),
        context_ui_hide_branch,
        context_ui_hide_branch_mask,
        sizeof(context_ui_hide_branch),
        16u,
        "KBO salary arbitration withdraw UI hide branch");
    if (ui_hide_branch_target != NULL) {
        ok |= patch_static_bytes(
            "KBO salary arbitration withdraw UI hide branch",
            ui_hide_branch_target,
            expected_ui_hide_branch,
            patch_ui_hide_branch,
            sizeof(expected_ui_hide_branch));
    }
    uint8_t* action_gate_target = resolve_patch_target_by_rva_or_pattern(
        exe,
        OOTP27_ARBITRATION_WITHDRAW_ACTION_GATE_RVA,
        expected_action_gate,
        sizeof(expected_action_gate),
        "KBO salary arbitration withdraw action gate");
    if (action_gate_target != NULL) {
        ok |= patch_static_bytes(
            "KBO salary arbitration withdraw action gate",
            action_gate_target,
            expected_action_gate,
            patch_action_gate,
            sizeof(expected_action_gate));
    }

    uint8_t* final_zero_target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_GATE_RVA,
        expected_final_zero_tender_gate,
        sizeof(expected_final_zero_tender_gate),
        context_final_zero_tender_gate,
        context_final_zero_tender_gate_mask,
        sizeof(context_final_zero_tender_gate),
        16u,
        "KBO salary arbitration AI final zero-offer tender floor");
    if (final_zero_target != NULL) {
        uint8_t* final_zero_continue = KBO_ARBITRATION_RELATED_TARGET(final_zero_target, OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_GATE_RVA, OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_CONTINUE_RVA, "KBO salary arbitration AI final zero-offer tender floor");
        if (final_zero_continue != NULL) {
            uint8_t* final_zero_stub = build_kbo_salary_arbitration_final_zero_tender_stub(
                final_zero_target + sizeof(expected_final_zero_tender_gate),
                final_zero_continue);
            if (final_zero_stub != NULL) {
            ok |= patch_kbo_salary_arbitration_r11_detour_at(
                "KBO salary arbitration AI final zero-offer tender floor",
                final_zero_target,
                expected_final_zero_tender_gate,
                sizeof(expected_final_zero_tender_gate),
                final_zero_stub);
            } else {
                kbo_log_runtime_line("failed to allocate KBO salary arbitration final zero-offer tender floor stub");
            }
        }
    } else {
        kbo_log_runtime_line("KBO salary arbitration AI final zero-offer tender floor target unresolved");
    }

    uint8_t* ai_offer_target_681012 = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_RVA,
        expected_ai_offer_write_681012,
        sizeof(expected_ai_offer_write_681012),
        context_ai_offer_write_681012,
        sizeof(context_ai_offer_write_681012),
        16u,
        "KBO salary arbitration AI offer floor 681012");
    if (ai_offer_target_681012 != NULL) {
        uint8_t* ai_offer_floor_return_681012 = KBO_ARBITRATION_RELATED_TARGET(ai_offer_target_681012, OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_RVA, OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_FLOOR_RETURN_RVA, "KBO salary arbitration AI offer floor 681012");
        uint8_t* ai_offer_pass_return_681012 = KBO_ARBITRATION_RELATED_TARGET(ai_offer_target_681012, OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_RVA, OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_PASS_RETURN_RVA, "KBO salary arbitration AI offer floor 681012");
        if (ai_offer_floor_return_681012 != NULL && ai_offer_pass_return_681012 != NULL) {
            uint8_t* ai_offer_stub_681012 = build_kbo_salary_arbitration_ai_offer_write_681012_stub(
                ai_offer_floor_return_681012,
                ai_offer_pass_return_681012);
            if (ai_offer_stub_681012 != NULL) {
            ok |= patch_kbo_salary_arbitration_r11_detour_at(
            "KBO salary arbitration AI offer floor 681012",
            ai_offer_target_681012,
            expected_ai_offer_write_681012,
            sizeof(expected_ai_offer_write_681012),
            ai_offer_stub_681012);
            } else {
                kbo_log_runtime_line("failed to allocate KBO salary arbitration AI offer floor 681012 stub");
            }
        }
    } else {
        kbo_log_runtime_line("KBO salary arbitration AI offer floor 681012 target unresolved");
    }

    uint8_t* zero_offer_target_682089 = resolve_patch_target_by_rva_or_context_pattern(
        exe,
        OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_RVA,
        expected_zero_offer_check_682089,
        sizeof(expected_zero_offer_check_682089),
        context_zero_offer_check_682089,
        sizeof(context_zero_offer_check_682089),
        16u,
        "KBO salary arbitration AI zero-offer final check 682089");
    if (zero_offer_target_682089 != NULL) {
        uint8_t* zero_offer_tender_return_682089 = KBO_ARBITRATION_RELATED_TARGET(zero_offer_target_682089, OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_RVA, OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_TENDER_RETURN_RVA, "KBO salary arbitration AI zero-offer final check 682089");
        uint8_t* zero_offer_pass_return_682089 = KBO_ARBITRATION_RELATED_TARGET(zero_offer_target_682089, OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_RVA, OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_PASS_RETURN_RVA, "KBO salary arbitration AI zero-offer final check 682089");
        if (zero_offer_tender_return_682089 != NULL && zero_offer_pass_return_682089 != NULL) {
            uint8_t* zero_offer_check_stub_682089 = build_kbo_salary_arbitration_zero_offer_check_682089_stub(
                zero_offer_tender_return_682089,
                zero_offer_pass_return_682089);
            if (zero_offer_check_stub_682089 != NULL) {
            ok |= patch_kbo_salary_arbitration_r11_detour_at(
            "KBO salary arbitration AI zero-offer final check 682089",
            zero_offer_target_682089,
            expected_zero_offer_check_682089,
            sizeof(expected_zero_offer_check_682089),
            zero_offer_check_stub_682089);
            } else {
                kbo_log_runtime_line("failed to allocate KBO salary arbitration AI zero-offer final check 682089 stub");
            }
        }
    } else {
        kbo_log_runtime_line("KBO salary arbitration AI zero-offer final check 682089 target unresolved");
    }

    uint8_t* high_limit_skip_target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_ARBITRATION_HIGH_LIMIT_SKIP_RSI_BRANCH_RVA,
        expected_high_limit_skip_rsi_branch,
        sizeof(expected_high_limit_skip_rsi_branch),
        context_high_limit_skip_rsi_branch,
        context_high_limit_skip_rsi_branch_mask,
        sizeof(context_high_limit_skip_rsi_branch),
        16u,
        "KBO salary arbitration high-limit skip-rsi branch retarget");
    if (high_limit_skip_target != NULL) {
        ok |= patch_static_bytes(
            "KBO salary arbitration high-limit skip-rsi branch retarget",
            high_limit_skip_target,
            expected_high_limit_skip_rsi_branch,
            patch_high_limit_skip_rsi_branch,
            sizeof(expected_high_limit_skip_rsi_branch));
    }

    uint8_t* high_limit_non_tender_target_6820af = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_GATE_RVA,
        expected_high_limit_non_tender_gate_6820af,
        sizeof(expected_high_limit_non_tender_gate_6820af),
        context_high_limit_non_tender_gate_6820af,
        context_high_limit_non_tender_gate_6820af_mask,
        sizeof(context_high_limit_non_tender_gate_6820af),
        16u,
        "KBO salary arbitration high-limit non-tender gate 6820af");
    if (high_limit_non_tender_target_6820af != NULL) {
        uint8_t* high_limit_non_tender_pass_return_6820af = KBO_ARBITRATION_RELATED_TARGET(high_limit_non_tender_target_6820af, OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_GATE_RVA, OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_6820AF_PASS_RETURN_RVA, "KBO salary arbitration high-limit non-tender gate 6820af");
        uint8_t* high_limit_non_tender_floor_return_6820af = KBO_ARBITRATION_RELATED_TARGET(high_limit_non_tender_target_6820af, OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_GATE_RVA, OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_6820AF_FLOOR_RETURN_RVA, "KBO salary arbitration high-limit non-tender gate 6820af");
        if (high_limit_non_tender_pass_return_6820af != NULL
                && high_limit_non_tender_floor_return_6820af != NULL) {
            uint8_t* high_limit_non_tender_stub_6820af = build_kbo_salary_arbitration_high_limit_non_tender_6820af_stub(
                high_limit_non_tender_pass_return_6820af,
                high_limit_non_tender_floor_return_6820af);
            if (high_limit_non_tender_stub_6820af != NULL) {
            ok |= patch_kbo_salary_arbitration_r11_detour_at(
                "KBO salary arbitration high-limit non-tender gate 6820af",
                high_limit_non_tender_target_6820af,
                expected_high_limit_non_tender_gate_6820af,
                sizeof(expected_high_limit_non_tender_gate_6820af),
                high_limit_non_tender_stub_6820af);
            } else {
                kbo_log_runtime_line("failed to allocate KBO salary arbitration high-limit non-tender gate 6820af stub");
            }
        }
    } else {
        kbo_log_runtime_line("KBO salary arbitration high-limit non-tender gate 6820af target unresolved");
    }

    uint8_t* ai_offer_target_6827cd = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_RVA,
        expected_ai_offer_write_6827cd,
        sizeof(expected_ai_offer_write_6827cd),
        context_ai_offer_write_6827cd,
        context_ai_offer_write_6827cd_mask,
        sizeof(context_ai_offer_write_6827cd),
        16u,
        "KBO salary arbitration AI offer floor 6827cd");
    if (ai_offer_target_6827cd != NULL) {
        void* ai_offer_superstar_source_6827cd = resolve_rip_relative_lea_target(ai_offer_target_6827cd + 6);
        if (ai_offer_superstar_source_6827cd == NULL) {
            kbo_log_runtime_line("failed to resolve KBO salary arbitration AI offer floor 6827cd superstar source");
        } else {
            uint8_t* ai_offer_return_6827cd = KBO_ARBITRATION_RELATED_TARGET(ai_offer_target_6827cd, OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_RVA, OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_RETURN_RVA, "KBO salary arbitration AI offer floor 6827cd");
            if (ai_offer_return_6827cd != NULL) {
                uint8_t* ai_offer_stub_6827cd = build_kbo_salary_arbitration_ai_offer_write_6827cd_stub(
                    ai_offer_superstar_source_6827cd,
                    ai_offer_return_6827cd);
                if (ai_offer_stub_6827cd != NULL) {
                ok |= patch_kbo_salary_arbitration_r11_detour_at(
                    "KBO salary arbitration AI offer floor 6827cd",
                    ai_offer_target_6827cd,
                    expected_ai_offer_write_6827cd,
                    sizeof(expected_ai_offer_write_6827cd),
                    ai_offer_stub_6827cd);
                } else {
                    kbo_log_runtime_line("failed to allocate KBO salary arbitration AI offer floor 6827cd stub");
                }
            }
        }
    } else {
        kbo_log_runtime_line("KBO salary arbitration AI offer floor 6827cd target unresolved");
    }

    ok |= install_kbo_salary_arbitration_non_tender_function_patch(exe);

    kbo_log_runtimef("KBO salary arbitration no-withdraw/zero-offer patch complete installed_any=%d", ok ? 1 : 0);
    return ok;
}

#undef KBO_ARBITRATION_RELATED_TARGET
