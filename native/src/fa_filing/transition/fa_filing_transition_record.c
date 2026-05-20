#include "../fa_filing_internal.h"
#include "../../core/dates/constants/kbo_date_constants.h"

int kbo_record_fa_filing_transition(
    uintptr_t player_ptr,
    uint32_t player_id,
    uint32_t filing_date,
    uint32_t original_team_id,
    uint32_t league_id,
    uint32_t caller_rva,
    uint8_t notify,
    uint8_t contract_level,
    const char* source)
{
    if (player_id == 0u || filing_date == 0u || original_team_id == 0u || league_id == 0u) {
        return 0;
    }

    uint32_t season = filing_date / 10000u;
    if (season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    KboFaFilingRecord* rows = (KboFaFilingRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_FILING_MAX * sizeof(KboFaFilingRecord));
    if (rows == NULL) {
        kbo_log_runtime_line("KBO FA filing record skipped: allocation failed");
        return 0;
    }

    char path[MAX_PATH] = {0};
    kbo_fa_filing_enter_lock();
    int row_count = kbo_load_fa_filing_records_unlocked(rows, KBO_FA_FILING_MAX, path, sizeof(path));
    int slot = -1;
    for (int i = 0; i < row_count; i++) {
        if (rows[i].player_id == player_id && rows[i].season == season) {
            slot = i;
            break;
        }
    }
    if (slot < 0 && row_count < KBO_FA_FILING_MAX) {
        slot = row_count++;
    }
    if (slot < 0) {
        kbo_fa_filing_leave_lock();
        HeapFree(GetProcessHeap(), 0, rows);
        kbo_log_runtimef("KBO FA filing record skipped: ledger full player=%u season=%u", player_id, season);
        return 0;
    }

    KboFaFilingRecord* rec = &rows[slot];
    memset(rec, 0, sizeof(*rec));
    rec->player_id = player_id;
    rec->filing_date = filing_date;
    rec->season = season;
    rec->original_team_id = original_team_id;
    rec->league_id = league_id;
    rec->source_caller_rva = caller_rva;
    rec->notify = notify;
    rec->contract_level = contract_level;
    if (player_ptr != 0 && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        kbo_copy_player_display_name((uint8_t*)player_ptr, rec->player_name, sizeof(rec->player_name));
    }
    if (rec->player_name[0] == '\0' || strcmp(rec->player_name, "Unknown player") == 0) {
        snprintf(rec->player_name, sizeof(rec->player_name), "Player #%u", player_id);
    }
    snprintf(rec->source, sizeof(rec->source), "%s", source != NULL ? source : "fa_filing_transition");

    int wrote = kbo_write_fa_filing_records_unlocked(rows, row_count, path, sizeof(path));
    if (wrote) {
        InterlockedExchange(&g_kbo_fa_filing_cache_dirty, 1);
        kbo_fa_filing_negative_cache_clear();
    }
    kbo_fa_filing_leave_lock();
    HeapFree(GetProcessHeap(), 0, rows);

    if (wrote) {
        kbo_log_runtimef(
            "KBO FA filing recorded player=%u date=%u season=%u original_team=%u league=%u caller_rva=0x%x csv=%s",
            player_id,
            filing_date,
            season,
            original_team_id,
            league_id,
            caller_rva,
            path);
        return 1;
    }
    kbo_log_runtimef("KBO FA filing record write failed player=%u date=%u csv=%s", player_id, filing_date, path);
    return 0;
}

