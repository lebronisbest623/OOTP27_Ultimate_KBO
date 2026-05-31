#ifndef KBO_CORE_LOGGING_EVENT_LOG_EVENT_INTERNAL_H
#define KBO_CORE_LOGGING_EVENT_LOG_EVENT_INTERNAL_H

#include "log_event.h"

int kbo_log_event_write_paths(
    int has_global,
    const char* global_path,
    int has_save,
    const char* save_path,
    const char* json,
    KboLogLevel level,
    size_t max_bytes,
    int archive_count);

#endif
