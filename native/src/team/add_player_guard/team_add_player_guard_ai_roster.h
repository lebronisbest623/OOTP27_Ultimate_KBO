#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_AI_ROSTER_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_AI_ROSTER_H_

void kbo_set_ai_roster_select_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_primary_apply_flow_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_apply_selection_trace_trampoline(void* trampoline);
int kbo_run_foreign_ai_roster_daily_callup(const char* source);
void kbo_mark_foreign_ai_roster_daily_callup_dirty(const char* reason);
int kbo_consume_foreign_ai_roster_daily_callup_dirty(const char* source);

#endif
