#ifndef KBOFIX_SRC_BOOTSTRAP_OOTP_TYPEDEFS_H_
#define KBOFIX_SRC_BOOTSTRAP_OOTP_TYPEDEFS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

typedef void* (__fastcall *OotpCreateLeagueEventFn)(
    void* event_manager,
    void* date,
    uint32_t event_type,
    uint32_t league_id,
    const char* title,
    uint16_t aux_id);

typedef uint8_t (__fastcall *OotpCreateNewsItemFn)(
    uint32_t news_type,
    uint32_t team_id,
    void* date,
    const char* text,
    uint8_t immediate);

typedef void (__fastcall *OotpCreateMessageCoreFn)(
    void* context,
    uint32_t internal_message_type,
    uint32_t team_id,
    void* date,
    const char* text);

typedef int (__fastcall *OotpSqlExecFn)(void* database, const char* sql);

typedef uint32_t (__fastcall *OotpLeagueNewsRealAddFn)(
    void* manager,
    void* news_object,
    uint8_t flag);

typedef void* (__fastcall *OotpNewsObjectCtorFn)(void* news_object);
typedef void (__fastcall *OotpNewsStringEnsureFn)(void* news_object);
typedef void (__fastcall *OotpNewsRelatedAddFn)(void* news_object, uint32_t id);
typedef void* (__fastcall *OotpOperatorNewFn)(size_t size);
typedef void* (__fastcall *OotpCoreStringAssignFn)(void* string_object, const char* text);

#endif
