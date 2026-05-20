#ifndef KBOFIX_SRC_MILITARY_SERVICE_RUNTIME_DAYS_TICK_MILITARY_SERVICE_DAYS_TICK_H_
#define KBOFIX_SRC_MILITARY_SERVICE_RUNTIME_DAYS_TICK_MILITARY_SERVICE_DAYS_TICK_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

DWORD WINAPI kbo_military_days_tick_thread(LPVOID parameter);
DWORD WINAPI kbo_military_seed_bootstrap_thread(LPVOID parameter);
void kbo_military_prewarm_save_scoped_bootstrap_files(const char* save_path);

#endif
