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
#include "../../dates/core_text_date.h"
#include "../../core_flags/api/flags_api.h"
#include "../../text/ootp_text_encoding.h"
#include "../../../runtime_memory/runtime_memory.h"

/* Core native news object construction helpers. */

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
    OotpNewsRelatedAddFn add_related_player =
        (OotpNewsRelatedAddFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_NEWS_ADD_RELATED_PLAYER_RVA);
    OotpNewsRelatedAddFn add_related_team =
        (OotpNewsRelatedAddFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_NEWS_ADD_RELATED_TEAM_RVA);
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
    uint32_t object_category = 3u;
    uint32_t object_type = primary_player_id != 0u && primary_team_id != 0u ? 5u : 3u;

    *(uint32_t*)(news + OOTP27_NEWS_LEAGUE_ID_OFFSET) = league_id;
    *(uint32_t*)(news + OOTP27_NEWS_CATEGORY_OFFSET) = object_category;
    *(uint32_t*)(news + OOTP27_NEWS_FLAGS_54_OFFSET) = 0u;
    *(uint32_t*)(news + OOTP27_NEWS_FLAGS_58_OFFSET) = 1u;
    *(uint32_t*)(news + OOTP27_NEWS_TYPE_OFFSET) = object_type;
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

    int related_player_helper_ok = memory_range_readable((void*)add_related_player, 16);
    int related_team_helper_ok = memory_range_readable((void*)add_related_team, 16);
    if (related_player_helper_ok) {
        for (int i = 0; i < related.player_count; i++) {
            add_related_player(news, related.player_ids[i]);
        }
    }
    if (related_team_helper_ok) {
        for (int i = 0; i < related.team_count; i++) {
            add_related_team(news, related.team_ids[i]);
        }
    }
    uint32_t related_player_slot0 = kbo_read_news_u32(news, OOTP27_NEWS_RELATED_PLAYERS_OFFSET);
    uint32_t related_team_slot0 = kbo_read_news_u32(news, OOTP27_NEWS_RELATED_TEAMS_OFFSET);
    int related_player_slot_ok = primary_player_id == 0u || related_player_slot0 == primary_player_id;
    int related_team_slot_ok = primary_team_id == 0u || related_team_slot0 == primary_team_id;

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
    }
    int source_mutated = strcmp(title, original_title) != 0
        || strcmp(body != NULL ? body : "", original_body) != 0;

    kbo_log_runtimef(
        "league news real_add create source=%s title=%s date=%04u-%02u-%02u league_id=%u type=%u object_category=%u object_type=%u primary_player=%u primary_team=%u related_player_slot0=%u related_team_slot0=%u related_player_slot_ok=%d related_team_slot_ok=%d related_player_helper_ok=%d related_team_helper_ok=%d related_players=%d related_teams=%d manager=%p object=%p result=%u id8c=%u title_ok=%d body_ok=%d body_file=%d source_mutated=%d title_encoding=%s body_encoding=%s",
        source != NULL ? source : "",
        original_title,
        year,
        month,
        day,
        league_id,
        message_type,
        object_category,
        object_type,
        primary_player_id,
        primary_team_id,
        related_player_slot0,
        related_team_slot0,
        related_player_slot_ok,
        related_team_slot_ok,
        related_player_helper_ok,
        related_team_helper_ok,
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
