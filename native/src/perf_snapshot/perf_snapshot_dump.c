#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../bootstrap/abi/ootp_offsets.h"
#include "../core/core_flags/api/flags_api.h"
#include "../core/dates/tick/current_date_tick_capture.h"
#include "../core/files/atomic/core_atomic_file.h"
#include "../core/files/save_paths/core_save_paths.h"
#include "../core/logging/core_log.h"
#include "../foreign/common/dates/foreign_waiver_date.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/lookup/team_lookup.h"
#include "perf_snapshot.h"
#include "perf_snapshot_format.h"

static uint64_t kbo_perf_snapshot_filetime_now(void)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ((uint64_t)ft.dwHighDateTime << 32) | (uint64_t)ft.dwLowDateTime;
}

static int kbo_perf_snapshot_write_all(HANDLE file, const void* data, DWORD size)
{
    if (file == INVALID_HANDLE_VALUE || data == NULL || size == 0u) {
        return 0;
    }
    DWORD written = 0;
    return WriteFile(file, data, size, &written, NULL) && written == size;
}

static int kbo_perf_snapshot_write_meta(
    const char* snapshot_path,
    const KboPerfSnapshotHeader* header,
    const char* source)
{
    if (snapshot_path == NULL || header == NULL) {
        return 0;
    }
    (void)snapshot_path;

    char path[MAX_PATH] = {0};
    if (!kbo_get_save_scoped_data_file(KBO_PERF_SNAPSHOT_META_FILE, path, sizeof(path))) {
        return 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char json[2048] = {0};
    int len = snprintf(
        json,
        sizeof(json),
        "{\r\n"
        "  \"schema\": %u,\r\n"
        "  \"player_file\": \"%s\",\r\n"
        "  \"source\": \"%s\",\r\n"
        "  \"source_player_count\": %u,\r\n"
        "  \"copied_player_count\": %u,\r\n"
        "  \"skipped_player_count\": %u,\r\n"
        "  \"player_scan_bytes\": %u,\r\n"
        "  \"record_header_bytes\": %u,\r\n"
        "  \"vector_offset\": %u,\r\n"
        "  \"yyyymmdd\": %u,\r\n"
        "  \"created_filetime\": %llu\r\n"
        "}\r\n",
        header->schema,
        KBO_PERF_SNAPSHOT_PLAYER_FILE,
        source != NULL ? source : "",
        header->source_player_count,
        header->copied_player_count,
        header->skipped_player_count,
        header->player_scan_bytes,
        header->record_header_bytes,
        header->vector_offset,
        header->yyyymmdd,
        (unsigned long long)header->created_filetime);
    if (len <= 0 || len >= (int)sizeof(json)
            || !kbo_perf_snapshot_write_all(file, json, (DWORD)len)) {
        kbo_atomic_abort(file, tmp_path);
        return 0;
    }
    return kbo_atomic_commit(file, tmp_path, path);
}

int kbo_dump_perf_player_snapshot(const char* source, char* out_path, size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0u) {
        out_path[0] = '\0';
    }
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "perf_snapshot_dump")) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0u;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)
            || player_vector == 0u || player_count <= 0 || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT) {
        kbo_log_runtimef(
            "perf snapshot skipped reason=no_player_vector source=%s vector=%p count=%d",
            source != NULL ? source : "",
            (void*)player_vector,
            player_count);
        return 0;
    }

    SIZE_T vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, vector_bytes)) {
        kbo_log_runtimef(
            "perf snapshot skipped reason=unreadable_vector source=%s vector=%p count=%d",
            source != NULL ? source : "",
            (void*)player_vector,
            player_count);
        return 0;
    }

    uintptr_t* snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, vector_bytes);
    if (snapshot == NULL) {
        kbo_log_runtimef("perf snapshot skipped reason=alloc_vector_failed count=%d", player_count);
        return 0;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            snapshot,
            vector_bytes,
            &bytes_read)
            || bytes_read != vector_bytes) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        kbo_log_runtimef("perf snapshot skipped reason=copy_vector_failed count=%d", player_count);
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_save_scoped_data_file(KBO_PERF_SNAPSHOT_PLAYER_FILE, path, sizeof(path))) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        kbo_log_runtime_line("perf snapshot skipped reason=path_unavailable");
        return 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        kbo_log_runtimef("perf snapshot skipped reason=open_failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    KboPerfSnapshotHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, KBO_PERF_SNAPSHOT_MAGIC, sizeof(header.magic));
    header.schema = KBO_PERF_SNAPSHOT_SCHEMA;
    header.header_bytes = (uint32_t)sizeof(header);
    header.record_header_bytes = (uint32_t)sizeof(KboPerfSnapshotPlayerRecordHeader);
    header.player_scan_bytes = OOTP27_PLAYER_SCAN_BYTES;
    header.source_player_count = (uint32_t)player_count;
    header.vector_offset = vector_offset;
    header.created_filetime = kbo_perf_snapshot_filetime_now();
    kbo_current_date_tick_latest_published_date(&header.yyyymmdd);

    int ok = kbo_perf_snapshot_write_all(file, &header, (DWORD)sizeof(header));
    uint8_t player_scan[OOTP27_PLAYER_SCAN_BYTES];
    for (int32_t i = 0; ok && i < player_count; i++) {
        uintptr_t player_ptr = snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)) {
            header.skipped_player_count++;
            continue;
        }
        SIZE_T player_bytes_read = 0;
        if (!ReadProcessMemory(
                GetCurrentProcess(),
                (LPCVOID)player_ptr,
                player_scan,
                sizeof(player_scan),
                &player_bytes_read)
                || player_bytes_read != sizeof(player_scan)) {
            header.skipped_player_count++;
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player_scan + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            header.skipped_player_count++;
            continue;
        }

        KboPerfSnapshotPlayerRecordHeader record;
        record.source_index = (uint32_t)i;
        record.player_id = player_id;
        record.source_ptr = (uint64_t)player_ptr;
        ok = kbo_perf_snapshot_write_all(file, &record, (DWORD)sizeof(record))
            && kbo_perf_snapshot_write_all(file, player_scan, (DWORD)sizeof(player_scan));
        if (ok) {
            header.copied_player_count++;
        }
    }

    HeapFree(GetProcessHeap(), 0, snapshot);
    if (!ok) {
        kbo_atomic_abort(file, tmp_path);
        kbo_log_runtimef("perf snapshot failed reason=write_failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    SetFilePointer(file, 0, NULL, FILE_BEGIN);
    if (!kbo_perf_snapshot_write_all(file, &header, (DWORD)sizeof(header))) {
        kbo_atomic_abort(file, tmp_path);
        kbo_log_runtimef("perf snapshot failed reason=rewrite_header_failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }
    if (!kbo_atomic_commit(file, tmp_path, path)) {
        kbo_log_runtimef("perf snapshot failed reason=commit_failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    kbo_perf_snapshot_write_meta(path, &header, source);
    if (out_path != NULL && out_path_size > 0u) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    kbo_log_runtimef(
        "perf snapshot written source=%s players=%u skipped=%u date=%u path=%s",
        source != NULL ? source : "",
        header.copied_player_count,
        header.skipped_player_count,
        header.yyyymmdd,
        path);
    return header.copied_player_count > 0u;
}
