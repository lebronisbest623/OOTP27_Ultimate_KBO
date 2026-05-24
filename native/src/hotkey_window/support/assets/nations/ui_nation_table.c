#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#define JSMN_STATIC
#include "../../../../../third_party/jsmn/jsmn.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "ui_nation_table.h"

#define KBO_NATION_TABLE_MAX      256
#define KBO_NATIONS_FILE          "kbo_nations.json"
#define KBO_NATIONS_JSON_MAX_BYTES (128u * 1024u)
#define KBO_NATIONS_JSON_MAX_TOKENS 4096

typedef struct {
    uint32_t id;
    char label[64];
    char abbrev[8];
    char flag_file[32];
} KboNationEntry;

static INIT_ONCE g_nation_once = INIT_ONCE_STATIC_INIT;
static KboNationEntry g_nations[KBO_NATION_TABLE_MAX];
static int g_nation_count = 0;

static int nation_subtree_end(const jsmntok_t* tokens, int parsed, int index)
{
    if (index < 0 || index >= parsed) {
        return index + 1;
    }
    int cursor = index + 1;
    int children = tokens[index].size;
    for (int i = 0; i < children && cursor < parsed; i++) {
        cursor = nation_subtree_end(tokens, parsed, cursor);
    }
    return cursor;
}

static int nation_key_eq(const char* json, const jsmntok_t* tok, const char* key)
{
    if (tok->type != JSMN_STRING || tok->start < 0 || tok->end < tok->start) {
        return 0;
    }
    size_t len = (size_t)(tok->end - tok->start);
    return strlen(key) == len && memcmp(json + tok->start, key, len) == 0;
}

static void nation_str_copy(const char* json, const jsmntok_t* tok, char* out, size_t out_size)
{
    if (tok->type != JSMN_STRING || tok->start < 0 || tok->end < tok->start || out == NULL || out_size == 0) {
        return;
    }
    size_t len = (size_t)(tok->end - tok->start);
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, json + tok->start, len);
    out[len] = '\0';
}

static int nation_uint32(const char* json, const jsmntok_t* tok, uint32_t* out)
{
    if (tok->type != JSMN_PRIMITIVE || tok->start < 0 || tok->end <= tok->start) {
        return 0;
    }
    const char* p = json + tok->start;
    size_t len = (size_t)(tok->end - tok->start);
    unsigned long long val = 0;
    for (size_t i = 0; i < len; i++) {
        if (p[i] < '0' || p[i] > '9') {
            return 0;
        }
        val = val * 10u + (unsigned long long)(unsigned char)(p[i] - '0');
        if (val > 0xFFFFFFFFULL) {
            return 0;
        }
    }
    *out = (uint32_t)val;
    return 1;
}

static void kbo_nation_table_parse(const char* json, DWORD json_size)
{
    jsmntok_t tokens[KBO_NATIONS_JSON_MAX_TOKENS];
    jsmn_parser parser;
    jsmn_init(&parser);
    int parsed = jsmn_parse(&parser, json, (size_t)json_size, tokens, KBO_NATIONS_JSON_MAX_TOKENS);
    if (parsed < 3 || tokens[0].type != JSMN_OBJECT) {
        return;
    }

    int array_idx = -1;
    int cursor = 1;
    for (int pair = 0; pair < tokens[0].size && cursor + 1 < parsed; pair++) {
        if (nation_key_eq(json, &tokens[cursor], "nations") && tokens[cursor + 1].type == JSMN_ARRAY) {
            array_idx = cursor + 1;
            break;
        }
        cursor = nation_subtree_end(tokens, parsed, cursor + 1);
    }
    if (array_idx < 0) {
        return;
    }

    cursor = array_idx + 1;
    for (int ni = 0; ni < tokens[array_idx].size && cursor < parsed && g_nation_count < KBO_NATION_TABLE_MAX; ni++) {
        if (tokens[cursor].type != JSMN_OBJECT) {
            cursor = nation_subtree_end(tokens, parsed, cursor);
            continue;
        }
        KboNationEntry entry;
        memset(&entry, 0, sizeof(entry));
        int inner = cursor + 1;
        for (int kv = 0; kv < tokens[cursor].size && inner + 1 < parsed; kv++) {
            int k = inner;
            int v = inner + 1;
            if (nation_key_eq(json, &tokens[k], "id")) {
                nation_uint32(json, &tokens[v], &entry.id);
            } else if (nation_key_eq(json, &tokens[k], "label")) {
                nation_str_copy(json, &tokens[v], entry.label, sizeof(entry.label));
            } else if (nation_key_eq(json, &tokens[k], "abbrev")) {
                nation_str_copy(json, &tokens[v], entry.abbrev, sizeof(entry.abbrev));
            } else if (nation_key_eq(json, &tokens[k], "flag_file")) {
                nation_str_copy(json, &tokens[v], entry.flag_file, sizeof(entry.flag_file));
            }
            inner = nation_subtree_end(tokens, parsed, v);
        }
        if (entry.id > 0u && entry.label[0] != '\0') {
            g_nations[g_nation_count++] = entry;
        }
        cursor = nation_subtree_end(tokens, parsed, cursor);
    }
}

static BOOL CALLBACK kbo_nation_table_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    char path[MAX_PATH] = {0};
    if (!kbo_get_global_data_file(KBO_NATIONS_FILE, path, sizeof(path))) {
        return TRUE;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return TRUE;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > KBO_NATIONS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return TRUE;
    }

    char* buf = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buf == NULL) {
        CloseHandle(file);
        return TRUE;
    }

    DWORD read = 0;
    if (ReadFile(file, buf, size, &read, NULL) && read > 0u) {
        kbo_nation_table_parse(buf, read);
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buf);
    return TRUE;
}

static const KboNationEntry* kbo_nation_find(uint32_t id)
{
    InitOnceExecuteOnce(&g_nation_once, kbo_nation_table_init_once, NULL, NULL);
    for (int i = 0; i < g_nation_count; i++) {
        if (g_nations[i].id == id) {
            return &g_nations[i];
        }
    }
    return NULL;
}

const char* kbo_nation_table_label(uint32_t nation_id)
{
    const KboNationEntry* e = kbo_nation_find(nation_id);
    return e != NULL ? e->label : "Unknown nation";
}

const char* kbo_nation_table_abbrev(uint32_t nation_id)
{
    const KboNationEntry* e = kbo_nation_find(nation_id);
    return e != NULL ? e->abbrev : "---";
}

const char* kbo_nation_table_flag_file(uint32_t nation_id)
{
    const KboNationEntry* e = kbo_nation_find(nation_id);
    return e != NULL ? e->flag_file : "unknown.png";
}
