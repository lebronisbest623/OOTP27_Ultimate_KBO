#include "core_news_object.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/abi/ootp_typedefs.h"
#include "../../../build_verify/build_verify.h"
#include "../../logging/core_log.h"
#include "../../files/message_body/core_message_body_file.h"
#include "../links/core_news_links.h"
#include "../../dates/core_current_date.h"
#include "../../files/save_paths/core_save_paths.h"
#include "../../dates/core_text_date.h"
#include "../../core_flags/api/flags_api.h"
#include "../../text/ootp_text_encoding.h"
#include "../../../runtime_memory/runtime_memory.h"

/* Core native news object construction helpers. */

static int kbo_write_file_u32_at(HANDLE file, DWORD offset, uint32_t value)
{
    DWORD written = 0u;
    if (SetFilePointer(file, (LONG)offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER
            && GetLastError() != NO_ERROR) {
        return 0;
    }
    return WriteFile(file, &value, sizeof(value), &written, NULL) && written == sizeof(value);
}

static uint32_t kbo_read_unaligned_u32(const uint8_t* data)
{
    uint32_t value = 0u;
    if (data != NULL) {
        memcpy(&value, data, sizeof(value));
    }
    return value;
}

typedef struct KboMessagePrimaryPatchRequest {
    uint32_t message_id;
    uint32_t primary_player_id;
    uint32_t primary_team_id;
    char source[96];
} KboMessagePrimaryPatchRequest;

static int kbo_patch_message_dat_primary_related(
    uint32_t message_id,
    uint32_t primary_player_id,
    uint32_t primary_team_id,
    const char* source,
    int log_not_found)
{
    if (message_id == 0u || (primary_player_id == 0u && primary_team_id == 0u)) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    char messages_path[MAX_PATH] = {0};
    snprintf(messages_path, sizeof(messages_path), "%s\\messages.dat", save_path);
    HANDLE file = CreateFileA(
        messages_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "league news message primary patch skipped source=%s id=%u reason=open_failed gle=%lu",
            source != NULL ? source : "",
            message_id,
            GetLastError());
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size < 128u || size > 32u * 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    uint8_t* data = (uint8_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    if (data == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    int ok = ReadFile(file, data, size, &read, NULL) && read == size;
    DWORD record_offset = 0u;
    int found = 0;
    if (ok) {
        for (DWORD i = 0u; i + 115u <= size; i++) {
            if (kbo_read_unaligned_u32(data + i) != message_id) {
                continue;
            }
            uint32_t year = kbo_read_unaligned_u32(data + i + 98u);
            if (year < 1800u || year > 2300u) {
                continue;
            }
            record_offset = i;
            found = 1;
            break;
        }
    }
    HeapFree(GetProcessHeap(), 0, data);

    if (!ok || !found) {
        CloseHandle(file);
        if (log_not_found) {
            kbo_log_runtimef(
                "league news message primary patch skipped source=%s id=%u reason=record_not_found",
                source != NULL ? source : "",
                message_id);
        }
        return 0;
    }

    int player_ok = primary_player_id == 0u
        || kbo_write_file_u32_at(file, record_offset + 4u, primary_player_id);
    int team_ok = primary_team_id == 0u
        || kbo_write_file_u32_at(file, record_offset + 44u, primary_team_id);
    CloseHandle(file);

    kbo_log_runtimef(
        "league news message primary patch source=%s id=%u player=%u team=%u offset=%lu player_ok=%d team_ok=%d",
        source != NULL ? source : "",
        message_id,
        primary_player_id,
        primary_team_id,
        (unsigned long)record_offset,
        player_ok,
        team_ok);
    return player_ok && team_ok;
}

static DWORD WINAPI kbo_message_primary_patch_retry_thread(LPVOID param)
{
    KboMessagePrimaryPatchRequest* request = (KboMessagePrimaryPatchRequest*)param;
    if (request == NULL) {
        return 0;
    }

    for (int attempt = 1; attempt <= 60; attempt++) {
        Sleep(500);
        if (kbo_patch_message_dat_primary_related(
                request->message_id,
                request->primary_player_id,
                request->primary_team_id,
                request->source,
                attempt == 60)) {
            if (attempt > 1) {
                kbo_log_runtimef(
                    "league news message primary patch delayed source=%s id=%u attempts=%d",
                    request->source,
                    request->message_id,
                    attempt);
            }
            HeapFree(GetProcessHeap(), 0, request);
            return 0;
        }
    }

    HeapFree(GetProcessHeap(), 0, request);
    return 0;
}

static void kbo_schedule_message_dat_primary_patch(
    uint32_t message_id,
    uint32_t primary_player_id,
    uint32_t primary_team_id,
    const char* source)
{
    if (kbo_patch_message_dat_primary_related(
            message_id,
            primary_player_id,
            primary_team_id,
            source,
            0)) {
        return;
    }

    KboMessagePrimaryPatchRequest* request = (KboMessagePrimaryPatchRequest*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(*request));
    if (request == NULL) {
        kbo_log_runtimef(
            "league news message primary patch skipped source=%s id=%u reason=retry_alloc_failed",
            source != NULL ? source : "",
            message_id);
        return;
    }

    request->message_id = message_id;
    request->primary_player_id = primary_player_id;
    request->primary_team_id = primary_team_id;
    snprintf(request->source, sizeof(request->source), "%s", source != NULL ? source : "");

    HANDLE thread = CreateThread(
        NULL,
        0,
        kbo_message_primary_patch_retry_thread,
        request,
        0,
        NULL);
    if (thread == NULL) {
        kbo_log_runtimef(
            "league news message primary patch skipped source=%s id=%u reason=retry_thread_failed gle=%lu",
            request->source,
            message_id,
            GetLastError());
        HeapFree(GetProcessHeap(), 0, request);
        return;
    }
    CloseHandle(thread);
}

int assign_kbo_news_pointer_string(void* news_object, uint32_t offset, const char* text)
{
    if (news_object == NULL || text == NULL
            || !memory_range_readable((uint8_t*)news_object + offset, sizeof(void*))) {
        return 0;
    }

    void* string_object = *(void**)((uint8_t*)news_object + offset);
    if (string_object == NULL || !memory_range_readable(string_object, 0x18)) {
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return 0;
    }

    OotpCoreStringAssignFn assign_string =
        (OotpCoreStringAssignFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PISD_STRING_ASSIGN_RVA);
    if (!memory_range_readable((void*)assign_string, 16)) {
        return 0;
    }

    assign_string(string_object, text);
    return 1;
}

uint32_t kbo_read_news_u32(void* object, uint32_t offset)
{
    if (object == NULL || !memory_range_readable((uint8_t*)object + offset, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)((uint8_t*)object + offset);
}

int create_kbo_real_add_news(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body,
    const char* source)
{
    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t global = get_ootp_global_database();
    uintptr_t manager = global;
    if (exe == NULL || manager == 0 || title == NULL || title[0] == '\0') {
        kbo_log_runtimef(
            "league news real_add skipped source=%s title=%s reason=no_exe_manager_or_title exe=%p manager=%p global=%p",
            source != NULL ? source : "",
            title != NULL ? title : "",
            exe,
            (void*)manager,
            (void*)global);
        return 0;
    }

    OotpOperatorNewFn ootp_new =
        (OotpOperatorNewFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_UI_OPERATOR_NEW_RVA);
    OotpNewsObjectCtorFn ctor =
        (OotpNewsObjectCtorFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_NEWS_OBJECT_CTOR_RVA);
    OotpNewsStringEnsureFn ensure_strings =
        (OotpNewsStringEnsureFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_NEWS_STRING_ENSURE_RVA);
    OotpLeagueNewsRealAddFn real_add =
        (OotpLeagueNewsRealAddFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_LEAGUE_NEWS_REAL_ADD_RVA);
    if (!memory_range_readable((void*)ootp_new, 16)
            || !memory_range_readable((void*)ctor, 16)
            || !memory_range_readable((void*)ensure_strings, 16)
            || !memory_range_readable((void*)real_add, 16)) {
        kbo_log_runtimef(
            "league news real_add skipped source=%s title=%s reason=build_specific_func_unavailable new=%p ctor=%p ensure=%p add=%p",
            source != NULL ? source : "",
            title,
            (void*)ootp_new,
            (void*)ctor,
            (void*)ensure_strings,
            (void*)real_add);
        return 0;
    }

    uint8_t* news = (uint8_t*)ootp_new(OOTP27_MESSAGE_OBJECT_SIZE);
    if (news == NULL || !memory_range_readable(news, OOTP27_MESSAGE_OBJECT_SIZE)) {
        kbo_log_runtimef(
            "league news real_add skipped source=%s title=%s reason=alloc_failed object=%p",
            source != NULL ? source : "",
            title,
            news);
        return 0;
    }

    ctor(news);
    ensure_strings(news);

    char original_title[512] = {0};
    char original_body[8192] = {0};
    snprintf(original_title, sizeof(original_title), "%s", title);
    snprintf(original_body, sizeof(original_body), "%s", body != NULL ? body : "");
    const char* title_for_ootp = title;
    const char* body_for_ootp = body != NULL ? body : "";
    char* internal_title = kbo_alloc_ootp_internal_text(title_for_ootp);
    char* internal_body = kbo_alloc_ootp_internal_text(body_for_ootp);
    if (internal_title != NULL) {
        title_for_ootp = internal_title;
    }
    if (internal_body != NULL) {
        body_for_ootp = internal_body;
    }
    KboNewsRelatedIds related;
    kbo_news_related_ids_collect_pair(&related, title, body);
    uint32_t primary_team_id = related.team_count > 0 ? related.team_ids[0] : 0u;
    uint32_t primary_player_id = related.player_count > 0 ? related.player_ids[0] : 0u;

    *(uint32_t*)(news + OOTP27_NEWS_LEAGUE_ID_OFFSET) = league_id;
    *(uint32_t*)(news + OOTP27_NEWS_CATEGORY_OFFSET) = 3u;
    *(uint32_t*)(news + OOTP27_NEWS_FLAGS_54_OFFSET) = 0u;
    *(uint32_t*)(news + OOTP27_NEWS_FLAGS_58_OFFSET) = 1u;
    *(uint32_t*)(news + OOTP27_NEWS_TYPE_OFFSET) = 3u;
    *(uint32_t*)(news + OOTP27_NEWS_SECONDARY_LEAGUE_ID_OFFSET) = league_id;
    *(uint32_t*)(news + OOTP27_NEWS_TEAM_ID_OFFSET) = primary_team_id != 0u ? primary_team_id : 0xffffffffu;
    *(uint32_t*)(news + OOTP27_NEWS_PLAYER_ID_OFFSET) = primary_player_id;
    *(uint16_t*)(news + OOTP27_NEWS_YEAR_OFFSET) = (uint16_t)year;
    *(news + OOTP27_NEWS_DAY_OFFSET) = (uint8_t)day;
    *(news + OOTP27_NEWS_MONTH_OFFSET) = (uint8_t)month;
    *(uint32_t*)(news + OOTP27_NEWS_STATUS_OFFSET) = 0u;
    *(uint32_t*)(news + OOTP27_NEWS_PRIORITY_OFFSET) = 0x00000100u;
    *(uint32_t*)(news + OOTP27_NEWS_RELATED_ID_OFFSET) = 0xffffffffu;
    *(uint32_t*)(news + OOTP27_NEWS_VISIBLE_OFFSET) = 0x00000001u;
    *(uint32_t*)(news + OOTP27_MESSAGE_OBJECT_ID_OFFSET) = 0u;

    int title_ok = assign_kbo_news_pointer_string(news, 0x90u, title_for_ootp);
    int body_ok = assign_kbo_news_pointer_string(news, 0x98u, body_for_ootp);
    uint32_t result = real_add((void*)manager, news, 0);
    uint32_t message_id = kbo_read_news_u32(news, OOTP27_MESSAGE_OBJECT_ID_OFFSET);
    if (message_id == 0 && result != 0) {
        message_id = result;
    }
    int body_file_ok = 0;
    if (result != 0 && message_id != 0) {
        body_file_ok = write_kbo_message_body_file(message_id, original_title, original_body, source);
        kbo_schedule_message_dat_primary_patch(
            message_id,
            primary_player_id,
            primary_team_id,
            source);
    }
    int source_mutated = strcmp(title, original_title) != 0
        || strcmp(body != NULL ? body : "", original_body) != 0;

    kbo_log_runtimef(
        "league news real_add create source=%s title=%s date=%04u-%02u-%02u league_id=%u type=%u disk_primary_player=%u disk_primary_team=%u related_players=%d related_teams=%d manager=%p object=%p result=%u id8c=%u title_ok=%d body_ok=%d body_file=%d source_mutated=%d title_encoding=%s body_encoding=%s",
        source != NULL ? source : "",
        original_title,
        year,
        month,
        day,
        league_id,
        message_type,
        primary_player_id,
        primary_team_id,
        related.player_count,
        related.team_count,
        (void*)manager,
        news,
        result,
        message_id,
        title_ok,
        body_ok,
        body_file_ok,
        source_mutated,
        internal_title != NULL ? "ootp-internal" : "raw",
        internal_body != NULL ? "ootp-internal" : "raw");
    if (internal_title != NULL) {
        kbo_free_ootp_internal_text(internal_title);
    }
    if (internal_body != NULL) {
        kbo_free_ootp_internal_text(internal_body);
    }
    return result != 0;
}
