#include "../../internal/foreign_signability_internal.h"
#include "foreign_signability_offer_attach_probe_utils.h"
#include "../../../../../build_verify/build_verify.h"
#include "../../../../../fa_market_investigation/probe/domestic_fa_offer_probe.h"
#include "../../../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../../../controller/foreign_ai_controller.h"
#include "../../../api/foreign_signability_salary_floor.h"
#include "../../../../../core/core_league_context_parts/api/league_context_lookup.h"

typedef void (__fastcall *KboOotpForeignAiOfferAttachFn)(uintptr_t player_ptr, uintptr_t offer_slot_ptr);
typedef uintptr_t (__fastcall *KboOotpForeignAiOfferBuildFn)(
    uintptr_t player_ptr,
    int32_t team_id,
    uintptr_t zero_arg,
    uintptr_t flag_ptr,
    uint8_t stack_flag);
typedef uint8_t (__fastcall *KboOotpForeignAiOfferFinalGateFn)(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary);

void kbo_prepare_foreign_fa_offer_demand_baseline(uintptr_t player_ptr, const char* source);
void kbo_restore_foreign_fa_demand_salary_ladder(const char* source);

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

static int32_t kbo_offer_probe_player_value_score(uint8_t* player)
{
    if (kbo_domestic_fa_offer_probe_should_log_player(player)) {
        return kbo_domestic_fa_offer_probe_value_score(player);
    }
    return kbo_foreign_waiver_value_score(player);
}

static int kbo_foreign_ai_fast_fill_offer_gate_enabled(void)
{
    enum { KBO_FOREIGN_AI_FAST_FILL_FLAG_CACHE_MS = 500u };
    static volatile LONG s_cached_tick = 0;
    static volatile LONG s_cached_enabled = 0;

    DWORD now = GetTickCount();
    LONG cached_tick = InterlockedCompareExchange(&s_cached_tick, 0, 0);
    if (cached_tick != 0
            && (DWORD)(now - (DWORD)cached_tick) <= KBO_FOREIGN_AI_FAST_FILL_FLAG_CACHE_MS) {
        return InterlockedCompareExchange(&s_cached_enabled, 0, 0) != 0;
    }

    int enabled = kbo_custom_foreign_policy_enabled()
        && (read_kbo_localappdata_flag_file("enable_foreign_ai_roster_management.txt")
            || kbo_foreign_ai_controller_enabled())
        && !read_kbo_localappdata_flag_file("disable_kbo_foreign_ai_fast_fill_offer_gate.txt");
    InterlockedExchange(&s_cached_enabled, enabled ? 1 : 0);
    InterlockedExchange(&s_cached_tick, (LONG)now);
    return enabled;
}

static uint8_t kbo_foreign_ai_fast_fill_offer_final_gate(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary,
    uintptr_t offer_ptr,
    uint8_t original_result)
{
    if (original_result != 0u || !kbo_foreign_ai_fast_fill_offer_gate_enabled()) {
        return original_result;
    }
    if (team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return original_result;
    }

    uint32_t team_id = kbo_offer_probe_team_id_from_ptr(team_ptr);
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (team_id == 0u
            || player_id == 0u
            || !kbo_player_is_foreign_for_kbo_rights(player)
            || !kbo_custom_foreign_policy_can_override_original_block(player, team_id)
            || kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)
            || *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != 0u) {
        return original_result;
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today) || today == 0u) {
        return original_result;
    }

    uint32_t pending_asian = 0u;
    uint32_t pending_non_asian = 0u;
    int candidate_pending = 0;
    kbo_custom_foreign_count_pending_offers(
        team_id,
        today,
        player_id,
        &pending_asian,
        &pending_non_asian,
        &candidate_pending);
    if (candidate_pending) {
        return original_result;
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    int allowed = kbo_custom_foreign_policy_team_allows_candidate(
        team_id,
        player,
        &effective_before,
        &effective_after,
        &effective_limit,
        &slot_type,
        &injured_player_id);

    uint32_t base_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    if (!allowed || base_limit == 0u || effective_after > base_limit) {
        return original_result;
    }

    kbo_record_custom_foreign_pending_offer(team_id, player, today);
    kbo_record_recent_custom_foreign_policy_allow(player_id, team_id, today);

    static volatile LONG fast_fill_log_count = 0;
    LONG slot = InterlockedIncrement(&fast_fill_log_count);
    if (slot <= 300) {
        kbo_log_runtimef(
            "foreign ai fast-fill offer final gate adjusted player=%u team=%u original=%u adjusted=1 effective_before=%u effective_after=%u limit=%u base_limit=%u pending_asian=%u pending_non_asian=%u asian=%u injury_slot=%s injured=%u salary_arg=%d demand=%d score=%d offer=%p offer_salary=%d offer_years=%u today=%u",
            player_id,
            team_id,
            (uint32_t)original_result,
            effective_before,
            effective_after,
            effective_limit,
            base_limit,
            pending_asian,
            pending_non_asian,
            kbo_player_is_asian_quota_candidate(player) ? 1u : 0u,
            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
            injured_player_id,
            salary,
            *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET),
            kbo_offer_probe_player_value_score(player),
            (void*)offer_ptr,
            kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET),
            (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_YEAR_COUNT_OFFSET),
            today);
    }
    return 1u;
}

