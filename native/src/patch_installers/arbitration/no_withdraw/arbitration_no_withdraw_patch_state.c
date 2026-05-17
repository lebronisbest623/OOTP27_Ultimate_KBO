#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/logging/core_log.h"
#include "../../../fa_declaration/fa_declaration.h"
#include "../../../fa_filing/fa_filing.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/signability/state/submit_offer_probe_state.h"
#include "../../../hook_stubs/military/hook_stubs_military.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "arbitration_no_withdraw_patch.h"
#include "../common/arbitration_offer_floor_stubs.h"
#include "../common/arbitration_patch_helpers.h"
#include "arbitration_no_withdraw_patch_internal.h"
typedef void (__fastcall *OotpArbitrationNonTenderFn)(void* team, void* player, uint8_t notify);
typedef uint8_t* (__fastcall *KboArbitrationLeagueFinancialsLookupFn)(void* global_db, int32_t league_id);




__declspec(noinline) void ootp_kbo_salary_arbitration_non_tender_wrapper(
    void* team_ptr,
    void* player_ptr,
    uint8_t notify,
    OotpArbitrationNonTenderFn original_func)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (kbo_runtime_save_in_progress()) {
        if (original_func != NULL) {
            KBO_HOOK_PROFILE_PAUSE(profile_hook);
            original_func(team_ptr, player_ptr, notify);
            KBO_HOOK_PROFILE_RESUME(profile_hook);
        }
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "arbitration.non_tender");
    }

    if (player_ptr == NULL || !memory_range_readable(player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        if (original_func != NULL) {
            KBO_HOOK_PROFILE_PAUSE(profile_hook);
            original_func(team_ptr, player_ptr, notify);
            KBO_HOOK_PROFILE_RESUME(profile_hook);
        }
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "arbitration.non_tender");
    }

    void* return_address = __builtin_return_address(0);
    uintptr_t caller_rva = kbo_salary_arbitration_caller_rva(return_address);
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t pre_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t pre_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t player_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t player_draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    uint32_t team_league_id = 0u;
    uint32_t team_id = 0u;
    if (team_ptr != NULL
            && memory_range_readable(
                (uint8_t*)team_ptr + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET,
                OOTP27_KBO_TEAM_ID_OFFSET - OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET + sizeof(uint32_t))) {
        uint8_t* team = (uint8_t*)team_ptr;
        team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    }

    int kbo_context =
        team_league_id == OOTP27_KBO_MAIN_LEAGUE_ID
        || player_league_id == OOTP27_KBO_MAIN_LEAGUE_ID
        || player_draft_league_id == OOTP27_KBO_MAIN_LEAGUE_ID;
    if (!kbo_context) {
        if (original_func != NULL) {
            KBO_HOOK_PROFILE_PAUSE(profile_hook);
            original_func(team_ptr, player_ptr, notify);
            KBO_HOOK_PROFILE_RESUME(profile_hook);
        }
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "arbitration.non_tender");
    }

    uint8_t contract_level = *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    int32_t old_offer = *(int32_t*)(player + OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET);
    uint32_t original_team_id = team_id != 0u
        ? team_id
        : (pre_active_team_id != 0u ? pre_active_team_id : pre_current_team_id);
    uint32_t original_team_league_id = team_league_id != 0u
        ? team_league_id
        : kbo_fa_filing_team_league_id(original_team_id);
    int fa_filing_candidate =
        kbo_fa_filing_is_official_transition_caller(caller_rva)
        && notify == 0u
        && contract_level != 0u
        && nation_id == OOTP27_KBO_KOREA_NATION_ID
        && original_team_id != 0u
        && original_team_league_id == OOTP27_KBO_MAIN_LEAGUE_ID;
    uint32_t today = 0u;
    uint32_t declaration_season = 0u;
    KboFaDeclarationDecision decision;
    memset(&decision, 0, sizeof(decision));
    int fa_declaration_decision_found = 0;
    if (fa_filing_candidate) {
        if (kbo_current_date_tick_latest_published_date(&today) && today != 0u) {
            declaration_season = today / 10000u;
        }
        fa_declaration_decision_found = declaration_season != 0u
            && kbo_fa_declaration_find_latest_decision(player_id, declaration_season, &decision);

        if (fa_declaration_decision_found && decision.declared == 0u) {
            uint32_t floor_league_id = original_team_league_id != 0u
                ? original_team_league_id
                : (player_league_id != 0u ? player_league_id : player_draft_league_id);
            int32_t salary_floor = kbo_salary_arbitration_resolve_minimum_salary(floor_league_id);
            int32_t arbitration_offer = old_offer;
            if (decision.salary > arbitration_offer) {
                arbitration_offer = decision.salary;
            }
            if (salary_floor > arbitration_offer) {
                arbitration_offer = salary_floor;
            }
            if (arbitration_offer > 0) {
                *(int32_t*)(player + OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET) = arbitration_offer;
            }
            int32_t new_offer = *(int32_t*)(player + OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET);

            static LONG fa_declaration_skip_log_count = 0;
            LONG slot = InterlockedIncrement(&fa_declaration_skip_log_count);
            if (slot <= 120) {
                kbo_log_runtimef(
                    "KBO FA declaration transition kept arbitration player=%u original_team=%u team_league=%u declaration_date=%u declaration_season=%u today=%u declared_salary=%d demand=%d old_offer=%d new_offer=%d floor=%d caller_rva=0x%llx",
                    player_id,
                    original_team_id,
                    original_team_league_id,
                    decision.declaration_date,
                    decision.season,
                    today,
                    decision.salary,
                    decision.fa_demand,
                    old_offer,
                    new_offer,
                    salary_floor,
                    (unsigned long long)caller_rva);
            }
            KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "arbitration.non_tender");
        }
        if (fa_declaration_decision_found && decision.declared != 0u) {
            static LONG fa_declaration_pass_log_count = 0;
            LONG slot = InterlockedIncrement(&fa_declaration_pass_log_count);
            if (slot <= 120) {
                kbo_log_runtimef(
                    "KBO FA declaration transition allowed player=%u original_team=%u team_league=%u declaration_date=%u declaration_season=%u today=%u declared_salary=%d demand=%d old_offer=%d caller_rva=0x%llx",
                    player_id,
                    original_team_id,
                    original_team_league_id,
                    decision.declaration_date,
                    decision.season,
                    today,
                    decision.salary,
                    decision.fa_demand,
                    old_offer,
                    (unsigned long long)caller_rva);
            }
            if (original_func != NULL) {
                KBO_HOOK_PROFILE_PAUSE(profile_hook);
                original_func(team_ptr, player_ptr, notify);
                KBO_HOOK_PROFILE_RESUME(profile_hook);
            }
            if (memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
                uint32_t post_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                if (post_current_team_id == 0u) {
                    kbo_record_fa_filing_transition(
                        (uintptr_t)player_ptr,
                        player_id,
                        today,
                        original_team_id,
                        original_team_league_id,
                        (uint32_t)caller_rva,
                        notify,
                        contract_level,
                        "fa_declaration_declared_transition");
                }
            }
            KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "arbitration.non_tender");
        }
    }
    int direct_block_candidate = contract_level != 0u
        && old_offer <= 0
        && kbo_salary_arbitration_is_known_non_tender_return(caller_rva);
    int missing_declaration_transition = fa_filing_candidate && !fa_declaration_decision_found;
    int official_zero_offer_transition =
        kbo_fa_filing_is_official_transition_caller(caller_rva)
        && notify == 0u
        && contract_level != 0u
        && old_offer <= 0
        && nation_id == OOTP27_KBO_KOREA_NATION_ID
        && (team_league_id == OOTP27_KBO_MAIN_LEAGUE_ID
            || player_league_id == OOTP27_KBO_MAIN_LEAGUE_ID
            || player_draft_league_id == OOTP27_KBO_MAIN_LEAGUE_ID);
    int should_block = direct_block_candidate
        || missing_declaration_transition
        || official_zero_offer_transition;
    if (!should_block) {
        static LONG pass_log_count = 0;
        LONG pass_slot = InterlockedIncrement(&pass_log_count);
        if (pass_slot <= 120) {
            kbo_log_runtimef(
                "KBO salary arbitration non-tender pass-through player=%u team=%u team_league=%u player_league=%u draft_league=%u contract_level=%u offer=%d notify=%u caller_rva=0x%llx direct_block_candidate=%d official_zero_offer_transition=%d",
                player_id,
                team_id,
                team_league_id,
                player_league_id,
                player_draft_league_id,
                (unsigned)contract_level,
                old_offer,
                (unsigned)notify,
                (unsigned long long)caller_rva,
                direct_block_candidate ? 1 : 0,
                official_zero_offer_transition ? 1 : 0);
        }
        if (original_func != NULL) {
            KBO_HOOK_PROFILE_PAUSE(profile_hook);
            original_func(team_ptr, player_ptr, notify);
            KBO_HOOK_PROFILE_RESUME(profile_hook);
        }
        if (fa_filing_candidate && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            uint32_t post_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            if (post_current_team_id == 0u) {
                uint32_t today = 0u;
                if (kbo_current_date_tick_latest_published_date(&today) && today != 0u) {
                    kbo_record_fa_filing_transition(
                        (uintptr_t)player_ptr,
                        player_id,
                        today,
                        original_team_id,
                        original_team_league_id,
                        (uint32_t)caller_rva,
                        notify,
                        contract_level,
                        "arbitration_non_tender_67d11e");
                }
            }
        }
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "arbitration.non_tender");
    }

    uint32_t floor_league_id = team_league_id != 0u
        ? team_league_id
        : (player_league_id != 0u ? player_league_id : player_draft_league_id);
    int32_t salary_floor = kbo_salary_arbitration_resolve_minimum_salary(floor_league_id);
    if (old_offer < salary_floor) {
        *(int32_t*)(player + OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET) = salary_floor;
    }

    static LONG block_log_count = 0;
    LONG slot = InterlockedIncrement(&block_log_count);
    if (slot <= 120) {
        kbo_log_runtimef(
            "KBO salary arbitration non-tender blocked player=%u team=%u team_league=%u player_league=%u draft_league=%u contract_level=%u old_offer=%d floor=%d notify=%u caller_rva=0x%llx direct_block_candidate=%d missing_declaration_transition=%d official_zero_offer_transition=%d declaration_season=%u",
            player_id,
            team_id,
            team_league_id,
            player_league_id,
            player_draft_league_id,
            (unsigned)contract_level,
            old_offer,
            salary_floor,
            (unsigned)notify,
            (unsigned long long)caller_rva,
            direct_block_candidate ? 1 : 0,
            missing_declaration_transition ? 1 : 0,
            official_zero_offer_transition ? 1 : 0,
            declaration_season);
    }
    KBO_HOOK_PROFILE_END(profile_hook, "arbitration.non_tender");
}


uint8_t* build_kbo_salary_arbitration_non_tender_detour_stub(void* original_trampoline)
{
    uint8_t code[32] = {
        0x49, 0xB9,                                     /* mov r9, original_trampoline */
        0,0,0,0,0,0,0,0,
        0x48, 0xB8,                                     /* mov rax, wrapper */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[2], (uint64_t)(uintptr_t)original_trampoline);
    write_u64(&code[12], (uint64_t)(uintptr_t)&ootp_kbo_salary_arbitration_non_tender_wrapper);
    return allocate_kbo_salary_arbitration_stub(code, sizeof(code));
}





