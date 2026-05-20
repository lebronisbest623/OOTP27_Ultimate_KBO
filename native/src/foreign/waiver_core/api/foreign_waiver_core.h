#ifndef KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_CORE_H_
#define KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_CORE_H_

#include <stddef.h>
#include <stdint.h>

int kbo_read_foreign_waiver_window(uint32_t* out_start, uint32_t* out_end);
int kbo_is_foreign_waiver_negotiation_window_open(void);
int kbo_get_foreign_waiver_window_status_text(char* out, size_t out_size);
int kbo_current_foreign_waiver_window_dates(uint32_t* out_start, uint32_t* out_end);
uint32_t kbo_get_foreign_waiver_decision_team_id(uint8_t* player);
int kbo_foreign_waiver_latest_decision_action(
    uint32_t window_end,
    uint32_t team_id,
    uint32_t player_id,
    char* out_action,
    size_t out_action_size);
int kbo_append_foreign_waiver_user_decision(uint32_t team_id, uint32_t player_id, int retain);
int kbo_get_foreign_waiver_command_path(char* out, size_t out_size);
int kbo_foreign_waiver_command_file_ready(void);
int kbo_resolve_foreign_waiver_top_candidate_for_team(
    uint32_t team_id,
    uint32_t* out_player_id,
    uint32_t* out_current_team_id);
void process_foreign_waiver_commands(void);
void start_kbo_foreign_waiver_scanner_thread(void);

#endif
