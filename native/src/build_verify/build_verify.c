#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "build_verify.h"
#include "../bootstrap/abi/ootp_offsets.h"
#include "../core/logging/core_log.h"

#include "supported_builds.generated.h"

OotpBuildInfo read_ootp_build_info(void)
{
    OotpBuildInfo info = {0};

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("build verify: GetModuleHandleA(NULL) returned NULL");
        return info;
    }

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)exe;
    if (IsBadReadPtr(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        kbo_log_runtime_line("build verify: invalid DOS header");
        return info;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((uint8_t*)exe + dos->e_lfanew);
    if (IsBadReadPtr(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        kbo_log_runtime_line("build verify: invalid NT header");
        return info;
    }

    info.ok = 1;
    info.timestamp = nt->FileHeader.TimeDateStamp;
    info.size_of_image = nt->OptionalHeader.SizeOfImage;
    return info;
}

size_t kbo_supported_ootp_build_count(void)
{
    return sizeof(KBO_SUPPORTED_OOTP_BUILDS) / sizeof(KBO_SUPPORTED_OOTP_BUILDS[0]);
}

const OotpSupportedBuild* kbo_supported_ootp_build_at(size_t index)
{
    if (index >= kbo_supported_ootp_build_count()) {
        return NULL;
    }
    return &KBO_SUPPORTED_OOTP_BUILDS[index];
}

int kbo_ootp_build_is_steam_2026_05_04(OotpBuildInfo info)
{
    return info.ok
        && info.timestamp == KBO_SUPPORTED_OOTP_BUILD_STEAM_2026_05_04_TIMESTAMP
        && info.size_of_image == KBO_SUPPORTED_OOTP_BUILD_STEAM_2026_05_04_SIZE_OF_IMAGE;
}

void* kbo_resolve_build_specific_rva_ptr(HMODULE exe, uint32_t steam_rva)
{
    if (exe == NULL) {
        return NULL;
    }

    OotpBuildInfo info = read_ootp_build_info();
    uint32_t rva = 0u;
    if (kbo_ootp_build_is_steam_2026_05_04(info)) {
        rva = steam_rva;
    } else if (info.ok
            && info.timestamp == KBO_SUPPORTED_OOTP_BUILD_OFFICIAL_27_2_60_TIMESTAMP
            && info.size_of_image == KBO_SUPPORTED_OOTP_BUILD_OFFICIAL_27_2_60_SIZE_OF_IMAGE) {
        switch (steam_rva) {
            case OOTP27_CREATE_LEAGUE_EVENT_RVA: rva = 0x00A49EB0u; break;
            case OOTP27_PISD_STRING_ASSIGN_RVA: rva = 0x01B990F0u; break;
            case OOTP27_LEAGUE_NEWS_REAL_ADD_RVA: rva = 0x003A0E10u; break;
            case OOTP27_NEWS_OBJECT_CTOR_RVA: rva = 0x005C3030u; break;
            case OOTP27_NEWS_STRING_ENSURE_RVA: rva = 0x005C5AA0u; break;
            case OOTP27_NEWS_ADD_RELATED_PLAYER_RVA: rva = 0x005CAF40u; break;
            case OOTP27_NEWS_ADD_RELATED_TEAM_RVA: rva = 0x005CB1C0u; break;
            case OOTP27_CREATE_MESSAGE_CORE_RVA: rva = 0x011DCDE0u; break;
            case OOTP27_UI_OPERATOR_NEW_RVA: rva = 0x024872C4u; break;
            case OOTP27_PLAYER_TOOLTIP_HOVER_MANAGER_RVA: rva = 0x00F51670u; break;
            case OOTP27_PLAYER_TOOLTIP_FACTORY_RVA: rva = 0x019F7A50u; break;
            case OOTP27_PLAYER_TOOLTIP_RENDER_RVA: rva = 0x019FBE60u; break;
            case OOTP27_PLAYER_TOOLTIP_TEXT_APPEND_RVA: rva = 0x01B08BD0u; break;
            case OOTP27_PLAYER_TOOLTIP_STRING_FORMAT_RVA: rva = 0x01B99470u; break;
            case OOTP27_PLAYER_TOOLTIP_CURRENT_GLOBAL_RVA: rva = 0x02E227B0u; break;
            case OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA: rva = 0x00414330u; break;
            case OOTP27_ALLSTAR_TEAM_SETUP_FUNC_RVA: rva = 0x0031F09Cu; break;
            case OOTP27_ALLSTAR_CANDIDATE_REBUILD_FUNC_RVA: rva = 0x00539340u; break;
            case OOTP27_MAKE_ALLSTAR_GAME_EVENTS_RVA: rva = 0x0053D193u; break;
            case OOTP27_SQL_EXEC_RVA: rva = 0x011195A0u; break;
            case OOTP27_SQL_EXEC_DIRECT_RVA: rva = 0x0031049Cu; break;
            case OOTP27_PLAYER_TEAM_SIGNABILITY_RVA: rva = 0x00732620u; break;
            case OOTP27_PLAYER_SIGNABILITY_BLOCK_849320_RVA: rva = 0x0071F600u; break;
            case OOTP27_FA_SUBMIT_OFFER_PROBE_RVA: rva = 0x00339BFAu; break;
            case OOTP27_FA_OFFER_SCREEN_CALLBACK_PROBE_RVA: rva = 0x0168CD00u; break;
            case OOTP27_FA_CONTRACT_OFFER_CALLBACK_PROBE_RVA: rva = 0x01294EF0u; break;
            case OOTP27_FA_FILING_OFFICIAL_TRANSITION_RETURN_RVA: rva = 0x005533CEu; break;
            case OOTP27_TEAM_ADD_PLAYER_RVA: rva = 0x0091EBF0u; break;
            case OOTP27_LEAGUE_TRADE_CHECK_RVA: rva = 0x009FA060u; break;
            case OOTP27_AI_FA_STATUS_CANDIDATE_INSERT_RVA: rva = 0x009C707Bu; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_638D32_RVA: rva = 0x0050EFF2u; break;
            case OOTP27_NO_MINOR_CONTRACT_DYNAMIC_ZERO_6331F5_RVA: rva = 0x005094B5u; break;
            case OOTP27_NO_MINOR_CONTRACT_OFFER_MAJOR_FLAG_COPY_13B0235_RVA: rva = 0x01294F55u; break;
            case OOTP27_NO_MINOR_CONTRACT_OFFER_REPORT_MINOR_ITEM_17DA0EC_RVA: rva = 0x016BF57Cu; break;
            case OOTP27_NO_MINOR_CONTRACT_PLAYER_ACTION_ELIGIBILITY_RVA: rva = 0x00F56B50u; break;
            case OOTP27_NO_MINOR_CONTRACT_SUBMIT_SALARY_FLOOR_13B0712_RVA: rva = 0x01295432u; break;
            case OOTP27_NO_MINOR_CONTRACT_SUBMIT_ZERO_ERROR_13B072C_RVA: rva = 0x0129544Cu; break;
            case OOTP27_NO_MINOR_CONTRACT_SUBMIT_MIN_CHECK_13B07BB_RVA: rva = 0x012954DBu; break;
            case OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_RVA: rva = 0x00980251u; break;
            case OOTP27_NO_MINOR_CONTRACT_FA_DEMAND_WRITE_AAB739_DONE_RVA: rva = 0x005556A8u; break;
            case OOTP27_FOREIGN_FA_DEMAND_BASELINE_PREPARE_AAB624_RVA: rva = 0x0097E46Eu; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_6393A3_RVA: rva = 0x0050F519u; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_6DE33E_RVA: rva = 0x005B461Eu; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_6DE58C_RVA: rva = 0x005B486Cu; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_6DE6BF_RVA: rva = 0x005B499Fu; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_6DE96E_RVA: rva = 0x005B4C4Eu; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_5568EF_RVA: rva = 0x0042C7EFu; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_A4BE04_RVA: rva = 0x00321308u; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_2184B61_RVA: rva = 0x02065C41u; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_2185963_RVA: rva = 0x02066A43u; break;
            case OOTP27_NO_MINOR_CONTRACT_WRITE_2185D00_RVA: rva = 0x00312CA0u; break;
            case OOTP27_ARBITRATION_WITHDRAW_ACTION_GATE_RVA: rva = 0x01245878u; break;
            case OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_GATE_RVA: rva = 0x00557DD0u; break;
            case OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_RVA: rva = 0x00393262u; break;
            case OOTP27_ARBITRATION_HIGH_LIMIT_SKIP_RSI_BRANCH_RVA: rva = 0x0055807Fu; break;
            case OOTP27_ARBITRATION_NON_TENDER_FUNCTION_RVA: rva = 0x0092A8E0u; break;
            case OOTP27_INTL_ESTABLISHED_FA_PLAYER_PROBE_RVA: rva = 0x0058BA19u; break;
            case OOTP27_INTL_ESTABLISHED_FA_REGISTER_GATE_RVA: rva = 0x0035D40Fu; break;
            case OOTP27_INTL_ESTABLISHED_FA_PROGRESS_TITLE_RVA: rva = 0x02755EF8u; break;
            case OOTP27_ACTIVE_FOREIGN_PITCHER_COUNT_RVA: rva = 0x00907650u; break;
            case OOTP27_SECONDARY_FOREIGN_HITTER_COUNT_RVA: rva = 0x009076E0u; break;
            case OOTP27_SECONDARY_FOREIGN_PITCHER_COUNT_RVA: rva = 0x00907B50u; break;
            case OOTP27_CALLUP_FOREIGN_HITTER_LIMIT_BRANCH_RVA: rva = 0x00F633B5u; break;
            case OOTP27_IMPORT_SCHEDULE_ALLSTAR_DAY_GATE_RVA: rva = 0x007BCE75u; break;
            case OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_SITE_RVA: rva = 0x005AD4EAu; break;
            case OOTP27_ALLSTAR_CANDIDATE_TEAM_SPLIT_DONE_RVA: rva = 0x005AD798u; break;
            case OOTP27_ALLSTAR_CANDIDATE_TEAM_ROSTER_PUSH_SITE_RVA: rva = 0x005AD4EEu; break;
            case OOTP27_ALLSTAR_CANDIDATE_PLAYER_PUSH_SITE_RVA: rva = 0x005AD877u; break;
            case OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_3_RVA: rva = 0x003364C9u; break;
            case OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_6_RVA: rva = 0x005AF1BAu; break;
            case OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA: rva = 0x005AF121u; break;
            case OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_FALLBACK_SKIP_RVA: rva = 0x004EF000u; break;
            case OOTP27_ALLSTAR_VOTING_BEGIN_PREP_SITE_RVA: rva = 0x0063A03Bu; break;
            case OOTP27_ALLSTAR_TEAM_SETUP_SINGLE_DIVISION_GATE_RVA: rva = 0x0053CDE2u; break;
            case OOTP27_MAKE_ALLSTAR_EVENTS_PREP_SITE_RVA: rva = 0x0053C9C8u; break;
            case OOTP27_ALLSTAR_TEAM_SETUP_SINGLE_DIVISION_BAIL_RVA: rva = 0x0053CB35u; break;
            case OOTP27_SEASON_PHASE_WRITE_1_RVA: rva = 0x00A40DC1u; break;
            case OOTP27_SEASON_PHASE_WRITE_0_RVA: rva = 0x00A415E7u; break;
            case OOTP27_SEASON_PHASE_WRITE_4_RVA: rva = 0x00A41AD8u; break;
            case OOTP27_SEASON_PHASE_WRITE_2A_RVA: rva = 0x00A43427u; break;
            case OOTP27_SEASON_PHASE_WRITE_2B_RVA: rva = 0x00A4630Bu; break;
            case OOTP27_SEASON_PHASE_WRITE_2C_RVA: rva = 0x00B6C221u; break;
            case OOTP27_SEASON_PHASE_WRITE_3A_RVA: rva = 0x00523DE2u; break;
            case OOTP27_SEASON_PHASE_WRITE_3B_RVA: rva = 0x00312059u; break;
            case OOTP27_SEASON_PHASE_WRITE_3C_RVA: rva = 0x01330924u; break;
            case OOTP27_SEASON_PHASE_INIT_0A_RVA: rva = 0x00426D64u; break;
            case OOTP27_SEASON_PHASE_INIT_0B_RVA: rva = 0x0042B537u; break;
            case OOTP27_SEASON_PHASE_INIT_2_RVA: rva = 0x0042BDE4u; break;
            case OOTP27_SEASON_PHASE_INIT_0C_RVA: rva = 0x0042C96Fu; break;
            case OOTP27_UI_CHECKBOX_SET_BOOL_RVA: rva = 0x01D5B500u; break;
            case OOTP27_VECTOR_PUSH_BACK_RVA: rva = 0x01BA4850u; break;
            case OOTP27_ARBITRATION_AI_FINAL_ZERO_TENDER_CONTINUE_RVA: rva = 0x00557EB6u; break;
            case OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_FLOOR_RETURN_RVA: rva = 0x0031EEB0u; break;
            case OOTP27_ARBITRATION_AI_OFFER_WRITE_681012_PASS_RETURN_RVA: rva = 0x005582FAu; break;
            case OOTP27_ARBITRATION_AI_ZERO_OFFER_CHECK_682089_TENDER_RETURN_RVA: rva = 0x00558367u; break;
            case OOTP27_ARBITRATION_HIGH_LIMIT_NON_TENDER_6820AF_PASS_RETURN_RVA: rva = 0x005584AFu; break;
            case OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_RETURN_RVA: rva = 0x00558A8Au; break;
            case OOTP27_ALLSTAR_SETTINGS_ONE_DIVISION_WARNING_GATE_RVA: rva = 0x0142B028u; break;
            case OOTP27_FOREIGN_CALLUP_LIMIT_TOTAL_CHECK_FALLBACK_RVA: rva = 0x00F64BE4u; break;
            default: break;
        }
    } else if (info.ok
            && info.timestamp == KBO_SUPPORTED_OOTP_BUILD_OFFICIAL_27_2_59_TIMESTAMP
            && info.size_of_image == KBO_SUPPORTED_OOTP_BUILD_OFFICIAL_27_2_59_SIZE_OF_IMAGE) {
        switch (steam_rva) {
            case OOTP27_CREATE_LEAGUE_EVENT_RVA: rva = 0x00A4A890u; break;
            case OOTP27_PISD_STRING_ASSIGN_RVA: rva = 0x01BA62E0u; break;
            case OOTP27_LEAGUE_NEWS_REAL_ADD_RVA: rva = 0x003A0E10u; break;
            case OOTP27_NEWS_OBJECT_CTOR_RVA: rva = 0x005C3030u; break;
            case OOTP27_NEWS_STRING_ENSURE_RVA: rva = 0x005C5AA0u; break;
            case OOTP27_NEWS_ADD_RELATED_PLAYER_RVA: rva = 0x005CAF40u; break;
            case OOTP27_NEWS_ADD_RELATED_TEAM_RVA: rva = 0x005CB1C0u; break;
            case OOTP27_CREATE_MESSAGE_CORE_RVA: rva = 0x011DDD90u; break;
            case OOTP27_UI_OPERATOR_NEW_RVA: rva = 0x024957D4u; break;
            case OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA: rva = 0x00414330u; break;
            default: break;
        }
    }

    return rva != 0u ? (void*)((uint8_t*)exe + rva) : NULL;
}

int verify_ootp_build(void)
{
    OotpBuildInfo info = read_ootp_build_info();
    if (!info.ok) {
        kbo_log_runtime_line("build verify: failed to read PE headers; skipping all KBO patches");
        return 0;
    }

    if (KBO_SUPPORTED_OOTP_BUILD_COUNT == 0u) {
        kbo_log_runtimef(
            "build verify: discovery mode; detected timestamp=0x%08X size_of_image=0x%08X",
            info.timestamp, info.size_of_image);
        return 1;
    }

    for (size_t i = 0; i < kbo_supported_ootp_build_count(); ++i) {
        const OotpSupportedBuild* build = kbo_supported_ootp_build_at(i);
        if (build != NULL && info.timestamp == build->timestamp && info.size_of_image == build->size_of_image) {
            if (!build->native_patches_supported) {
                kbo_log_runtimef(
                    "build verify: metadata-only label=%s timestamp=0x%08X size_of_image=0x%08X. "
                    "Native patches DISABLED because this build does not have a complete verified RVA table.",
                    build->label, info.timestamp, info.size_of_image);
                return 0;
            }

            kbo_log_runtimef(
                "build verify: ok label=%s timestamp=0x%08X size_of_image=0x%08X",
                build->label, info.timestamp, info.size_of_image);
            return 1;
        }
    }

    kbo_log_runtimef(
        "build verify: MISMATCH expected one of %u supported builds, "
        "detected timestamp=0x%08X size_of_image=0x%08X. "
        "All KBO patches DISABLED to prevent crashes or save corruption.",
        (unsigned)kbo_supported_ootp_build_count(),
        info.timestamp,
        info.size_of_image);

    return 0;
}
