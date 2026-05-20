#ifndef KBOFIX_RUNTIME_MEMORY_H
#define KBOFIX_RUNTIME_MEMORY_H

#include <stdint.h>
#include <windows.h>

#define KBO_RUNTIME_MIN_USER_POINTER ((uintptr_t)0x10000u)
#define KBO_RUNTIME_MAX_USER_POINTER ((uintptr_t)0x7FFFFFFFFFFFFFFFllu)
#define KBO_RUNTIME_USER_SCAN_END ((uintptr_t)0x0000800000000000ull)
#define KBO_RUNTIME_FAST_LEAGUE_SCAN_BYTES ((SIZE_T)0x00040000u)
#define KBO_RUNTIME_BOUNDED_SCAN_MAX_BYTES ((SIZE_T)0x00400000u)

int memory_range_readable(const void* address, SIZE_T size);
uintptr_t get_ootp_global_database(void);
uintptr_t get_ootp_cached_global_database(void);
uint8_t* find_ootp_executable_pattern(const uint8_t* pattern, size_t pattern_len);
int is_kbo_historical_league_context(uintptr_t league_ptr);

#endif
