#include "foreign_ai_offer_contract_type.h"
#include "foreign_signability_offer_attach_probe_utils.h"
#include "../../../../../core/core_league_context_parts/api/league_context_lookup.h"

#include <string.h>

static void kbo_foreign_ai_offer_contract_type_result_init(
    KboForeignAiOfferContractTypeResult* result)
{
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
}

static void kbo_foreign_ai_offer_demand_salary_result_init(
    KboForeignAiOfferDemandSalaryResult* result)
{
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
}

static void kbo_foreign_ai_offer_final_gate_salary_result_init(
    KboForeignAiOfferFinalGateSalaryResult* result)
{
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
}

static int kbo_foreign_ai_offer_demand_salary_plausible(int32_t demand_salary)
{
    return demand_salary > 0 && demand_salary <= 20000000;
}

int kbo_foreign_ai_offer_contract_type_apply_bytes(
    uint8_t* offer,
    size_t offer_size,
    KboForeignAiOfferContractTypeResult* out_result)
{
    KboForeignAiOfferContractTypeResult result;
    kbo_foreign_ai_offer_contract_type_result_init(&result);
    if (offer == NULL || offer_size <= KBO_OFFER_MINOR_FLAG_OFFSET) {
        if (out_result != NULL) {
            *out_result = result;
        }
        return 0;
    }

    result.eligible = 1;
    result.before_major = offer[KBO_OFFER_MAJOR_FLAG_OFFSET];
    result.before_minor = offer[KBO_OFFER_MINOR_FLAG_OFFSET];
    result.after_major = 1u;
    result.after_minor = 0u;

    if (offer[KBO_OFFER_MAJOR_FLAG_OFFSET] != result.after_major) {
        offer[KBO_OFFER_MAJOR_FLAG_OFFSET] = result.after_major;
        result.changed = 1;
    }
    if (offer[KBO_OFFER_MINOR_FLAG_OFFSET] != result.after_minor) {
        offer[KBO_OFFER_MINOR_FLAG_OFFSET] = result.after_minor;
        result.changed = 1;
    }

    if (out_result != NULL) {
        *out_result = result;
    }
    return result.changed;
}

int kbo_foreign_ai_offer_demand_salary_apply_bytes(
    uint8_t* offer,
    size_t offer_size,
    int32_t demand_salary,
    int generated_established_fa,
    int reserve_right,
    KboForeignAiOfferDemandSalaryResult* out_result)
{
    KboForeignAiOfferDemandSalaryResult result;
    kbo_foreign_ai_offer_demand_salary_result_init(&result);
    result.generated_established_fa = generated_established_fa ? 1 : 0;
    result.reserve_right = reserve_right ? 1 : 0;
    result.demand_salary = demand_salary;
    if (offer == NULL
            || offer_size < KBO_OFFER_SALARY_FIRST_YEAR_OFFSET + sizeof(int32_t)
            || !generated_established_fa
            || reserve_right
            || !kbo_foreign_ai_offer_demand_salary_plausible(demand_salary)) {
        if (out_result != NULL) {
            *out_result = result;
        }
        return 0;
    }

    int32_t* primary = (int32_t*)(offer + KBO_OFFER_SALARY_PRIMARY_OFFSET);
    int32_t* first_year = (int32_t*)(offer + KBO_OFFER_SALARY_FIRST_YEAR_OFFSET);
    result.eligible = 1;
    result.before_primary = *primary;
    result.before_first_year = *first_year;
    result.after_primary = result.before_primary;
    result.after_first_year = result.before_first_year;

    if (result.after_primary < demand_salary) {
        *primary = demand_salary;
        result.after_primary = demand_salary;
        result.changed = 1;
    }
    if (result.after_first_year < demand_salary) {
        *first_year = demand_salary;
        result.after_first_year = demand_salary;
        result.changed = 1;
    }

    if (out_result != NULL) {
        *out_result = result;
    }
    return result.changed;
}

int kbo_foreign_ai_offer_final_gate_salary_apply(
    int32_t salary_arg,
    int32_t demand_salary,
    int generated_established_fa,
    int reserve_right,
    KboForeignAiOfferFinalGateSalaryResult* out_result)
{
    KboForeignAiOfferFinalGateSalaryResult result;
    kbo_foreign_ai_offer_final_gate_salary_result_init(&result);
    result.generated_established_fa = generated_established_fa ? 1 : 0;
    result.reserve_right = reserve_right ? 1 : 0;
    result.demand_salary = demand_salary;
    result.before_salary_arg = salary_arg;
    result.after_salary_arg = salary_arg;

    if (!generated_established_fa
            || reserve_right
            || !kbo_foreign_ai_offer_demand_salary_plausible(demand_salary)) {
        if (out_result != NULL) {
            *out_result = result;
        }
        return 0;
    }

    result.eligible = 1;
    if (result.after_salary_arg < demand_salary) {
        result.after_salary_arg = demand_salary;
        result.changed = 1;
    }

    if (out_result != NULL) {
        *out_result = result;
    }
    return result.changed;
}

