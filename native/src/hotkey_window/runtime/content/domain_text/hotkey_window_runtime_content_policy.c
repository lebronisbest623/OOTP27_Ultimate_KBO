#include "../hotkey_window_runtime_content.h"
#include "../../hotkey_window_domain_contract.h"
#include "../../../../core/product/ootp_product.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"

static void kbo_refresh_foreign_injury_slots_for_hub(const char* source)
{
    uint32_t today = 0u;
    if (kbo_current_date_tick_latest_published_date(&today) && today != 0u) {
        kbo_foreign_injury_replacement_scan_captured_date(source, today);
    } else {
        kbo_ensure_foreign_injury_replacements_loaded();
    }
}

void kbo_build_foreign_injury_replacement_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    uint32_t selected_team_id = g_kbo_hub_selected_team_id;
    char selected_team_name[96] = {0};
    kbo_hub_copy_team_display_name_by_id(selected_team_id, selected_team_name, sizeof(selected_team_name), NULL);
    kbo_refresh_foreign_injury_slots_for_hub("foreign_injury_hub_render");

    int open_count = 0;
    int pending_count = 0;
    int closed_count = 0;
    kbo_count_foreign_injury_replacements_for_team(selected_team_id, &open_count, &pending_count, &closed_count);

    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    kbo_count_team_asian_quota_probe(selected_team_id, &team_foreign, &team_asian, &team_non_asian);
    uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);

    kbo_window_text_appendf(&buffer, "FOREIGN INJURY REPLACEMENT\r\n");
    kbo_window_text_appendf(&buffer, "Team: %s (%u)\r\n", selected_team_name, selected_team_id);
    kbo_window_text_appendf(&buffer, "Open: %d / Decision due: %d / Closed: %d\r\n", open_count, pending_count, closed_count);
    kbo_window_text_appendf(
        &buffer,
        "Raw foreign: %u / Asian: %u / Non-Asian: %u / Effective foreign: %u\r\n\r\n",
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective);
    kbo_window_text_appendf(&buffer, "  SLOT          INJURED PLAYER            ID          REPLACEMENT              ID          DAYS  STATUS\r\n");
    kbo_window_text_appendf(&buffer, "--------------------------------------------------------------------------------------------------------\r\n");

    int rendered = 0;
    kbo_ensure_foreign_injury_replacements_loaded();
    kbo_lock_foreign_injury_replacements();
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count && rendered < 500; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (selected_team_id != 0u && rec->team_id != selected_team_id) {
            continue;
        }

        char player_name[64] = {0};
        char replacement_name[64] = {0};
        int days_left = 0;
        uint8_t* player = kbo_find_player_by_id(rec->injured_player_id, NULL, NULL);
        if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            days_left = (int)*(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET);
        } else {
            snprintf(player_name, sizeof(player_name), "Player #%u", rec->injured_player_id);
        }
        uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
        if (replacement != NULL && memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)) {
            kbo_hub_copy_player_display_name(replacement, replacement_name, sizeof(replacement_name));
        } else if (rec->replacement_player_id != 0u) {
            snprintf(replacement_name, sizeof(replacement_name), "Player #%u", rec->replacement_player_id);
        } else {
            snprintf(replacement_name, sizeof(replacement_name), "-");
        }

        kbo_window_text_appendf(
            &buffer,
            "  %-13.13s %-24.24s %-11u %-24.24s %-11u %-5d %s\r\n",
            kbo_foreign_injury_slot_label(rec->slot_type),
            player_name,
            rec->injured_player_id,
            replacement_name,
            rec->replacement_player_id,
            days_left,
            kbo_foreign_injury_status_label(rec->status));
        rendered++;
    }
    kbo_unlock_foreign_injury_replacements();

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
    }
}

void kbo_build_mod_info_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_window_text_appendf(&buffer, "%s\r\n\r\n", kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "모드 정보"));
    kbo_window_text_appendf(&buffer, "GitHub\r\n");
    kbo_window_text_appendf(&buffer, "  %s\r\n\r\n", KBO_PRODUCT_REQUIRED_ROSTER_MARKER_URL);
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "Ultimate KBO \xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 \xec\x98\xa4\xec\xa7\x81 OOTP\xec\x97\x90\xec\x84\x9c \xec\xb5\x9c\xea\xb3\xa0\xec\x9d\x98 KBO \xea\xb2\xbd\xed\x97\x98\xec\x9d\x84 \xec\x9c\x84\xed\x95\xb4 \xeb\xa7\x8c\xeb\x93\xa4\xec\x96\xb4\xec\xa7\x84 \xeb\xaa\xa8\xeb\x93\x9c\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
            "Ultimate KBO is built only to provide the best KBO experience in OOTP."));
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "\xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 KBO \xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xed\x95\xa8\xea\xbb\x98\xed\x95\xa0 \xeb\x95\x8c \xec\xb5\x9c\xec\x83\x81\xec\x9d\x98 \xea\xb2\xbd\xed\x97\x98\xec\x9d\x84 \xed\x95\xa0 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
            "The mod is best experienced together with the KBO Launcher."));
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "\xeb\xb3\xb8 \xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 OOTPD\xec\x9d\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x92\x88\xec\x9d\xb4 \xec\x95\x84\xeb\x8b\x99\xeb\x8b\x88\xeb\x8b\xa4.",
            "This mod is not an official OOTPD product."));
    for (size_t i = 0; i < kbo_supported_ootp_build_count(); i++) {
        const OotpSupportedBuild* build = kbo_supported_ootp_build_at(i);
        if (build == NULL) {
            continue;
        }
        kbo_window_text_appendf(
            &buffer,
            "Supported build: %s %s / timestamp 0x%08X / image 0x%08X\r\n",
            KBO_OOTP_PRODUCT_SHORT_NAME,
            build->label,
            build->timestamp,
            build->size_of_image);
    }
}

