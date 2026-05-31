#include "log_event_internal.h"
#include "../../files/save_paths/core_save_paths.h"
#include "../../files/save_paths/platform/core_path_io.h"
#include "../../sync/spin_lock.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define KBO_LOG_RUNTIME_FILE_NAME "runtime.ndjson"
static KboSpinLock g_kbo_log_event_run_id_lock = KBO_SPIN_LOCK_INIT;
static volatile LONG g_kbo_log_event_sequence = 0;
static char g_kbo_log_event_run_id[64] = {0};

const char* kbo_log_level_text(KboLogLevel level)
{
    switch (level) {
    case KBO_LOG_LEVEL_DEBUG: return "DEBUG";
    case KBO_LOG_LEVEL_WARN: return "WARN";
    case KBO_LOG_LEVEL_ERROR: return "ERROR";
    case KBO_LOG_LEVEL_INFO:
    default: return "INFO";
    }
}

static void kbo_log_append_text(char* out, size_t out_size, size_t* pos, const char* text)
{
    if (out == NULL || out_size == 0u || pos == NULL || *pos >= out_size) {
        return;
    }
    int written = snprintf(out + *pos, out_size - *pos, "%s", text != NULL ? text : "");
    if (written < 0) {
        return;
    }
    *pos = (size_t)written >= out_size - *pos ? out_size - 1u : *pos + (size_t)written;
}

static void kbo_log_escape_json(char* out, size_t out_size, const char* value)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }

    size_t pos = 0u;
    for (const unsigned char* p = (const unsigned char*)value; *p != '\0' && pos + 1u < out_size; p++) {
        switch (*p) {
        case '"': kbo_log_append_text(out, out_size, &pos, "\\\""); break;
        case '\\': kbo_log_append_text(out, out_size, &pos, "\\\\"); break;
        case '\b': kbo_log_append_text(out, out_size, &pos, "\\b"); break;
        case '\f': kbo_log_append_text(out, out_size, &pos, "\\f"); break;
        case '\n': kbo_log_append_text(out, out_size, &pos, "\\n"); break;
        case '\r': kbo_log_append_text(out, out_size, &pos, "\\r"); break;
        case '\t': kbo_log_append_text(out, out_size, &pos, "\\t"); break;
        default:
            if (*p < 0x20u) {
                char escaped[8] = {0};
                snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned)*p);
                kbo_log_append_text(out, out_size, &pos, escaped);
            } else {
                out[pos++] = (char)*p;
                out[pos] = '\0';
            }
            break;
        }
    }
    out[out_size - 1u] = '\0';
}

void kbo_log_fields_init(KboLogFields* fields)
{
    if (fields == NULL) {
        return;
    }
    memset(fields, 0, sizeof(*fields));
}

static int kbo_log_fields_append(KboLogFields* fields, const char* text)
{
    if (fields == NULL || text == NULL || text[0] == '\0') {
        return 0;
    }
    size_t len = strlen(text);
    if (len >= sizeof(fields->json) - fields->used) {
        fields->truncated = 1;
        fields->json[sizeof(fields->json) - 1u] = '\0';
        fields->used = sizeof(fields->json) - 1u;
        return 0;
    }
    memcpy(fields->json + fields->used, text, len + 1u);
    fields->used += len;
    return 1;
}

static int kbo_log_fields_append_name(KboLogFields* fields, const char* name)
{
    if (fields == NULL || name == NULL || name[0] == '\0') {
        return 0;
    }
    if (fields->count > 0 && !kbo_log_fields_append(fields, ",")) {
        return 0;
    }
    char escaped[128] = {0};
    kbo_log_escape_json(escaped, sizeof(escaped), name);
    char prefix[160] = {0};
    snprintf(prefix, sizeof(prefix), "\"%s\":", escaped);
    return kbo_log_fields_append(fields, prefix);
}

static void kbo_log_field_raw_json(KboLogFields* fields, const char* raw_json_members)
{
    if (fields == NULL || raw_json_members == NULL || raw_json_members[0] == '\0') {
        return;
    }
    if (fields->count > 0 && !kbo_log_fields_append(fields, ",")) {
        return;
    }
    if (kbo_log_fields_append(fields, raw_json_members)) {
        fields->count++;
    }
}

void kbo_log_fields_merge(KboLogFields* fields, const KboLogFields* other)
{
    if (fields == NULL || other == NULL || other->json[0] == '\0') {
        return;
    }
    kbo_log_field_raw_json(fields, other->json);
    if (other->truncated) {
        fields->truncated = 1;
    }
}

