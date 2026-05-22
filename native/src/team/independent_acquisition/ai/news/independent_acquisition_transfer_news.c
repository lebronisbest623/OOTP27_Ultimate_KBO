#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/news/live/core_live_news.h"
#include "../../../../core/news/templates/core_news_templates.h"
#include "../../../../core/sql/history_transactions/core_sql_history_transactions.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../classification/team_classification.h"
#include "../../../names/team_name_cache.h"
#include "../../../names/team_string.h"

static int kbo_independent_acquisition_team_name_placeholder(const char* text)
{
    return text == NULL
        || text[0] == '\0'
        || _stricmp(text, "Team") == 0
        || _stricmp(text, "Unknown") == 0;
}

static void kbo_independent_acquisition_copy_team_name(
    uint8_t* team,
    uint32_t team_id,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    KboIndependentFuturesTeamLeague futures_teams[32];
    int futures_count = kbo_collect_independent_futures_team_leagues(
        futures_teams,
        (int)(sizeof(futures_teams) / sizeof(futures_teams[0])),
        NULL,
        NULL);
    for (int i = 0; i < futures_count; i++) {
        if (futures_teams[i].team_id == team_id && futures_teams[i].display_name[0] != '\0') {
            snprintf(out, out_size, "%s", futures_teams[i].display_name);
            return;
        }
    }

    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, city, sizeof(city));
        copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_NICKNAME_STRING_OFFSET, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));

        if (!kbo_independent_acquisition_team_name_placeholder(full_name)
                && (strchr(full_name, ' ') != NULL || kbo_ootp_text_has_non_ascii(full_name))) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (_stricmp(city, "Doosan") == 0 && _stricmp(nickname, "DOO") == 0) {
            snprintf(out, out_size, "Doosan Bears");
            return;
        }
        if (_stricmp(city, "Lotte") == 0 && _stricmp(nickname, "LOT") == 0) {
            snprintf(out, out_size, "Lotte Giants");
            return;
        }
        if (_stricmp(city, "Samsung") == 0 && _stricmp(nickname, "SAM") == 0) {
            snprintf(out, out_size, "Samsung Lions");
            return;
        }
        if (_stricmp(city, "KIA") == 0 && _stricmp(nickname, "KIA") == 0) {
            snprintf(out, out_size, "KIA Tigers");
            return;
        }
        if (_stricmp(city, "SSG") == 0 && _stricmp(nickname, "SSG") == 0) {
            snprintf(out, out_size, "SSG Landers");
            return;
        }
        if (_stricmp(city, "Hanwha") == 0 && _stricmp(nickname, "HAN") == 0) {
            snprintf(out, out_size, "Hanwha Eagles");
            return;
        }
        if (_stricmp(city, "Kiwoom") == 0 && _stricmp(nickname, "KIW") == 0) {
            snprintf(out, out_size, "Kiwoom Heroes");
            return;
        }
        if (_stricmp(city, "NC") == 0 && _stricmp(nickname, "NC") == 0) {
            snprintf(out, out_size, "NC Dinos");
            return;
        }
        if (_stricmp(city, "KT") == 0 && _stricmp(nickname, "KT") == 0) {
            snprintf(out, out_size, "KT Wiz");
            return;
        }
        if (_stricmp(city, "LG") == 0 && _stricmp(nickname, "LG") == 0) {
            snprintf(out, out_size, "LG Twins");
            return;
        }
        if (!kbo_independent_acquisition_team_name_placeholder(city)
                && !kbo_independent_acquisition_team_name_placeholder(nickname)
                && _stricmp(city, nickname) != 0) {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        if (!kbo_independent_acquisition_team_name_placeholder(full_name)) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (!kbo_independent_acquisition_team_name_placeholder(nickname)) {
            snprintf(out, out_size, "%s", nickname);
            return;
        }
        if (!kbo_independent_acquisition_team_name_placeholder(city)) {
            snprintf(out, out_size, "%s", city);
            return;
        }
    }

    snprintf(out, out_size, "Team #%u", team_id);
}