void kbo_build_foreign_policy_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_refresh_foreign_injury_slots_for_hub("foreign_policy_hub_render");

    int configured_nations = kbo_load_asian_quota_nation_ids_once();
    uint32_t team_foreign = 0u;
    uint32_t team_asian = 0u;
    uint32_t team_non_asian = 0u;
    kbo_count_team_asian_quota_probe(g_kbo_hub_selected_team_id, &team_foreign, &team_asian, &team_non_asian);
    uint32_t team_effective = kbo_effective_foreign_count_with_asian_quota(team_asian, team_non_asian);
    kbo_window_text_appendf(&buffer, "KBO FOREIGN PLAYERS\r\n");
    kbo_window_text_appendf(
        &buffer,
        "Policy: %s / Base effective limit: %u / Asian-quota nation IDs: %d\r\n\r\n",
        kbo_custom_foreign_policy_enabled() ? "Custom KBO layer" : "OOTP fallback",
        KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT,
        configured_nations);
    kbo_window_text_appendf(
        &buffer,
        "Raw foreign: %u / Asian: %u / Non-Asian: %u / Effective foreign: %u\r\n\r\n",
        team_foreign,
        team_asian,
        team_non_asian,
        team_effective);
    kbo_window_text_appendf(&buffer, "  SLOT  PLAYER                   ID          TEAM       ACTIVE     NAT     STATUS\r\n");
    kbo_window_text_appendf(&buffer, "----------------------------------------------------------------------------------\r\n");

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_window_text_appendf(&buffer, "\r\n");
        return;
    }

    int rendered = 0;
    for (int32_t i = 0; i < player_count && rendered < 500; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (player_id == 0u
                || !kbo_player_is_foreign_for_kbo_rights(player)
                || !kbo_player_current_assignment_matches_team_or_affiliate(player, g_kbo_hub_selected_team_id)) {
            continue;
        }

        char player_name[64] = {0};
        char flags[64] = {0};
        kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
        snprintf(flags, sizeof(flags), "%s%s%s%s%s",
            player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] ? "Restricted " : "",
            player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] ? "SecRestricted " : "",
            player[OOTP27_PLAYER_DFA_FLAG_OFFSET] ? "DFA " : "",
            player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] ? "Loan " : "",
            player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] ? "Injured " : "");
        if (flags[0] == '\0') {
            snprintf(flags, sizeof(flags), "Active");
        }

        kbo_window_text_appendf(
            &buffer,
            "  %-5.5s %-24.24s %-11u %-10u %-10u %-7u %s\r\n",
            kbo_hub_foreign_slot_code_for_player(player),
            player_name,
            player_id,
            current_team_id,
            active_team_id,
            nation_id,
            flags);
        rendered++;
    }

    if (rendered == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
    }

}

void kbo_build_asian_games_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_clear_asian_games_roster_if_save_changed("hotkey_text");

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0) {
        kbo_load_asian_games_roster_csv("hotkey_text");
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }

    int departed = 0;
    int returned = 0;
    int exempted = 0;
    for (LONG i = 0; i < roster_count; i++) {
        if (g_kbo_asian_games_roster[i].departed) { departed++; }
        if (g_kbo_asian_games_roster[i].returned) { returned++; }
        if (g_kbo_asian_games_roster[i].exempted) { exempted++; }
    }

    const char* result_text = "Pending";
    if (g_kbo_asian_games_result == KBO_ASIAN_GAMES_RESULT_GOLD) {
        result_text = "Gold";
    } else if (g_kbo_asian_games_result == KBO_ASIAN_GAMES_RESULT_NO_GOLD) {
        result_text = "No gold";
    }

    kbo_window_text_appendf(&buffer, "ASIAN GAMES ROSTER\r\n");
    kbo_window_text_appendf(
        &buffer,
        "Year: %u / Result: %s / Selected: %ld / Departed: %d / Returned: %d / Exempted: %d\r\n\r\n",
        g_kbo_asian_games_roster_year,
        result_text,
        roster_count,
        departed,
        returned,
        exempted);

    if (roster_count == 0) {
        kbo_window_text_appendf(&buffer, "\r\n");
        kbo_window_text_appendf(&buffer, "Advance through the roster-selection event, then refresh this panel.\r\n");
        return;
    }

    kbo_window_text_appendf(&buffer, "  #  PLAYER                   ID          TEAM       LG         AGE ROLE WC  STATUS\r\n");
    kbo_window_text_appendf(&buffer, "----------------------------------------------------------------------------------------\r\n");

    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        uintptr_t player_ptr = entry->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_find_player_by_id(entry->player_id, NULL, NULL);
        }

        char player_name[64] = {0};
        if (kbo_player_pointer_plausible(player_ptr)) {
            kbo_hub_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
        } else {
            snprintf(player_name, sizeof(player_name), "Unknown player");
        }

        const char* status = "Selected";
        if (entry->returned) {
            status = entry->exempted ? "Returned/Exempt" : "Returned";
        } else if (entry->departed) {
            status = "Departed/IL";
        }

        kbo_window_text_appendf(
            &buffer,
            "  %02ld %-24.24s %-11u %-10u %-10u %-3u %-4s %-3s %s\r\n",
            i + 1,
            player_name,
            entry->player_id,
            entry->original_team_id,
            entry->original_league_id,
            entry->age,
            kbo_asian_games_role_bucket_label(entry->role),
            entry->wildcard ? "YES" : "NO",
            status);
    }
}

