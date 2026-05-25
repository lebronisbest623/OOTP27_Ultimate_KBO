#ifndef KBO_CORE_LOGGING_EVENT_LOG_EVENT_H_
#define KBO_CORE_LOGGING_EVENT_LOG_EVENT_H_

#include <stddef.h>
#include <stdint.h>

#define KBO_LOG_FIELDS_BYTES 2048u

#define KBO_LOG_SINK_GLOBAL 0x01u
#define KBO_LOG_SINK_SAVE_SCOPED 0x02u

#define KBO_LOG_RUNTIME_MAX_BYTES (10u * 1024u * 1024u)
#define KBO_LOG_RULE_AUDIT_MAX_BYTES (50u * 1024u * 1024u)
#define KBO_LOG_DEFAULT_ARCHIVES 5

typedef enum KboLogLevel {
    KBO_LOG_LEVEL_DEBUG = 0,
    KBO_LOG_LEVEL_INFO = 1,
    KBO_LOG_LEVEL_WARN = 2,
    KBO_LOG_LEVEL_ERROR = 3
} KboLogLevel;

typedef struct KboLogFields {
    char json[KBO_LOG_FIELDS_BYTES];
    size_t used;
    int count;
    int truncated;
} KboLogFields;

const char* kbo_log_level_text(KboLogLevel level);

void kbo_log_fields_init(KboLogFields* fields);
void kbo_log_fields_merge(KboLogFields* fields, const KboLogFields* other);
void kbo_log_field_str(KboLogFields* fields, const char* name, const char* value);
void kbo_log_field_bool(KboLogFields* fields, const char* name, int value);
void kbo_log_field_u32(KboLogFields* fields, const char* name, uint32_t value);
void kbo_log_field_i32(KboLogFields* fields, const char* name, int32_t value);
void kbo_log_field_u64(KboLogFields* fields, const char* name, uint64_t value);
void kbo_log_field_i64(KboLogFields* fields, const char* name, int64_t value);
void kbo_log_field_ptr(KboLogFields* fields, const char* name, const void* value);

int kbo_log_event_emit(
    const char* file_name,
    const char* channel,
    KboLogLevel level,
    const char* domain,
    const char* event,
    const char* decision,
    const char* reason,
    const char* source,
    const KboLogFields* fields,
    unsigned sink_flags,
    size_t max_bytes,
    int archive_count);

void kbo_log_runtime_event(
    KboLogLevel level,
    const char* domain,
    const char* event,
    const char* decision,
    const char* reason,
    const char* source,
    const KboLogFields* fields);

void kbo_log_runtime_message(
    KboLogLevel level,
    const char* domain,
    const char* event,
    const char* message);

void kbo_log_event_flush(void);
void kbo_log_event_shutdown(void);

#endif
