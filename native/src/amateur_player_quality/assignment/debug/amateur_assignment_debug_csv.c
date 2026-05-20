#include "../../internal/amateur_assignment_internal.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"

void kbo_amateur_assignment_append_debug_csv(
    const char* phase,
    const char* source,
    uint32_t player_id,
    uint32_t league_id,
    int16_t age,
    int32_t quality_score,
    int player_tier,
    uint32_t from_team_id,
    uint8_t from_reputation,
    int from_tier,
    int32_t from_player_count,
    uint32_t to_team_id,
    uint8_t to_reputation,
    int to_tier,
    int32_t to_player_count,
    int32_t target_player_count,
    uint8_t target_reputation,
    int original_result,
    uint32_t after_team_id,
    uint32_t after_league_id)
{
    if (!read_kbo_localappdata_flag_file("enable_amateur_assignment_debug_csv.txt")) {
        return;
    }

    if (strcmp(phase != NULL ? phase : "", "original_success") == 0) {
        static volatile LONG success_csv_count = 0;
        if (InterlockedIncrement(&success_csv_count) > 300) {
            return;
        }
    } else {
        static volatile LONG reroute_csv_count = 0;
        if (InterlockedIncrement(&reroute_csv_count) > 5000) {
            return;
        }
    }
    char path[MAX_PATH] = {0};
    if (!kbo_get_save_scoped_data_file("amateur_assignment_debug.csv", path, sizeof(path))) {
        return;
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        static volatile LONG open_log_count = 0;
        if (InterlockedIncrement(&open_log_count) <= 5) {
            kbo_log_runtimef("amateur assignment debug csv open failed path=%s gle=%lu", path, GetLastError());
        }
        return;
    }

    if (kbo_amateur_assignment_debug_csv_empty(file)) {
        kbo_amateur_assignment_debug_write_text(
            file,
            "year,month,day,phase,source,player_id,league_id,age,quality_score,player_tier,"
            "from_team_id,from_reputation,from_tier,from_player_count,"
            "to_team_id,to_reputation,to_tier,to_player_count,target_player_count,target_reputation,"
            "original_result,after_team_id,after_league_id\r\n");
    }

    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    kbo_current_date_tick_latest_components(&year, &month, &day);

    char line[1024] = {0};
    snprintf(
        line,
        sizeof(line),
        "%u,%u,%u,",
        year,
        month,
        day);
    kbo_amateur_assignment_debug_write_text(file, line);
    kbo_amateur_assignment_debug_write_csv_text(file, phase != NULL ? phase : "");
    kbo_amateur_assignment_debug_write_text(file, ",");
    kbo_amateur_assignment_debug_write_csv_text(file, source != NULL ? source : "");
    snprintf(
        line,
        sizeof(line),
        ",%u,%u,%d,%d,%d,%u,%u,%d,%d,%u,%u,%d,%d,%d,%u,%d,%u,%u\r\n",
        player_id,
        league_id,
        (int)age,
        quality_score,
        player_tier,
        from_team_id,
        (uint32_t)from_reputation,
        from_tier,
        from_player_count,
        to_team_id,
        (uint32_t)to_reputation,
        to_tier,
        to_player_count,
        target_player_count,
        (uint32_t)target_reputation,
        original_result,
        after_team_id,
        after_league_id);
    kbo_amateur_assignment_debug_write_text(file, line);
    CloseHandle(file);
}
