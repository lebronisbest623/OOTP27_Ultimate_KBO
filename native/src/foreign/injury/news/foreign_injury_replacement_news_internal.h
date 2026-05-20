#ifndef KBOFIX_FOREIGN_INJURY_REPLACEMENT_NEWS_INTERNAL_H_
#define KBOFIX_FOREIGN_INJURY_REPLACEMENT_NEWS_INTERNAL_H_

#include "../internal/foreign_injury_internal.h"

int kbo_foreign_injury_news_uses_korean(void);
void kbo_foreign_injury_copy_team_link(uint32_t team_id, char* out, size_t out_size);

#endif
