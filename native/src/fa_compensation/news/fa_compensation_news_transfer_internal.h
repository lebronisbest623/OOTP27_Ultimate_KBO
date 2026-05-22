#ifndef KBOFIX_SRC_FA_COMPENSATION_NEWS_FA_COMPENSATION_NEWS_TRANSFER_INTERNAL_H_
#define KBOFIX_SRC_FA_COMPENSATION_NEWS_FA_COMPENSATION_NEWS_TRANSFER_INTERNAL_H_

#include <stdint.h>
#include <stddef.h>

void kbo_fa_compensation_format_salary_text(int32_t salary, char* out, size_t out_size);
void kbo_fa_compensation_copy_team_history_name(uint32_t team_id, char* out, size_t out_size);
void kbo_fa_compensation_copy_team_link(uint32_t team_id, char* out, size_t out_size);

#endif
