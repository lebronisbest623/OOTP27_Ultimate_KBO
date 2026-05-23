#include "ui_secondary_draft_view_internal.h"

#include <string.h>

typedef struct KboSecondaryDraftListMetrics {
    int auto_protected;
    int saved_protected;
    int exposed;
    int drafted;
} KboSecondaryDraftListMetrics;

static void kbo_secondary_draft_ui_count_list_metrics(
    const KboSecondaryDraftCandidateRow* rows,
    int count,
    KboSecondaryDraftListMetrics* out)
{
    if (out == NULL) {
        return;
    }
    out->auto_protected = 0;
    out->saved_protected = 0;
    out->exposed = 0;
    out->drafted = 0;
    if (rows == NULL || count <= 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        const KboSecondaryDraftCandidateRow* row = &rows[i];
        if (row->already_drafted) {
            out->drafted++;
        } else if (row->auto_protected) {
            out->auto_protected++;
        } else if (row->saved_protected) {
            out->saved_protected++;
        } else {
            out->exposed++;
        }
    }
}

static const char* kbo_secondary_draft_ui_list_bucket_label(const KboSecondaryDraftCandidateRow* row)
{
    if (row == NULL) {
        return "-";
    }
    if (row->already_drafted) {
        return "\xec\xa7\x80\xeb\xaa\x85\xeb\x90\xa8";
    }
    if (row->auto_protected) {
        return "\xec\x9e\x90\xeb\x8f\x99 \xeb\xb3\xb4\xed\x98\xb8";
    }
    if (row->saved_protected) {
        return "\xeb\xb3\xb4\xed\x98\xb8";
    }
    return "\xeb\x85\xb8\xec\xb6\x9c \xec\x9c\x84\xed\x97\x98";
}

static const char* kbo_secondary_draft_ui_list_note_label(const KboSecondaryDraftCandidateRow* row)
{
    if (row == NULL) {
        return "-";
    }
    if (row->already_drafted) {
        return "\xec\x9d\xb4\xeb\xaf\xb8 \xec\xa7\x80\xeb\xaa\x85";
    }
    if (row->auto_protected) {
        return "\xec\x97\xb0\xec\xb0\xa8 \xec\x9e\x90\xeb\x8f\x99 \xeb\xb3\xb4\xed\x98\xb8";
    }
    if (row->submitted && row->saved_protected) {
        return "\xec\xa0\x9c\xec\xb6\x9c \xeb\xb3\xb4\xed\x98\xb8";
    }
    if (row->submitted) {
        return "\xec\xa0\x9c\xec\xb6\x9c \xed\x9b\x84 \xeb\x85\xb8\xec\xb6\x9c";
    }
    if (row->saved_protected) {
        return "\xeb\xaa\x85\xeb\x8b\xa8\xec\x97\x90 \xed\x8f\xac\xed\x95\xa8\xeb\x90\xa8";
    }
    return "\xeb\x93\x9c\xeb\x9e\x98\xed\x94\x84\xed\x8a\xb8 \xeb\x8c\x80\xec\x83\x81";
}

