#include "../submit_offer_probe.h"
#include "../../../common/policy/foreign_player_policy.h"

int kbo_foreign_fa_demand_remap_already_applied(uint32_t player_id, int32_t demand)
{
    if (player_id == 0u || demand <= 0) {
        return 0;
    }
    for (int i = 0; i < (int)(sizeof(g_kbo_foreign_fa_demand_remap_records) / sizeof(g_kbo_foreign_fa_demand_remap_records[0])); i++) {
        if (g_kbo_foreign_fa_demand_remap_records[i].player_id == player_id
                && g_kbo_foreign_fa_demand_remap_records[i].mapped_demand == demand) {
            return 1;
        }
    }
    return 0;
}

void kbo_foreign_fa_demand_remap_remember(uint32_t player_id, int32_t original_demand, int32_t mapped_demand)
{
    if (player_id == 0u || original_demand <= 0 || mapped_demand <= 0) {
        return;
    }
    for (int i = 0; i < (int)(sizeof(g_kbo_foreign_fa_demand_remap_records) / sizeof(g_kbo_foreign_fa_demand_remap_records[0])); i++) {
        if (g_kbo_foreign_fa_demand_remap_records[i].player_id == player_id) {
            g_kbo_foreign_fa_demand_remap_records[i].original_demand = original_demand;
            g_kbo_foreign_fa_demand_remap_records[i].mapped_demand = mapped_demand;
            return;
        }
    }

    LONG slot = InterlockedIncrement((volatile LONG*)&g_kbo_foreign_fa_demand_remap_record_cursor);
    int index = (int)((slot - 1) % (LONG)(sizeof(g_kbo_foreign_fa_demand_remap_records) / sizeof(g_kbo_foreign_fa_demand_remap_records[0])));
    g_kbo_foreign_fa_demand_remap_records[index].player_id = player_id;
    g_kbo_foreign_fa_demand_remap_records[index].original_demand = original_demand;
    g_kbo_foreign_fa_demand_remap_records[index].mapped_demand = mapped_demand;
}

int kbo_foreign_fa_demand_remap_candidate(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (!kbo_foreign_policy_player_id_plausible(player_id)
            || !kbo_foreign_policy_market_age_allowed(age)
            || current_team_id != 0u) {
        return 0;
    }
    if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
        return 0;
    }
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }
    if (!memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))) {
        return 0;
    }
    int32_t demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    return kbo_foreign_policy_demand_salary_plausible(demand);
}

int32_t kbo_foreign_fa_remap_demand_from_salary_ladder(int32_t demand, uint8_t* financials, int asian_quota)
{
    if (demand <= 0 || financials == NULL) {
        return demand;
    }

    int32_t original[9];
    int32_t foreign_values[9];
    for (int i = 0; i < 9; i++) {
        if (!memory_range_readable(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i], sizeof(int32_t))) {
            return demand;
        }
        original[i] = *(int32_t*)(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i]);
        foreign_values[i] = kbo_get_foreign_fa_demand_baseline_value_for_player(i, asian_quota);
    }

    if (demand <= original[0]) {
        return demand < foreign_values[0] ? demand : foreign_values[0];
    }

    for (int i = 1; i < 9; i++) {
        if (original[i] <= original[i - 1]) {
            continue;
        }
        if (demand <= original[i]) {
            int64_t src_span = (int64_t)original[i] - (int64_t)original[i - 1];
            int64_t dst_span = (int64_t)foreign_values[i] - (int64_t)foreign_values[i - 1];
            int64_t src_delta = (int64_t)demand - (int64_t)original[i - 1];
            int64_t mapped = (int64_t)foreign_values[i - 1] + ((src_delta * dst_span) / src_span);
            if (mapped < foreign_values[0]) {
                mapped = foreign_values[0];
            }
            if (mapped > INT32_MAX) {
                mapped = INT32_MAX;
            }
            return (int32_t)mapped;
        }
    }

    if (original[8] <= 0 || foreign_values[8] <= 0) {
        return demand;
    }
    int64_t mapped = ((int64_t)demand * (int64_t)foreign_values[8]) / (int64_t)original[8];
    if (mapped < foreign_values[8]) {
        mapped = foreign_values[8];
    }
    if (mapped > INT32_MAX) {
        mapped = INT32_MAX;
    }
    return (int32_t)mapped;
}

static int kbo_foreign_reserve_demand_index_for_score(int32_t score)
{
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    for (int index = KBO_FOREIGN_POLICY_RESERVE_DEMAND_INDEX_COUNT - 1; index >= 1; index--) {
        if (score >= policy->reserve_demand_score_min[index]) {
            return index;
        }
    }
    return 0;
}

static int32_t kbo_foreign_reserve_demand_floor_from_index(int index, int asian_quota)
{
    int32_t base = kbo_get_foreign_fa_demand_baseline_value_for_player(index, asian_quota);
    int32_t floor = kbo_get_foreign_fa_demand_baseline_value_for_player(0, asian_quota);
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    int64_t discounted = ((int64_t)base * policy->reserve_demand_discount_percent) / 100;
    if (discounted < floor) {
        discounted = floor;
    }
    if (discounted > INT32_MAX) {
        discounted = INT32_MAX;
    }
    return (int32_t)discounted;
}

static int kbo_foreign_reserve_demand_asian_quota(uint8_t* player, int index)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_NATION_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }
    if (!kbo_nation_is_asian_quota_candidate(*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET))) {
        return 0;
    }

    int32_t asian_floor = kbo_foreign_reserve_demand_floor_from_index(index, 1);
    int32_t asian_limit = kbo_get_asian_quota_salary_limit();
    return asian_floor > 0 && asian_limit > 0 && asian_floor <= asian_limit;
}

