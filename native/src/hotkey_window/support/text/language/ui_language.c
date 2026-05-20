#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/files/save_paths/platform/core_path_io.h"
#include "../../../../core/news/templates/core_news_templates.h"
#include "../../../../core/sync/lock.h"
#include "ui_language.h"

static int g_kbo_hub_language = KBO_HUB_LANG_KO;

#define KBO_HUB_TEXT_RING_COUNT 48
#define KBO_HUB_TEXT_VALUE_MAX 2048
#define KBO_HUB_TEXT_CACHE_COUNT 256
#define KBO_HUB_TEXT_CACHE_KEY_MAX 256

typedef struct KboHubTextCacheEntry {
    int language;
    char key[KBO_HUB_TEXT_CACHE_KEY_MAX];
    char value[KBO_HUB_TEXT_VALUE_MAX];
} KboHubTextCacheEntry;

static char g_kbo_hub_text_ring[KBO_HUB_TEXT_RING_COUNT][KBO_HUB_TEXT_VALUE_MAX];
static volatile LONG g_kbo_hub_text_ring_index = 0;
static KboLock g_kbo_hub_text_cache_lock = KBO_LOCK_INIT;
static KboHubTextCacheEntry g_kbo_hub_text_cache[KBO_HUB_TEXT_CACHE_COUNT];
static int g_kbo_hub_text_cache_count = 0;

int kbo_hub_language(void)
{
    return g_kbo_hub_language;
}

void kbo_hub_set_language(int language)
{
    g_kbo_hub_language = language == KBO_HUB_LANG_EN ? KBO_HUB_LANG_EN : KBO_HUB_LANG_KO;
}

static const char* kbo_hub_language_dir(void)
{
    return g_kbo_hub_language == KBO_HUB_LANG_EN ? "en" : "ko";
}

static char* kbo_hub_text_ring_slot(void)
{
    LONG index = InterlockedIncrement(&g_kbo_hub_text_ring_index);
    char* slot = g_kbo_hub_text_ring[(uint32_t)index % KBO_HUB_TEXT_RING_COUNT];
    slot[0] = '\0';
    return slot;
}

static int kbo_hub_text_cache_get(int language, const char* key, char* out, size_t out_size)
{
    if (key == NULL || out == NULL || out_size == 0u || strlen(key) >= KBO_HUB_TEXT_CACHE_KEY_MAX) {
        return 0;
    }

    kbo_lock_enter(&g_kbo_hub_text_cache_lock);
    for (int i = 0; i < g_kbo_hub_text_cache_count; i++) {
        KboHubTextCacheEntry* entry = &g_kbo_hub_text_cache[i];
        if (entry->language == language && strcmp(entry->key, key) == 0) {
            snprintf(out, out_size, "%s", entry->value);
            kbo_lock_leave(&g_kbo_hub_text_cache_lock);
            return 1;
        }
    }
    kbo_lock_leave(&g_kbo_hub_text_cache_lock);
    return 0;
}

static void kbo_hub_text_cache_put(int language, const char* key, const char* value)
{
    if (key == NULL || value == NULL || strlen(key) >= KBO_HUB_TEXT_CACHE_KEY_MAX) {
        return;
    }

    kbo_lock_enter(&g_kbo_hub_text_cache_lock);
    if (g_kbo_hub_text_cache_count < KBO_HUB_TEXT_CACHE_COUNT) {
        KboHubTextCacheEntry* entry = &g_kbo_hub_text_cache[g_kbo_hub_text_cache_count++];
        entry->language = language;
        snprintf(entry->key, sizeof(entry->key), "%s", key);
        snprintf(entry->value, sizeof(entry->value), "%s", value);
    }
    kbo_lock_leave(&g_kbo_hub_text_cache_lock);
}

const char* kbo_hub_text_key(const char* key, const char* ko, const char* en)
{
    const char* fallback = g_kbo_hub_language == KBO_HUB_LANG_EN ? en : ko;
    if (fallback == NULL) {
        fallback = "";
    }
    if (key == NULL || key[0] == '\0') {
        return fallback;
    }

    char* slot = kbo_hub_text_ring_slot();
    int language = g_kbo_hub_language;
    if (kbo_hub_text_cache_get(language, key, slot, KBO_HUB_TEXT_VALUE_MAX)) {
        return slot;
    }
    if (kbo_text_resource_load_for_language(
            "ui_text",
            kbo_hub_language_dir(),
            "hotkey_window.json",
            key,
            slot,
            KBO_HUB_TEXT_VALUE_MAX,
            NULL,
            0u,
            "hotkey_ui")) {
        kbo_hub_text_cache_put(language, key, slot);
        return slot;
    }

    snprintf(slot, KBO_HUB_TEXT_VALUE_MAX, "%s", fallback);
    return slot;
}

const char* kbo_hub_text(const char* ko, const char* en)
{
    return kbo_hub_text_key(en, ko, en);
}

static void kbo_hub_language_file_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    kbo_get_global_data_file("hub_language.txt", out, out_size);
}

void kbo_hub_load_language_setting(void)
{
    g_kbo_hub_language = KBO_HUB_LANG_KO;

    char path[MAX_PATH] = {0};
    kbo_hub_language_file_path(path, sizeof(path));
    if (path[0] == '\0') {
        return;
    }

    HANDLE file = kbo_create_file_read_utf8(path);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char buffer[16] = {0};
    DWORD read = 0;
    ReadFile(file, buffer, sizeof(buffer) - 1, &read, NULL);
    CloseHandle(file);

    if (ascii_equals_ignore_case(buffer, "en") || ascii_equals_ignore_case(buffer, "english")) {
        g_kbo_hub_language = KBO_HUB_LANG_EN;
    }
}

void kbo_hub_save_language_setting(void)
{
    char path[MAX_PATH] = {0};
    kbo_hub_language_file_path(path, sizeof(path));
    if (path[0] == '\0') {
        return;
    }

    HANDLE file = kbo_create_file_utf8(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    const char* value = g_kbo_hub_language == KBO_HUB_LANG_EN ? "en\n" : "ko\n";
    DWORD wrote = 0;
    WriteFile(file, value, (DWORD)strlen(value), &wrote, NULL);
    CloseHandle(file);
}
