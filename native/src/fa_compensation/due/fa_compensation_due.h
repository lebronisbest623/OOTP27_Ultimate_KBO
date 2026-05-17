#ifndef KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_DUE_H_
#define KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_DUE_H_

#include <stdint.h>

int kbo_manual_submit_fa_compensation_protected_list(uint32_t fa_player_id, const char* source);
int kbo_process_due_fa_compensation_protected_lists_for_date(uint32_t today, const char* source);
int kbo_process_due_fa_compensation_protected_lists(const char* source);

#endif
