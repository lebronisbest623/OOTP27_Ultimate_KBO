#include "foreign_signability_foreign_ai_offer_attach_probe_log.h"
#include "../../../internal/foreign_signability_internal.h"
#include "../foreign_signability_offer_attach_probe_utils.h"
#include "../../../../../../fa_market_investigation/probe/domestic_fa_offer_probe.h"

static int kbo_foreign_ai_offer_attach_should_log(
    uint8_t* player,
    uint32_t player_id,
    uint32_t today,
    uint32_t* out_holder_team_id)
{
    if (out_holder_team_id != NULL) {
        *out_holder_team_id = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t holder_team_id = 0u;
    int has_holder = player_id != 0u
        && today != 0u
        && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
        && holder_team_id != 0u;
    if (out_holder_team_id != NULL && has_holder) {
        *out_holder_team_id = holder_team_id;
    }

    if (has_holder || kbo_player_is_foreign_for_kbo_rights(player)) {
        return 1;
    }

    return kbo_domestic_fa_offer_probe_should_log_player(player);
}

int32_t kbo_offer_probe_player_value_score(uint8_t* player)
{
    if (kbo_domestic_fa_offer_probe_should_log_player(player)) {
        return kbo_domestic_fa_offer_probe_value_score(player);
    }
    return kbo_foreign_waiver_value_score(player);
}

void kbo_log_foreign_ai_offer_attach(
    uintptr_t player_ptr,
    uintptr_t offer_slot_ptr,
    uintptr_t caller_return_ptr)
{
    if (player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)
            || offer_slot_ptr == 0
            || !memory_range_readable((void*)offer_slot_ptr, sizeof(uintptr_t))) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        today = 0u;
    }

    uint32_t holder_team_id = 0u;
    if (!kbo_foreign_ai_offer_attach_should_log(player, player_id, today, &holder_team_id)) {
        return;
    }

    static LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 600) {
        return;
    }

    uintptr_t offer_ptr = *(uintptr_t*)offer_slot_ptr;
    uint32_t caller_rva = kbo_foreign_ai_offer_attach_caller_rva(caller_return_ptr);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint8_t position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    uint8_t position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);
    int32_t demand_salary = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    int32_t score = kbo_offer_probe_player_value_score(player);
    int32_t selected_offer_id = *(int32_t*)(player + KBO_PLAYER_SELECTED_OFFER_ID_OFFSET);

    kbo_log_runtimef(
        "foreign ai offer attach probe #%ld caller_rva=0x%x player=%u nation=%u pos_group=%u pos_role=%u holder_team=%u today=%u current=%u active=%u original=%u demand=%d score=%d selected_offer=%d offer=%p offer_team=%d offer_org=%d salary24=%d salary38=%d years=%u flags_aa=%u ab=%u ac=%u d0=%u d8=%u type_ae=%u type_b0=%u",
        slot,
        caller_rva,
        player_id,
        nation_id,
        (uint32_t)position_group,
        (uint32_t)position_role,
        holder_team_id,
        today,
        current_team_id,
        active_team_id,
        original_team_id,
        demand_salary,
        score,
        selected_offer_id,
        (void*)offer_ptr,
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ORG_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_YEAR_COUNT_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AA_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AB_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AC_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_D0_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_D8_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_AE_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_B0_OFFSET));
}

static int kbo_foreign_ai_offer_decision_should_log(
    uint8_t* player,
    uint32_t player_id,
    uint32_t today,
    uint32_t* out_holder_team_id)
{
    if (out_holder_team_id != NULL) {
        *out_holder_team_id = 0u;
    }
    if (player == NULL
            || player_id == 0u
            || today == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t holder_team_id = 0u;
    int has_holder = kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
        && holder_team_id != 0u;
    if (out_holder_team_id != NULL && has_holder) {
        *out_holder_team_id = holder_team_id;
    }
    if (has_holder) {
        return 1;
    }

    if (read_kbo_localappdata_flag_file("enable_kbo_custom_foreign_offer_logs.txt")
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        return 1;
    }

    return kbo_domestic_fa_offer_probe_should_log_player(player);
}

void kbo_log_foreign_ai_offer_build(
    uintptr_t player_ptr,
    int32_t team_id,
    uintptr_t flag_ptr,
    uintptr_t offer_ptr)
{
    if (player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        today = 0u;
    }

    uint32_t holder_team_id = 0u;
    if (!kbo_foreign_ai_offer_decision_should_log(player, player_id, today, &holder_team_id)) {
        return;
    }

    static LONG build_log_count = 0;
    LONG slot = InterlockedIncrement(&build_log_count);
    if (slot > 1000) {
        return;
    }

    kbo_log_runtimef(
        "foreign ai offer build probe #%ld player=%u holder_team=%u today=%u team_arg=%d current=%u active=%u original=%u demand=%d score=%d flag_ptr=%p flag_value=%u offer=%p offer_major=%u offer_minor=%u offer_team=%d offer_org=%d salary24=%d salary38=%d years=%u flags_aa=%u ab=%u ac=%u type_ae=%u type_b0=%u",
        slot,
        player_id,
        holder_team_id,
        today,
        team_id,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
        *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET),
        kbo_offer_probe_player_value_score(player),
        (void*)flag_ptr,
        flag_ptr != 0 && memory_range_readable((void*)flag_ptr, sizeof(uint8_t)) ? (uint32_t)*(uint8_t*)flag_ptr : 0u,
        (void*)offer_ptr,
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_MAJOR_FLAG_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_MINOR_FLAG_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ORG_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_YEAR_COUNT_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AA_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AB_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AC_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_AE_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_B0_OFFSET));
}

void kbo_log_foreign_ai_offer_final_gate(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary,
    uintptr_t offer_ptr,
    uint8_t result)
{
    if (player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        today = 0u;
    }

    uint32_t holder_team_id = 0u;
    if (!kbo_foreign_ai_offer_decision_should_log(player, player_id, today, &holder_team_id)) {
        return;
    }

    static LONG gate_log_count = 0;
    LONG slot = InterlockedIncrement(&gate_log_count);
    if (slot > 1000) {
        return;
    }

    kbo_log_runtimef(
        "foreign ai offer final gate probe #%ld result=%u player=%u holder_team=%u today=%u team=%u salary_arg=%d current=%u active=%u original=%u demand=%d score=%d offer=%p offer_major=%u offer_minor=%u offer_team=%d offer_org=%d salary24=%d salary38=%d years=%u flags_aa=%u ab=%u ac=%u type_ae=%u type_b0=%u",
        slot,
        (uint32_t)result,
        player_id,
        holder_team_id,
        today,
        kbo_offer_probe_team_id_from_ptr(team_ptr),
        salary,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
        *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET),
        kbo_offer_probe_player_value_score(player),
        (void*)offer_ptr,
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_MAJOR_FLAG_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_MINOR_FLAG_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ORG_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_YEAR_COUNT_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AA_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AB_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AC_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_AE_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_B0_OFFSET));
}