static void kbo_independent_acquisition_format_cash_cost(
    int32_t cash_cost,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    char raw[32] = {0};
    snprintf(raw, sizeof(raw), "%d", cash_cost);

    const char* digits = raw;
    int negative = 0;
    if (digits[0] == '-') {
        negative = 1;
        digits++;
    }

    size_t digit_count = strlen(digits);
    if (digit_count == 0u) {
        snprintf(out, out_size, "%d", cash_cost);
        return;
    }

    size_t comma_count = (digit_count - 1u) / 3u;
    size_t needed = digit_count + comma_count + (negative ? 1u : 0u) + 1u;
    if (needed > out_size) {
        snprintf(out, out_size, "%d", cash_cost);
        return;
    }

    char* p = out;
    if (negative) {
        *p++ = '-';
    }
    size_t leading = digit_count % 3u;
    if (leading == 0u) {
        leading = 3u;
    }
    for (size_t i = 0u; i < digit_count; i++) {
        if (i != 0u && (i == leading || (i > leading && ((i - leading) % 3u) == 0u))) {
            *p++ = ',';
        }
        *p++ = digits[i];
    }
    *p = '\0';
}

int kbo_emit_independent_acquisition_transfer_news(
    uint32_t today,
    uint8_t* player,
    uint8_t* buyer_team,
    uint8_t* seller_team,
    uint32_t player_id,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    int32_t cash_cost,
    const char* source)
{
    if (today == 0u || player == NULL || buyer_team == NULL || player_id == 0u || buyer_team_id == 0u) {
        return 0;
    }

    uint32_t league_id = *(uint32_t*)(buyer_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        return 0;
    }

    char player_name[96] = {0};
    kbo_copy_player_display_name(player, player_name, sizeof(player_name));
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", player_id);
    }

    char buyer_name[96] = {0};
    char seller_name[96] = {0};
    kbo_independent_acquisition_copy_team_name(buyer_team, buyer_team_id, buyer_name, sizeof(buyer_name));
    kbo_independent_acquisition_copy_team_name(seller_team, seller_team_id, seller_name, sizeof(seller_name));

    char player_link[128] = {0};
    char buyer_link[128] = {0};
    char seller_link[128] = {0};
    char cash_cost_text[32] = {0};
    snprintf(player_link, sizeof(player_link), "<%s:player#%u>", player_name, player_id);
    snprintf(buyer_link, sizeof(buyer_link), "<%s:team#%u>", buyer_name, buyer_team_id);
    if (seller_team_id != 0u) {
        snprintf(seller_link, sizeof(seller_link), "<%s:team#%u>", seller_name, seller_team_id);
    } else {
        snprintf(seller_link, sizeof(seller_link), "%s", seller_name);
    }
    kbo_independent_acquisition_format_cash_cost(cash_cost, cash_cost_text, sizeof(cash_cost_text));

    char history_text[384] = {0};
    snprintf(
        history_text,
        sizeof(history_text),
        "Purchased by %s from %s for $%s through the independent acquisition window.",
        buyer_name,
        seller_name,
        cash_cost_text);
    int history_inserted = insert_kbo_player_history_sql(
        player_id,
        today / 10000u,
        (today / 100u) % 100u,
        today % 100u,
        history_text,
        "independent_acquisition_transfer");

    char title[180] = {0};
    char body[1024] = {0};
    const KboNewsTemplateVar vars[] = {
        {"player_name", player_name},
        {"player_link", player_link},
        {"buyer_team_name", buyer_name},
        {"buyer_team_link", buyer_link},
        {"seller_team_name", seller_name},
        {"seller_team_link", seller_link},
        {"cash_cost", cash_cost_text},
    };
    if (!kbo_news_template_render_key(
            "independent_acquisition.transfer.title",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            title,
            sizeof(title),
            source)
            || !kbo_news_template_render_key(
                "independent_acquisition.transfer.news.body",
                vars,
                (int)(sizeof(vars) / sizeof(vars[0])),
                body,
                sizeof(body),
                source)) {
        kbo_log_runtimef(
            "independent acquisition transfer news skipped missing_template source=%s player=%u buyer=%u seller=%u",
            source != NULL ? source : "",
            player_id,
            buyer_team_id,
            seller_team_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body_live_required(
        today / 10000u,
        (today / 100u) % 100u,
        today % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    kbo_log_runtimef(
        "independent acquisition transfer news source=%s player=%u buyer=%u seller=%u league=%u cash_cost=%d created=%d history_inserted=%d",
        source != NULL ? source : "",
        player_id,
        buyer_team_id,
        seller_team_id,
        league_id,
        cash_cost,
        created,
        history_inserted);
    return created;
}