int32_t kbo_foreign_reserve_demand_floor_for_player(
    uint8_t* player,
    uint32_t today,
    uint32_t* out_holder_team_id,
    int32_t* out_score,
    int* out_index,
    int* out_asian_quota)
{
    if (out_holder_team_id != NULL) { *out_holder_team_id = 0u; }
    if (out_score != NULL) { *out_score = 0; }
    if (out_index != NULL) { *out_index = 0; }
    if (out_asian_quota != NULL) { *out_asian_quota = 0; }

    if (!kbo_foreign_waiver_ai_enabled()
            || player == NULL
            || today == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t holder_team_id = 0u;
    if (player_id == 0u
            || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || !kbo_player_is_foreign_for_kbo_rights(player)
            || !kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
            || holder_team_id == 0u) {
        return 0;
    }

    int32_t score = kbo_foreign_waiver_value_score(player);
    if (score < kbo_get_foreign_waiver_value_threshold_for_player(player)) {
        return 0;
    }

    int index = kbo_foreign_reserve_demand_index_for_score(score);
    int asian_quota = kbo_foreign_reserve_demand_asian_quota(player, index);
    int32_t floor = kbo_foreign_reserve_demand_floor_from_index(index, asian_quota);
    if (floor <= 0) {
        return 0;
    }

    if (out_holder_team_id != NULL) { *out_holder_team_id = holder_team_id; }
    if (out_score != NULL) { *out_score = score; }
    if (out_index != NULL) { *out_index = index; }
    if (out_asian_quota != NULL) { *out_asian_quota = asian_quota; }
    return floor;
}

int32_t kbo_foreign_contract_salary_floor_for_player(
    uint8_t* player,
    uint32_t today,
    uint32_t* out_holder_team_id,
    int32_t* out_score,
    int* out_index,
    int* out_asian_quota)
{
    if (out_holder_team_id != NULL) { *out_holder_team_id = 0u; }
    if (out_score != NULL) { *out_score = 0; }
    if (out_index != NULL) { *out_index = 0; }
    if (out_asian_quota != NULL) { *out_asian_quota = 0; }

    if (player == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }

    int asian_quota = kbo_player_is_asian_quota_candidate(player) ? 1 : 0;
    int32_t floor = kbo_get_foreign_fa_demand_baseline_value_for_player(0, asian_quota);
    if (out_asian_quota != NULL) { *out_asian_quota = asian_quota; }

    uint32_t holder_team_id = 0u;
    int32_t score = 0;
    int index = 0;
    int reserve_asian_quota = 0;
    int32_t reserve_floor = kbo_foreign_reserve_demand_floor_for_player(
        player,
        today,
        &holder_team_id,
        &score,
        &index,
        &reserve_asian_quota);
    if (reserve_floor > 0) {
        if (reserve_floor > floor) {
            floor = reserve_floor;
        }
        asian_quota = reserve_asian_quota;
        if (out_holder_team_id != NULL) { *out_holder_team_id = holder_team_id; }
        if (out_score != NULL) { *out_score = score; }
        if (out_index != NULL) { *out_index = index; }
        if (out_asian_quota != NULL) { *out_asian_quota = asian_quota; }
    }

    return floor;
}

int kbo_apply_foreign_contract_demand_floor(uintptr_t player_ptr, uint32_t today, const char* source)
{
    if (player_ptr == 0 || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))) {
        return 0;
    }

    if (today == 0u) {
        kbo_get_foreign_waiver_current_yyyymmdd(&today);
    }

    uint32_t holder_team_id = 0u;
    int32_t score = 0;
    int index = 0;
    int asian_quota = 0;
    int32_t demand_floor = kbo_foreign_contract_salary_floor_for_player(
        player,
        today,
        &holder_team_id,
        &score,
        &index,
        &asian_quota);
    if (demand_floor <= 0 || holder_team_id == 0u) {
        return 0;
    }

    int32_t old_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    if (old_demand >= demand_floor) {
        return 0;
    }

    if (!kbo_write_i32(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, demand_floor)) {
        return 0;
    }

    static LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 240) {
        kbo_log_runtimef(
            "KBO foreign contract demand floor applied: source=%s player=%u old_demand=%d floor=%d holder_team=%u score=%d index=%d asian_quota=%d today=%u",
            source != NULL ? source : "",
            *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
            old_demand,
            demand_floor,
            holder_team_id,
            score,
            index,
            asian_quota,
            today);
    }

    return 1;
}

int kbo_apply_foreign_reserve_demand_floor(uintptr_t player_ptr, const char* source)
{
    if (player_ptr == 0 || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today) || today == 0u) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t holder_team_id = 0u;
    int32_t score = 0;
    int index = 0;
    int asian_quota = 0;
    int32_t demand_floor = kbo_foreign_reserve_demand_floor_for_player(
        player,
        today,
        &holder_team_id,
        &score,
        &index,
        &asian_quota);
    if (demand_floor <= 0
            || !memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))) {
        return 0;
    }

    int32_t old_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    if (old_demand >= demand_floor) {
        return 0;
    }

    if (!kbo_write_i32(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, demand_floor)) {
        return 0;
    }

    static LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 240) {
        kbo_log_runtimef(
            "KBO foreign reserve demand floor applied: source=%s player=%u holder_team=%u old_demand=%d floor=%d score=%d index=%d asian_quota=%d today=%u",
            source != NULL ? source : "",
            *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
            holder_team_id,
            old_demand,
            demand_floor,
            score,
            index,
            asian_quota,
            today);
    }
    return 1;
}