void kbo_log_field_str(KboLogFields* fields, const char* name, const char* value)
{
    if (!kbo_log_fields_append_name(fields, name)) {
        return;
    }
    char escaped[512] = {0};
    kbo_log_escape_json(escaped, sizeof(escaped), value);
    char entry[560] = {0};
    snprintf(entry, sizeof(entry), "\"%s\"", escaped);
    if (kbo_log_fields_append(fields, entry)) {
        fields->count++;
    }
}

static void kbo_log_field_number(KboLogFields* fields, const char* name, const char* format, ...)
{
    if (!kbo_log_fields_append_name(fields, name)) {
        return;
    }
    char value[96] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(value, sizeof(value), format, args);
    va_end(args);
    if (kbo_log_fields_append(fields, value)) {
        fields->count++;
    }
}

void kbo_log_field_bool(KboLogFields* fields, const char* name, int value) { kbo_log_field_number(fields, name, "%s", value ? "true" : "false"); }
void kbo_log_field_u32(KboLogFields* fields, const char* name, uint32_t value) { kbo_log_field_number(fields, name, "%u", value); }
void kbo_log_field_i32(KboLogFields* fields, const char* name, int32_t value) { kbo_log_field_number(fields, name, "%d", value); }
void kbo_log_field_u64(KboLogFields* fields, const char* name, uint64_t value) { kbo_log_field_number(fields, name, "%llu", (unsigned long long)value); }
void kbo_log_field_i64(KboLogFields* fields, const char* name, int64_t value) { kbo_log_field_number(fields, name, "%lld", (long long)value); }
void kbo_log_field_ptr(KboLogFields* fields, const char* name, const void* value) { kbo_log_field_number(fields, name, "\"%p\"", value); }

static void kbo_log_timestamp(char* out, size_t out_size)
{
    SYSTEMTIME now;
    GetLocalTime(&now);
    snprintf(out, out_size, "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
        (unsigned)now.wYear, (unsigned)now.wMonth, (unsigned)now.wDay,
        (unsigned)now.wHour, (unsigned)now.wMinute, (unsigned)now.wSecond,
        (unsigned)now.wMilliseconds);
    out[out_size - 1u] = '\0';
}

static const char* kbo_log_run_id(void)
{
    if (g_kbo_log_event_run_id[0] != '\0') {
        return g_kbo_log_event_run_id;
    }
    kbo_spin_lock(&g_kbo_log_event_run_id_lock);
    if (g_kbo_log_event_run_id[0] == '\0') {
        SYSTEMTIME now;
        GetLocalTime(&now);
        snprintf(g_kbo_log_event_run_id, sizeof(g_kbo_log_event_run_id),
            "%04u%02u%02u%02u%02u%02u-%lu",
            (unsigned)now.wYear, (unsigned)now.wMonth, (unsigned)now.wDay,
            (unsigned)now.wHour, (unsigned)now.wMinute, (unsigned)now.wSecond,
            (unsigned long)GetCurrentProcessId());
    }
    kbo_spin_unlock(&g_kbo_log_event_run_id_lock);
    return g_kbo_log_event_run_id;
}

static void kbo_log_extract_save_id(const char* save_path, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (save_path == NULL || save_path[0] == '\0') {
        return;
    }
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", save_path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
    }
    const char* base = strrchr(dir, '\\');
    snprintf(out, out_size, "%s", base != NULL ? base + 1 : dir);
}