static int kbo_foreign_ai_offer_target_is_kbo_org(
    uint32_t team_id,
    uint32_t* out_team_league_id,
    uint32_t* out_parent_team_id)
{
    if (out_team_league_id != NULL) { *out_team_league_id = 0u; }
    if (out_parent_team_id != NULL) { *out_parent_team_id = 0u; }
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
    uint32_t parent_team_id = memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET)
        : 0u;
    if (out_team_league_id != NULL) { *out_team_league_id = team_league_id; }
    if (out_parent_team_id != NULL) { *out_parent_team_id = parent_team_id; }
    if (team_league_id == kbo_league_id) {
        return 1;
    }

    if (parent_team_id == 0u) {
        return 0;
    }

    uint8_t* parent_team = find_kbo_team_by_numeric_id_any_league(parent_team_id, 1);
    return parent_team != NULL
        && memory_range_readable(parent_team, OOTP27_KBO_TEAM_READABLE_BYTES)
        && *(uint32_t*)(parent_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) == kbo_league_id;
}

static int kbo_force_foreign_ai_offer_major_terms(
    uintptr_t player_ptr,
    int32_t team_id_arg,
    uintptr_t offer_ptr,
    const char* source,
    int32_t* out_salary_floor)
{
    if (out_salary_floor != NULL) { *out_salary_floor = 0; }
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || offer_ptr == 0
            || !memory_range_readable((void*)offer_ptr, KBO_OFFER_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }

    uint32_t team_id = team_id_arg > 0 ? (uint32_t)team_id_arg : 0u;
    uint32_t offer_team_id = (uint32_t)kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ID_OFFSET);
    uint32_t checked_team_id = team_id != 0u ? team_id : offer_team_id;
    uint32_t team_league_id = 0u;
    uint32_t parent_team_id = 0u;
    if (!kbo_foreign_ai_offer_target_is_kbo_org(checked_team_id, &team_league_id, &parent_team_id)) {
        if (offer_team_id == 0u
                || offer_team_id == checked_team_id
                || !kbo_foreign_ai_offer_target_is_kbo_org(offer_team_id, &team_league_id, &parent_team_id)) {
            return 0;
        }
        checked_team_id = offer_team_id;
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        today = 0u;
    }

    uint32_t holder_team_id = 0u;
    int32_t score = 0;
    int index = 0;
    int asian_quota = 0;
    int32_t salary_floor = kbo_foreign_contract_salary_floor_for_player(
        player,
        today,
        &holder_team_id,
        &score,
        &index,
        &asian_quota);
    if (out_salary_floor != NULL) { *out_salary_floor = salary_floor; }

    uint8_t old_major = kbo_offer_read_u8(offer_ptr, KBO_OFFER_MAJOR_FLAG_OFFSET);
    uint8_t old_minor = kbo_offer_read_u8(offer_ptr, KBO_OFFER_MINOR_FLAG_OFFSET);
    int32_t old_salary_primary = kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET);
    int32_t old_salary_first = kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET);
    int changed = 0;

    if (old_major != 1u && kbo_offer_write_u8(offer_ptr, KBO_OFFER_MAJOR_FLAG_OFFSET, 1u)) {
        changed++;
    }
    if (old_minor != 0u && kbo_offer_write_u8(offer_ptr, KBO_OFFER_MINOR_FLAG_OFFSET, 0u)) {
        changed++;
    }
    if (salary_floor > 0) {
        if (old_salary_primary <= 0 || old_salary_primary < salary_floor) {
            if (kbo_offer_write_i32(offer_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET, salary_floor)) {
                changed++;
            }
        }
        if (old_salary_first <= 0 || old_salary_first < salary_floor) {
            if (kbo_offer_write_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET, salary_floor)) {
                changed++;
            }
        }
    }

    if (changed == 0) {
        return 0;
    }

    static volatile LONG force_log_count = 0;
    LONG slot = InterlockedIncrement(&force_log_count);
    if (slot <= 300) {
        kbo_log_runtimef(
            "foreign ai offer major terms forced source=%s player=%u team_arg=%d checked_team=%u offer_team=%u team_league=%u parent_team=%u offer=%p old_major=%u new_major=%u old_minor=%u new_minor=%u old_salary24=%d new_salary24=%d old_salary38=%d new_salary38=%d floor=%d holder_team=%u score=%d index=%d asian_quota=%d today=%u",
            source != NULL ? source : "",
            *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
            team_id_arg,
            checked_team_id,
            offer_team_id,
            team_league_id,
            parent_team_id,
            (void*)offer_ptr,
            (uint32_t)old_major,
            (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_MAJOR_FLAG_OFFSET),
            (uint32_t)old_minor,
            (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_MINOR_FLAG_OFFSET),
            old_salary_primary,
            kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET),
            old_salary_first,
            kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET),
            salary_floor,
            holder_team_id,
            score,
            index,
            asian_quota,
            today);
    }
    return changed;
}