static void kbo_secondary_draft_ui_append_list_metrics(
    KboWindowTextBuffer* buffer,
    const KboSecondaryDraftListMetrics* metrics,
    int saved_count,
    int submitted,
    const KboSecondaryDraftWindow* window,
    const char* window_status)
{
    if (buffer == NULL || metrics == NULL) {
        return;
    }
    int remaining = KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT - saved_count;
    if (remaining < 0) {
        remaining = 0;
    }
    char open_text[16] = "-";
    char deadline_text[16] = "-";
    if (window != NULL && window->season != 0u) {
        kbo_secondary_draft_ui_format_date(
            window->protection_open_yyyymmdd,
            open_text,
            sizeof(open_text));
        kbo_secondary_draft_ui_format_date(
            window->protection_deadline_yyyymmdd,
            deadline_text,
            sizeof(deadline_text));
    }
    kbo_window_text_appendf(
        buffer,
        "<div class='rosterTopBar'><div class='rosterTopText'>"
        "\xec\xa0\x9c\xec\xb6\x9c \xec\xb0\xbd %s~%s | %s | "
        "\xeb\xb3\xb4\xed\x98\xb8 %d/%d | \xec\x9e\x90\xeb\x8f\x99 \xeb\xb3\xb4\xed\x98\xb8 %d | "
        "\xeb\x82\xa8\xec\x9d\x80 \xec\x8a\xac\xeb\xa1\xaf %d | \xeb\x85\xb8\xec\xb6\x9c \xec\x9c\x84\xed\x97\x98 %d | "
        "\xec\xa7\x80\xeb\xaa\x85\xeb\x90\xa8 %d | %s"
        "</div></div>",
        open_text,
        deadline_text,
        window_status != NULL ? window_status : "-",
        saved_count,
        KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT,
        metrics->auto_protected,
        remaining,
        metrics->exposed,
        metrics->drafted,
        window_status != NULL
            ? window_status
            : (submitted ? "\xec\xa0\x9c\xec\xb6\x9c\xeb\x90\xa8" : "\xed\x8e\xb8\xec\xa7\x91 \xea\xb0\x80\xeb\x8a\xa5"));
}

static uint32_t kbo_secondary_draft_ui_today_yyyymmdd(void)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_tick_latest_components(&year, &month, &day)
            || year == 0u
            || month == 0u
            || day == 0u) {
        return 0u;
    }
    return year * 10000u + month * 100u + day;
}

static const char* kbo_secondary_draft_ui_list_window_status(
    int has_window,
    int submitted,
    uint32_t today,
    const KboSecondaryDraftWindow* window)
{
    if (!has_window || window == NULL) {
        return "\xec\xa0\x9c\xec\xb6\x9c \xec\xb0\xbd \xec\x97\x86\xec\x9d\x8c";
    }
    if (submitted) {
        return "\xec\xa0\x9c\xec\xb6\x9c\xeb\x90\xa8";
    }
    if (today != 0u
            && today >= window->protection_open_yyyymmdd
            && today <= window->protection_deadline_yyyymmdd) {
        return "\xed\x8e\xb8\xec\xa7\x91 \xea\xb0\x80\xeb\x8a\xa5";
    }
    if (today != 0u && today < window->protection_open_yyyymmdd) {
        return "\xeb\x8c\x80\xea\xb8\xb0";
    }
    return "\xeb\xaa\x85\xeb\x8b\xa8 \xec\x9e\xa0\xea\xb9\x80";
}

