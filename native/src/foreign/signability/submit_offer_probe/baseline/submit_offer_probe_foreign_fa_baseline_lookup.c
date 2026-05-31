#include "../submit_offer_probe.h"
#include "../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

uint8_t* kbo_resolve_current_league_financials(uint32_t* out_league_id)
{
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }

    int32_t league_id = kbo_no_minor_resolve_current_league_id();
    if (league_id <= 0) {
        return NULL;
    }

    uintptr_t global_db = get_ootp_global_database();
    if (global_db == 0) {
        return NULL;
    }
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    OotpLeagueFinancialsLookupFn lookup =
        (OotpLeagueFinancialsLookupFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA);
    if (!memory_range_readable((void*)lookup, 16)) {
        return NULL;
    }

    uint8_t* financials = lookup((void*)global_db, league_id);
    if (financials == NULL
            || !memory_range_readable(financials + OOTP27_FINANCIALS_PROBE_LAST_OFFSET, sizeof(int32_t))) {
        return NULL;
    }

    if (out_league_id != NULL) {
        *out_league_id = (uint32_t)league_id;
    }
    return financials;
}

uint8_t* kbo_resolve_team_key_league_financials(int32_t team_key, uint32_t* out_league_id)
{
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (team_key <= 0) {
        return NULL;
    }

    uintptr_t global_db = get_ootp_global_database();
    if (global_db == 0
            || !memory_range_readable((void*)(global_db + OOTP27_KBO_TEAM_COUNT_OFFSET), sizeof(int32_t))
            || !memory_range_readable((void*)(global_db + OOTP27_KBO_TEAM_VECTOR_OFFSET), sizeof(uintptr_t))) {
        return NULL;
    }

    int32_t team_count = *(int32_t*)(global_db + OOTP27_KBO_TEAM_COUNT_OFFSET);
    int32_t team_index = team_key - 1;
    if (team_count <= 0 || team_count > 100000 || team_index < 0 || team_index >= team_count) {
        return NULL;
    }

    uintptr_t team_vector = *(uintptr_t*)(global_db + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    uintptr_t team_slot = team_vector + ((uintptr_t)team_index * sizeof(uintptr_t));
    if (team_vector == 0 || !memory_range_readable((void*)team_slot, sizeof(uintptr_t))) {
        return NULL;
    }

    uint8_t* team = *(uint8_t**)team_slot;
    if (team == NULL
            || !memory_range_readable(
                team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET,
                sizeof(int32_t))) {
        return NULL;
    }

    int32_t league_id = *(int32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (league_id <= 0) {
        return NULL;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    OotpLeagueFinancialsLookupFn lookup =
        (OotpLeagueFinancialsLookupFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA);
    if (!memory_range_readable((void*)lookup, 16)) {
        return NULL;
    }

    uint8_t* financials = lookup((void*)global_db, league_id);
    if (financials == NULL
            || !memory_range_readable(financials + OOTP27_FINANCIALS_PROBE_LAST_OFFSET, sizeof(int32_t))) {
        return NULL;
    }

    uint32_t effective_league_id = (uint32_t)league_id;
    if (memory_range_readable(
            financials + OOTP27_KBO_LEAGUE_FINANCIALS_REDIRECT_LEAGUE_ID_OFFSET,
            sizeof(int32_t))) {
        int32_t redirect_league_id =
            *(int32_t*)(financials + OOTP27_KBO_LEAGUE_FINANCIALS_REDIRECT_LEAGUE_ID_OFFSET);
        if (redirect_league_id > 0 && redirect_league_id != league_id) {
            uint8_t* redirected_financials = lookup((void*)global_db, redirect_league_id);
            if (redirected_financials == NULL
                    || !memory_range_readable(
                        redirected_financials + OOTP27_FINANCIALS_PROBE_LAST_OFFSET,
                        sizeof(int32_t))) {
                return NULL;
            }
            financials = redirected_financials;
            effective_league_id = (uint32_t)redirect_league_id;
        }
    }

    if (out_league_id != NULL) {
        *out_league_id = effective_league_id;
    }
    return financials;
}

void kbo_log_financials_salary_baseline_probe(const char* source)
{
#define KBO_FINANCIALS_PROBE_LOW_OFFSET(index) \
    (OOTP27_FINANCIALS_PROBE_FIRST_OFFSET + ((uint32_t)(index) * sizeof(int32_t)))
#define KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(index) \
    (OOTP27_FINANCIALS_PROBE_SECONDARY_FIRST_OFFSET + ((uint32_t)(index) * sizeof(int32_t)))

    static LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 5 && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FINANCIALS_SALARY_BASELINE_PROBE_LOG_FILE)) {
        return;
    }

    uint32_t league_id = 0u;
    uint8_t* financials = kbo_resolve_current_league_financials(&league_id);
    if (financials == NULL) {
        if (slot <= 20) {
            kbo_log_runtimef("KBO financials salary baseline probe skipped source=%s reason=no_financials", source != NULL ? source : "");
        }
        return;
    }

    kbo_log_runtimef(
        "KBO financials salary baseline probe source=%s league=%u financials=%p off278=%d off27c=%d off280=%d off284=%d off288=%d off28c=%d off290=%d off294=%d off298=%d off29c=%d off2a0=%d off2a4=%d off2a8=%d off2ac=%d off360=%d off364=%d off368=%d off36c=%d off370=%d off374=%d off378=%d off37c=%d off380=%d off384=%d off388=%d off38c=%d off390=%d",
        source != NULL ? source : "",
        league_id,
        (void*)financials,
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(0)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(1)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(2)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(3)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(4)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(5)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(6)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(7)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(8)),
        *(int32_t*)(financials + OOTP27_FINANCIALS_AVERAGE_SALARY_OFFSET),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(10)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(11)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(12)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(13)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(0)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(1)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(2)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(3)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(4)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(5)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(6)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(7)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(8)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(9)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(10)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(11)),
        *(int32_t*)(financials + OOTP27_FINANCIALS_PROBE_LAST_OFFSET));

#undef KBO_FINANCIALS_PROBE_LOW_OFFSET
#undef KBO_FINANCIALS_PROBE_SECONDARY_OFFSET
}