static void kbo_log_foreign_ai_offer_attach(
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

static void kbo_log_foreign_ai_offer_build(
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

static void kbo_log_foreign_ai_offer_final_gate(
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

__declspec(noinline) void ootp_kbo_foreign_ai_offer_attach_probe_wrapper(
    uintptr_t player_ptr,
    uintptr_t offer_slot_ptr,
    uintptr_t caller_return_ptr,
    uintptr_t original_func_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KboOotpForeignAiOfferAttachFn original_func = (KboOotpForeignAiOfferAttachFn)original_func_ptr;
    if (original_func != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        original_func(player_ptr, offer_slot_ptr);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }
    if (read_kbo_localappdata_flag_file("enable_foreign_ai_roster_research_hooks.txt")
            || read_kbo_localappdata_flag_file("enable_kbo_foreign_ai_offer_attach_probe.txt")) {
        kbo_log_foreign_ai_offer_attach(player_ptr, offer_slot_ptr, caller_return_ptr);
    }
    KBO_HOOK_PROFILE_END(profile_hook, "foreign.ai_offer_attach");
}

__declspec(noinline) uintptr_t ootp_kbo_foreign_ai_offer_build_probe_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    uintptr_t zero_arg,
    uintptr_t flag_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KboOotpForeignAiOfferBuildFn original_func =
        (KboOotpForeignAiOfferBuildFn)kbo_offer_probe_resolve_rva(OOTP27_AI_FA_OFFER_BUILD_FUNC_RVA);

    uintptr_t offer_ptr = 0;
    if (original_func != NULL) {
        kbo_apply_foreign_contract_demand_floor(player_ptr, 0u, "ai_offer_build");
        kbo_prepare_foreign_fa_offer_demand_baseline(player_ptr, "ai_offer_build");
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        offer_ptr = original_func(player_ptr, team_id, zero_arg, flag_ptr, 0u);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        kbo_force_foreign_ai_offer_major_terms(
            player_ptr,
            team_id,
            offer_ptr,
            "ai_offer_build",
            NULL);
        kbo_restore_foreign_fa_demand_salary_ladder("ai_offer_build");
    }
    if (read_kbo_localappdata_flag_file("enable_foreign_ai_roster_research_hooks.txt")
            || read_kbo_localappdata_flag_file("enable_kbo_foreign_ai_offer_attach_probe.txt")) {
        kbo_log_foreign_ai_offer_build(player_ptr, team_id, flag_ptr, offer_ptr);
    }
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_build", offer_ptr);
}

__declspec(noinline) uint8_t ootp_kbo_foreign_ai_offer_final_gate_probe_wrapper(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary,
    uintptr_t offer_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KboOotpForeignAiOfferFinalGateFn original_func =
        (KboOotpForeignAiOfferFinalGateFn)kbo_offer_probe_resolve_rva(OOTP27_AI_FA_OFFER_FINAL_GATE_FUNC_RVA);

    uint8_t result = 0u;
    int32_t adjusted_salary = salary;
    if (original_func != NULL) {
        int32_t salary_floor = 0;
        kbo_force_foreign_ai_offer_major_terms(
            player_ptr,
            (int32_t)kbo_offer_probe_team_id_from_ptr(team_ptr),
            offer_ptr,
            "ai_offer_final_gate",
            &salary_floor);
        if (salary_floor > 0 && adjusted_salary < salary_floor) {
            adjusted_salary = salary_floor;
        }
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        result = original_func(team_ptr, player_ptr, adjusted_salary);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }
    result = kbo_foreign_ai_fast_fill_offer_final_gate(team_ptr, player_ptr, adjusted_salary, offer_ptr, result);
    if (read_kbo_localappdata_flag_file("enable_foreign_ai_roster_research_hooks.txt")
            || read_kbo_localappdata_flag_file("enable_kbo_foreign_ai_offer_attach_probe.txt")) {
        kbo_log_foreign_ai_offer_final_gate(team_ptr, player_ptr, adjusted_salary, offer_ptr, result);
    }
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_final_gate", result);
}