static void kbo_log_build_json(char* out, size_t out_size, const char* channel, KboLogLevel level,
    const char* domain, const char* event, const char* decision, const char* reason,
    const char* source, const char* save_id, const char* fields_json, int fields_truncated)
{
    char timestamp[64] = {0}, channel_json[64] = {0}, domain_json[192] = {0}, event_json[128] = {0};
    char decision_json[96] = {0}, reason_json[192] = {0}, source_json[192] = {0}, save_id_json[192] = {0};
    kbo_log_timestamp(timestamp, sizeof(timestamp));
    kbo_log_escape_json(channel_json, sizeof(channel_json), channel != NULL ? channel : "runtime");
    kbo_log_escape_json(domain_json, sizeof(domain_json), domain != NULL ? domain : "general");
    kbo_log_escape_json(event_json, sizeof(event_json), event != NULL ? event : "event");
    kbo_log_escape_json(decision_json, sizeof(decision_json), decision);
    kbo_log_escape_json(reason_json, sizeof(reason_json), reason);
    kbo_log_escape_json(source_json, sizeof(source_json), source);
    kbo_log_escape_json(save_id_json, sizeof(save_id_json), save_id);

    int len = snprintf(out, out_size,
        "{\"schema\":1,\"seq\":%ld,\"timestamp\":\"%s\",\"pid\":%lu,\"tid\":%lu,"
        "\"run_id\":\"%s\",\"channel\":\"%s\",\"level\":\"%s\",\"domain\":\"%s\",\"event\":\"%s\"",
        (long)InterlockedIncrement(&g_kbo_log_event_sequence), timestamp,
        (unsigned long)GetCurrentProcessId(), (unsigned long)GetCurrentThreadId(),
        kbo_log_run_id(), channel_json, kbo_log_level_text(level), domain_json, event_json);
    size_t used = len > 0 && (size_t)len < out_size ? (size_t)len : out_size - 1u;
    if (decision_json[0] != '\0' && used < out_size) { used += (size_t)snprintf(out + used, out_size - used, ",\"decision\":\"%s\"", decision_json); }
    if (reason_json[0] != '\0' && used < out_size) { used += (size_t)snprintf(out + used, out_size - used, ",\"reason\":\"%s\"", reason_json); }
    if (source_json[0] != '\0' && used < out_size) { used += (size_t)snprintf(out + used, out_size - used, ",\"source\":\"%s\"", source_json); }
    if (save_id_json[0] != '\0' && used < out_size) { used += (size_t)snprintf(out + used, out_size - used, ",\"save_id\":\"%s\"", save_id_json); }
    if (fields_json != NULL && fields_json[0] != '\0' && used < out_size) { used += (size_t)snprintf(out + used, out_size - used, ",%s", fields_json); }
    if (fields_truncated && used < out_size) { used += (size_t)snprintf(out + used, out_size - used, ",\"fields_truncated\":true"); }
    if (used >= out_size) {
        used = out_size - 1u;
    }
    snprintf(out + used, out_size - used, "}");
    out[out_size - 1u] = '\0';
}

static int kbo_log_event_emit_raw_internal(const char* file_name, const char* channel, KboLogLevel level,
    const char* domain, const char* event, const char* decision, const char* reason,
    const char* source, const char* fields_json, unsigned sink_flags, size_t max_bytes,
    int archive_count, int fields_truncated)
{
    char global_path[MAX_PATH] = {0}, save_path[MAX_PATH] = {0}, save_id[192] = {0};
    int has_global = (sink_flags & KBO_LOG_SINK_GLOBAL) && kbo_get_global_data_file(file_name, global_path, sizeof(global_path));
    int has_save = (sink_flags & KBO_LOG_SINK_SAVE_SCOPED) && kbo_get_save_scoped_data_file(file_name, save_path, sizeof(save_path));
    if (has_save) {
        kbo_log_extract_save_id(save_path, save_id, sizeof(save_id));
    }

    char json[4096] = {0};
    kbo_log_build_json(json, sizeof(json), channel, level, domain, event, decision, reason, source, save_id, fields_json, fields_truncated);

    return kbo_log_event_write_paths(
        has_global,
        global_path,
        has_save,
        save_path,
        json,
        level,
        max_bytes,
        archive_count);
}

int kbo_log_event_emit(const char* file_name, const char* channel, KboLogLevel level,
    const char* domain, const char* event, const char* decision, const char* reason,
    const char* source, const KboLogFields* fields, unsigned sink_flags, size_t max_bytes,
    int archive_count)
{
    return kbo_log_event_emit_raw_internal(file_name, channel, level, domain, event, decision, reason,
        source, fields != NULL ? fields->json : NULL, sink_flags, max_bytes, archive_count,
        fields != NULL ? fields->truncated : 0);
}

void kbo_log_runtime_event(KboLogLevel level, const char* domain, const char* event,
    const char* decision, const char* reason, const char* source, const KboLogFields* fields)
{
    kbo_log_event_emit(KBO_LOG_RUNTIME_FILE_NAME, "runtime", level, domain, event, decision, reason,
        source, fields, KBO_LOG_SINK_GLOBAL, KBO_LOG_RUNTIME_MAX_BYTES,
        KBO_LOG_DEFAULT_ARCHIVES);
}

void kbo_log_runtime_message(KboLogLevel level, const char* domain, const char* event, const char* message)
{
    KboLogFields fields;
    kbo_log_fields_init(&fields);
    kbo_log_field_str(&fields, "message", message != NULL ? message : "");
    kbo_log_runtime_event(level, domain, event, NULL, NULL, "runtime_message", &fields);
}
