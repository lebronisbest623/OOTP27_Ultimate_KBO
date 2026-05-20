#ifndef KBOFIX_SRC_PERF_SNAPSHOT_PERF_SNAPSHOT_FORMAT_H_
#define KBOFIX_SRC_PERF_SNAPSHOT_PERF_SNAPSHOT_FORMAT_H_

#include <stdint.h>

#define KBO_PERF_SNAPSHOT_SCHEMA 1u
#define KBO_PERF_SNAPSHOT_MAGIC_BYTES 8u
#define KBO_PERF_SNAPSHOT_PLAYER_FILE "perf_snapshot_players.bin"
#define KBO_PERF_SNAPSHOT_META_FILE "perf_snapshot_meta.json"
#define KBO_PERF_SNAPSHOT_PLAYER_PATH "cache\\perf_snapshot_players.bin"
#define KBO_PERF_SNAPSHOT_META_PATH "cache\\perf_snapshot_meta.json"

#pragma pack(push, 1)
typedef struct KboPerfSnapshotHeader {
    char magic[KBO_PERF_SNAPSHOT_MAGIC_BYTES];
    uint32_t schema;
    uint32_t header_bytes;
    uint32_t record_header_bytes;
    uint32_t player_scan_bytes;
    uint32_t source_player_count;
    uint32_t copied_player_count;
    uint32_t skipped_player_count;
    uint32_t vector_offset;
    uint32_t yyyymmdd;
    uint64_t created_filetime;
} KboPerfSnapshotHeader;

typedef struct KboPerfSnapshotPlayerRecordHeader {
    uint32_t source_index;
    uint32_t player_id;
    uint64_t source_ptr;
} KboPerfSnapshotPlayerRecordHeader;
#pragma pack(pop)

static const char KBO_PERF_SNAPSHOT_MAGIC[KBO_PERF_SNAPSHOT_MAGIC_BYTES] = {
    'K', 'B', 'O', 'P', 'S', 'N', 'P', '1'
};

#endif