static void kbo_secondary_draft_ui_append_protected_panel(
    KboWindowTextBuffer* buffer,
    KboSecondaryDraftCandidateRow* rows,
    int count,
    uint32_t season,
    uint32_t team_id,
    int submitted,
    int saved_count,
    int action_available,
    const char* window_status)
{
    kbo_window_text_appendf(
        buffer,
        "<aside class='secondaryDraftProtectedPane'>"
        "<div class='secondaryDraftProtectedHead'>"
        "<div class='secondaryDraftProtectedTitle'>\xec\xa0\x9c\xec\xb6\x9c \xeb\xb3\xb4\xed\x98\xb8 \xeb\xaa\x85\xeb\x8b\xa8</div>"
        "<div class='secondaryDraftProtectedMeta'>%d/%d | %s</div>"
        "</div>"
        "<section class='tablewrap rosterTableWrap secondaryDraftProtectedWrap'>"
        "<table class='ootpRosterTable secondaryDraftProtectedTable'><thead><tr>"
        "<th class='roAction'>\xec\x9e\x91\xec\x97\x85</th>"
        "<th class='roName' data-sort-type='text'>\xec\x84\xa0\xec\x88\x98</th>"
        "<th class='roAge' data-sort-type='number'>\xeb\x82\x98\xec\x9d\xb4</th>"
        "<th class='roStatus' data-sort-type='text'>\xec\x83\x81\xed\x83\x9c</th>"
        "</tr></thead><tbody>",
        saved_count,
        KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT,
        window_status != NULL ? window_status : "-");

    int protected_rows = 0;
    for (int i = 0; i < count; i++) {
        KboSecondaryDraftCandidateRow* row = &rows[i];
        if (!row->saved_protected) {
            continue;
        }
        protected_rows++;
        int can_remove = action_available && !submitted && !row->auto_protected && !row->already_drafted;
        kbo_window_text_appendf(buffer, "<tr><td class='roAction'><span class='rightsActions'>");
        if (can_remove) {
            kbo_window_text_appendf(
                buffer,
                "<a class='rightsAction rightsCancel' title='\xeb\xb3\xb4\xed\x98\xb8 \xeb\xaa\x85\xeb\x8b\xa8\xec\x97\x90\xec\x84\x9c \xec\xa0\x9c\xec\x99\xb8' href='kbo://secondary-draft/unprotect/%u/%u/%u'>Remove</a>",
                season,
                team_id,
                row->player_id);
        } else {
            kbo_window_text_appendf(buffer, "<span class='rightsAction rightsCancel disabled'>-</span>");
        }
        kbo_window_text_appendf(buffer, "</span></td>");
        kbo_webview_append_player_name_cell(buffer, row->player_name[0] != '\0' ? row->player_name : "-", row->player_id);
        kbo_window_text_appendf(
            buffer,
            "<td class='roAge'>%u</td><td class='roStatus'>",
            row->age);
        kbo_html_append_escaped(buffer, kbo_secondary_draft_ui_list_note_label(row));
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    if (protected_rows <= 0) {
        kbo_secondary_draft_ui_append_empty_row(
            buffer,
            4,
            "\xeb\xb3\xb4\xed\x98\xb8\xeb\x90\x9c \xec\x84\xa0\xec\x88\x98\xea\xb0\x80 \xec\x97\x86\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></aside>");
}

void kbo_secondary_draft_ui_append_list_view(
    KboWindowTextBuffer* buffer,
    uint32_t season,
    uint32_t team_id)
{
    KboSecondaryDraftCandidateRow rows[KBO_SECONDARY_DRAFT_UI_MAX_CANDIDATES];
    int submitted = 0;
    int saved_count = 0;
    int count = kbo_secondary_draft_collect_team_list_rows(
        season,
        team_id,
        rows,
        KBO_SECONDARY_DRAFT_UI_MAX_CANDIDATES,
        &submitted,
        &saved_count);
    KboSecondaryDraftWindow window;
    memset(&window, 0, sizeof(window));
    int has_window = kbo_secondary_draft_load_window(season, &window);
    uint32_t today = kbo_secondary_draft_ui_today_yyyymmdd();
    const char* window_status = kbo_secondary_draft_ui_list_window_status(
        has_window,
        submitted,
        today,
        &window);
    int window_open = kbo_secondary_draft_protection_window_open(season);
    int team_action_available = team_id != 0u
        && kbo_hub_ui_team_action_available(team_id, "hub_secondary_draft_list_render");
    const char* action_status = team_id == 0u
        ? "\xea\xb5\xac\xeb\x8b\xa8 \xec\x84\xa0\xed\x83\x9d \xed\x95\x84\xec\x9a\x94"
        : (team_action_available
            ? window_status
            : "\xea\xb5\xac\xeb\x8b\xa8 \xea\xb6\x8c\xed\x95\x9c \xec\x97\x86\xec\x9d\x8c");
    int action_available = team_action_available && window_open;
    kbo_secondary_draft_ui_append_action_bar(
        buffer,
        season,
        team_id,
        submitted,
        saved_count,
        action_available,
        action_status);
    KboSecondaryDraftListMetrics metrics;
    kbo_secondary_draft_ui_count_list_metrics(rows, count, &metrics);
    kbo_secondary_draft_ui_append_list_metrics(
        buffer,
        &metrics,
        saved_count,
        submitted,
        has_window ? &window : NULL,
        window_status);

    kbo_window_text_appendf(
        buffer,
        "<div class='secondaryDraftProtectionBoard'><div class='secondaryDraftCandidatesPane'>");
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap secondaryDraftCandidatesWrap'><table class='ootpRosterTable secondaryDraftListTable'><thead><tr>"
        "<th class='roAction'>\xec\x9e\x91\xec\x97\x85</th>"
        "<th class='roStatus' data-sort-type='text'>\xea\xb5\xac\xeb\xb6\x84</th>"
        "<th class='roName' data-sort-type='text'>\xec\x84\xa0\xec\x88\x98</th>"
        "<th class='roAge' data-sort-type='number'>\xeb\x82\x98\xec\x9d\xb4</th>"
        "<th class='roClub' data-sort-type='text'>\xeb\xb9\x84\xea\xb3\xa0</th>"
        "</tr></thead><tbody>");
    if (team_id == 0u) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 5, "\xeb\xa8\xbc\xec\xa0\x80 KBO \xea\xb5\xac\xeb\x8b\xa8\xec\x9d\x84 \xec\x84\xa0\xed\x83\x9d\xed\x95\x98\xec\x84\xb8\xec\x9a\x94.");
    } else if (count <= 0) {
        kbo_secondary_draft_ui_append_empty_row(buffer, 5, "\xec\x9d\xb4 \xea\xb5\xac\xeb\x8b\xa8\xec\x9d\x98 \xed\x9b\x84\xeb\xb3\xb4 \xec\x84\xa0\xec\x88\x98\xea\xb0\x80 \xec\x97\x86\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.");
    }

    for (int i = 0; i < count; i++) {
        KboSecondaryDraftCandidateRow* row = &rows[i];
        int can_edit = action_available && !submitted && !row->auto_protected && !row->already_drafted;
        kbo_window_text_appendf(buffer, "<tr><td class='roAction'><span class='rightsActions'>");
        if (can_edit && row->saved_protected) {
            kbo_window_text_appendf(
                buffer,
                "<a class='rightsAction rightsCancel' title='\xeb\xb3\xb4\xed\x98\xb8 \xeb\xaa\x85\xeb\x8b\xa8\xec\x97\x90\xec\x84\x9c \xec\xa0\x9c\xec\x99\xb8' href='kbo://secondary-draft/unprotect/%u/%u/%u'>Remove</a>",
                season,
                team_id,
                row->player_id);
        } else if (can_edit && saved_count < KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT) {
            kbo_window_text_appendf(
                buffer,
                "<a class='rightsAction rightsAdd' title='\xeb\xb3\xb4\xed\x98\xb8 \xeb\xaa\x85\xeb\x8b\xa8\xec\x97\x90 \xec\xb6\x94\xea\xb0\x80' href='kbo://secondary-draft/protect/%u/%u/%u'>Protect</a>",
                season,
                team_id,
                row->player_id);
        } else {
            kbo_window_text_appendf(buffer, "<span class='rightsAction rightsAdd disabled'>-</span>");
        }
        kbo_window_text_appendf(buffer, "</span></td>");
        kbo_window_text_appendf(buffer, "<td class='roStatus'>");
        kbo_html_append_escaped(buffer, kbo_secondary_draft_ui_list_bucket_label(row));
        kbo_window_text_appendf(buffer, "</td>");
        kbo_webview_append_player_name_cell(buffer, row->player_name[0] != '\0' ? row->player_name : "-", row->player_id);
        kbo_window_text_appendf(
            buffer,
            "<td class='roAge'>%u</td><td class='roClub'>",
            row->age);
        kbo_html_append_escaped(buffer, kbo_secondary_draft_ui_list_note_label(row));
        kbo_window_text_appendf(buffer, "</td></tr>");
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
    kbo_secondary_draft_ui_append_protected_panel(
        buffer,
        rows,
        count,
        season,
        team_id,
        submitted,
        saved_count,
        action_available,
        window_status);
    kbo_window_text_appendf(buffer, "</div>");
}