static uint8_t kbo_foreign_ai_offer_read_u8(uintptr_t offer_ptr, uint32_t offset)
{
    if (offer_ptr == 0
            || offset + sizeof(uint8_t) > KBO_OFFER_READABLE_BYTES
            || !memory_range_readable((void*)(offer_ptr + offset), sizeof(uint8_t))) {
        return 0u;
    }
    return *(uint8_t*)(offer_ptr + offset);
}

static int32_t kbo_foreign_ai_offer_read_i32(uintptr_t offer_ptr, uint32_t offset)
{
    if (offer_ptr == 0
            || offset + sizeof(int32_t) > KBO_OFFER_READABLE_BYTES
            || !memory_range_readable((void*)(offer_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(offer_ptr + offset);
}

static int kbo_foreign_ai_offer_write_i32(uintptr_t offer_ptr, uint32_t offset, int32_t value)
{
    if (offer_ptr == 0
            || offset + sizeof(int32_t) > KBO_OFFER_READABLE_BYTES
            || !memory_range_readable((void*)(offer_ptr + offset), sizeof(int32_t))) {
        return 0;
    }

    void* address = (void*)(offer_ptr + offset);
    DWORD old_protect = 0;
    if (!VirtualProtect(address, sizeof(value), PAGE_READWRITE, &old_protect)) {
        return 0;
    }

    *(int32_t*)address = value;
    DWORD ignored = 0;
    VirtualProtect(address, sizeof(value), old_protect, &ignored);
    return 1;
}

static int kbo_foreign_ai_offer_write_u8(uintptr_t offer_ptr, uint32_t offset, uint8_t value)
{
    if (offer_ptr == 0
            || offset + sizeof(uint8_t) > KBO_OFFER_READABLE_BYTES
            || !memory_range_readable((void*)(offer_ptr + offset), sizeof(uint8_t))) {
        return 0;
    }

    void* address = (void*)(offer_ptr + offset);
    DWORD old_protect = 0;
    if (!VirtualProtect(address, sizeof(value), PAGE_READWRITE, &old_protect)) {
        return 0;
    }

    *(uint8_t*)address = value;
    DWORD ignored = 0;
    VirtualProtect(address, sizeof(value), old_protect, &ignored);
    return 1;
}

static int kbo_foreign_ai_offer_team_is_kbo_org(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id == 0u) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_league_id == kbo_league_id) {
        return 1;
    }

    uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    if (parent_team_id == 0u) {
        return 0;
    }

    uint8_t* parent_team = find_kbo_team_by_numeric_id_any_league(parent_team_id, 1);
    if (parent_team == NULL || !memory_range_readable(parent_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    return *(uint32_t*)(parent_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) == kbo_league_id;
}

static int kbo_foreign_ai_offer_target_is_kbo_org(uintptr_t offer_ptr, int32_t team_id_hint)
{
    if (team_id_hint > 0 && kbo_foreign_ai_offer_team_is_kbo_org((uint32_t)team_id_hint)) {
        return 1;
    }

    int32_t offer_team_id = kbo_foreign_ai_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ID_OFFSET);
    if (offer_team_id > 0 && kbo_foreign_ai_offer_team_is_kbo_org((uint32_t)offer_team_id)) {
        return 1;
    }

    int32_t offer_org_id = kbo_foreign_ai_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ORG_ID_OFFSET);
    return offer_org_id > 0 && kbo_foreign_ai_offer_team_is_kbo_org((uint32_t)offer_org_id);
}

int kbo_foreign_ai_offer_force_major_contract(
    uintptr_t player_ptr,
    uintptr_t offer_ptr,
    int32_t team_id_hint,
    const char* source)
{
    if (player_ptr == 0
            || offer_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)
            || !kbo_foreign_ai_offer_target_is_kbo_org(offer_ptr, team_id_hint)) {
        return 0;
    }

    uint8_t before_major = kbo_foreign_ai_offer_read_u8(offer_ptr, KBO_OFFER_MAJOR_FLAG_OFFSET);
    uint8_t before_minor = kbo_foreign_ai_offer_read_u8(offer_ptr, KBO_OFFER_MINOR_FLAG_OFFSET);
    int changed = 0;
    if (before_major != 1u) {
        changed |= kbo_foreign_ai_offer_write_u8(offer_ptr, KBO_OFFER_MAJOR_FLAG_OFFSET, 1u);
    }
    if (before_minor != 0u) {
        changed |= kbo_foreign_ai_offer_write_u8(offer_ptr, KBO_OFFER_MINOR_FLAG_OFFSET, 0u);
    }

    if (changed) {
        uint8_t after_major = kbo_foreign_ai_offer_read_u8(offer_ptr, KBO_OFFER_MAJOR_FLAG_OFFSET);
        uint8_t after_minor = kbo_foreign_ai_offer_read_u8(offer_ptr, KBO_OFFER_MINOR_FLAG_OFFSET);
        static volatile LONG log_count = 0;
        LONG slot = InterlockedIncrement(&log_count);
        if (slot <= 240) {
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            kbo_log_runtimef(
                "KBO foreign AI offer forced major contract source=%s player=%u team_hint=%d offer=%p offer_team=%d offer_org=%d before_major=%u before_minor=%u after_major=%u after_minor=%u",
                source != NULL ? source : "foreign_ai_offer_contract_type",
                player_id,
                team_id_hint,
                (void*)offer_ptr,
                kbo_foreign_ai_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ID_OFFSET),
                kbo_foreign_ai_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ORG_ID_OFFSET),
                (uint32_t)before_major,
                (uint32_t)before_minor,
                (uint32_t)after_major,
                (uint32_t)after_minor);
        }
    }

    return changed;
}

