#ifndef KBO_BOOTSTRAP_PROFILING_OUTPUT_PROFILER_OUTPUT_H_
#define KBO_BOOTSTRAP_PROFILING_OUTPUT_PROFILER_OUTPUT_H_

#include <windows.h>

void kbo_profiler_write_bytes(const char* data, DWORD size);
void kbo_profiler_write_header_if_needed(void);

#endif
