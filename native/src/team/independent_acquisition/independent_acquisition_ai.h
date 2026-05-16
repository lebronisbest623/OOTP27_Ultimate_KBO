#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_H_

#include <stdint.h>

int kbo_run_independent_team_acquisition_ai(const char* source);
int kbo_run_independent_team_acquisition_ai_for_date(uint32_t today, const char* source);

#endif