int32_t kbo_foreign_ai_offer_adjust_final_gate_salary_arg(
    uintptr_t player_ptr,
    uintptr_t offer_ptr,
    int32_t team_id_hint,
    int reserve_right,
    int32_t salary_arg,
    const char* source)
{
    if (player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return salary_arg;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)
            || !kbo_foreign_ai_offer_target_is_kbo_org(offer_ptr, team_id_hint)) {
        return salary_arg;
    }

    uint8_t generation_context = player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET];
    int generated_established_fa = generation_context == 3u;
    int32_t demand_salary = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    KboForeignAiOfferFinalGateSalaryResult result;
    kbo_foreign_ai_offer_final_gate_salary_apply(
        salary_arg,
        demand_salary,
        generated_established_fa,
        reserve_right,
        &result);

    if (result.changed) {
        static volatile LONG log_count = 0;
        LONG slot = InterlockedIncrement(&log_count);
        if (slot <= 240) {
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            kbo_log_runtimef(
                "KBO foreign AI offer final gate salary matched demand baseline source=%s player=%u team_hint=%d offer=%p demand=%d before_salary_arg=%d after_salary_arg=%d generation_context=%u reserve_right=%d",
                source != NULL ? source : "foreign_ai_offer_final_gate_salary",
                player_id,
                team_id_hint,
                (void*)offer_ptr,
                result.demand_salary,
                result.before_salary_arg,
                result.after_salary_arg,
                (uint32_t)generation_context,
                reserve_right ? 1 : 0);
        }
    }

    return result.after_salary_arg;
}

int kbo_foreign_ai_offer_match_demand_salary(
    uintptr_t player_ptr,
    uintptr_t offer_ptr,
    int32_t team_id_hint,
    int reserve_right,
    const char* source)
{
    if (player_ptr == 0
            || offer_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)
            || !kbo_foreign_ai_offer_target_is_kbo_org(offer_ptr, team_id_hint)
            || reserve_right) {
        return 0;
    }

    uint8_t generation_context = player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET];
    int generated_established_fa = generation_context == 3u;
    int32_t demand_salary = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    if (!generated_established_fa
            || !kbo_foreign_ai_offer_demand_salary_plausible(demand_salary)) {
        return 0;
    }

    int32_t before_primary =
        kbo_foreign_ai_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET);
    int32_t before_first_year =
        kbo_foreign_ai_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET);
    int32_t after_primary = before_primary;
    int32_t after_first_year = before_first_year;
    int changed = 0;

    if (after_primary < demand_salary
            && kbo_foreign_ai_offer_write_i32(
                offer_ptr,
                KBO_OFFER_SALARY_PRIMARY_OFFSET,
                demand_salary)) {
        after_primary = demand_salary;
        changed = 1;
    }
    if (after_first_year < demand_salary
            && kbo_foreign_ai_offer_write_i32(
                offer_ptr,
                KBO_OFFER_SALARY_FIRST_YEAR_OFFSET,
                demand_salary)) {
        after_first_year = demand_salary;
        changed = 1;
    }

    if (changed) {
        static volatile LONG log_count = 0;
        LONG slot = InterlockedIncrement(&log_count);
        if (slot <= 240) {
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            kbo_log_runtimef(
                "KBO foreign AI offer salary matched demand baseline source=%s player=%u team_hint=%d offer=%p demand=%d before_primary=%d before_y1=%d after_primary=%d after_y1=%d generation_context=%u reserve_right=%d",
                source != NULL ? source : "foreign_ai_offer_salary_demand",
                player_id,
                team_id_hint,
                (void*)offer_ptr,
                demand_salary,
                before_primary,
                before_first_year,
                after_primary,
                after_first_year,
                (uint32_t)generation_context,
                reserve_right ? 1 : 0);
        }
    }

    return changed;
}
