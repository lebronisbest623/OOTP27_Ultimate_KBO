#include "../internal/ui_military_view_internal.h"

void kbo_webview_append_military_roster_view(KboWindowTextBuffer* buffer)
{
    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    char sang_name[64] = {0};
    char kpb_name[64]  = {0};
    kbo_hub_copy_team_display_name_from_ptr(sang, sang_name, sizeof(sang_name), "상무");
    kbo_hub_copy_team_display_name_from_ptr(kpb,  kpb_name,  sizeof(kpb_name), "경찰 야구단");

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, NULL);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable serviceRosterTable'><thead><tr>"
        "<th class='roPo' data-sort-type='text'>포지션</th><th class='roNum' data-sort-type='number'></th><th class='roName' data-sort-type='text'>선수</th>"
        "<th class='roLeague' data-sort-type='text'>복무 구단</th><th class='roClub' data-sort-type='text'>원 소속</th><th class='roReturn' data-sort-type='text'>복귀일</th></tr></thead><tbody>");

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    int rendered = 0;
    if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        for (int32_t i = 0; i < player_count && rendered < 500; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) { continue; }

            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
            const char* service_fallback = NULL;
            uint32_t service_team_id = 0;
            if (sang_id != 0 && (current_team_id == sang_id || loan_team_id == sang_id)) {
                service_team_id  = sang_id;
                service_fallback = sang_name;
            } else if (kpb_id != 0 && (current_team_id == kpb_id || loan_team_id == kpb_id)) {
                service_team_id  = kpb_id;
                service_fallback = kpb_name;
            }
            if (service_team_id == 0) { continue; }

            uint32_t original_team_id = 0u;
            uint32_t original_league_id = 0u;
            kbo_military_resolve_original_team(
                player,
                service_team_id,
                sang_id,
                kpb_id,
                &original_team_id,
                &original_league_id);

            char player_name[96] = {0};
            char uniform_number[8] = {0};
            char service_team_name[64] = {0};
            char original_team_name[64] = {0};
            char return_date[16] = {0};
            kbo_hub_copy_player_display_name(player, player_name, sizeof(player_name));
            kbo_webview_copy_player_uniform_number(player_id, uniform_number, sizeof(uniform_number));
            kbo_hub_copy_team_display_name_by_id(service_team_id,  service_team_name,  sizeof(service_team_name),  service_fallback);
            kbo_hub_copy_team_display_name_by_id(original_team_id, original_team_name, sizeof(original_team_name), NULL);
            kbo_military_format_yyyymmdd(
                kbo_military_effective_return_yyyymmdd(player),
                return_date,
                sizeof(return_date));

            kbo_window_text_appendf(
                buffer,
                "<tr><td class='roPo'>%s</td><td class='roNum'>",
                kbo_webview_player_position_label(player, 0u));
            kbo_html_append_escaped(buffer, uniform_number);
            kbo_window_text_appendf(buffer, "</td>");
            kbo_webview_append_player_name_cell(buffer, player_name[0] != '\0' ? player_name : "알 수 없는 선수", player_id);
            kbo_window_text_appendf(buffer, "<td class='roLeague'>");
            kbo_html_append_escaped(buffer, service_team_name);
            kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
            kbo_html_append_escaped(buffer, original_team_name);
            kbo_window_text_appendf(
                buffer,
                "</td><td class='roReturn'>%s</td></tr>",
                return_date);
            rendered++;
        }
        if (rendered == 0) {
            kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
        }
    } else {
        kbo_window_text_appendf(buffer, "<tr><td colspan='6'></td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
