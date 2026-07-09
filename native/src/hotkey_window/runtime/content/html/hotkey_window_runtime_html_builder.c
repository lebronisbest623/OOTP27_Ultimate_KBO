#include "../hotkey_window_runtime_content.h"
#include "../../hotkey_window_domain_contract.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"

#define KBO_WEBVIEW_HUB_HTML_CAP 8388608u
#define KBO_WEBVIEW_HUB_UPDATE_EXTRA_CAP 65536u

typedef struct KboWebViewHubRenderContext {
    char league_name[96];
    char team_name[96];
    char league_logo_path[MAX_PATH];
    char team_logo_path[MAX_PATH];
    char jeju_font_url[MAX_PATH * 3];
    char team_bar_primary[8];
    char team_bar_secondary[8];
    char current_date_text[64];
    char window_status[256];
    char scrollbar_css[65536];
    const char* ui_font_family;
    uint32_t current_year;
    int has_sub_tabs;
    int is_dashboard_panel;
    int is_roster_dashboard;
    int is_mod_dashboard;
} KboWebViewHubRenderContext;

static WCHAR* kbo_webview_utf8_to_wide_heap(const char* text, int* out_wide_len)
{
    if (out_wide_len != NULL) {
        *out_wide_len = 0;
    }
    if (text == NULL) {
        return NULL;
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    WCHAR* wide = NULL;
    if (wide_len > 0) {
        wide = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_len);
        }
    }
    if (out_wide_len != NULL) {
        *out_wide_len = wide_len;
    }
    return wide;
}

static int kbo_webview_prepare_hub_render_context(KboWebViewHubRenderContext* ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->team_bar_primary, sizeof(ctx->team_bar_primary), "#f04a22");
    snprintf(ctx->team_bar_secondary, sizeof(ctx->team_bar_secondary), "#2c2c2c");

    if (g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA) {
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_MARKET;
    }
    kbo_hub_ensure_valid_selection();
    if (!kbo_hub_view_available_for_selected_league(g_kbo_hub_selected_view)) {
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        if (g_kbo_hub_selected_mod_subview < 0
                || g_kbo_hub_selected_mod_subview >= KBO_HUB_MOD_SUBVIEW_COUNT) {
            g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_README;
        }
    }

    ctx->is_mod_dashboard =
        g_kbo_hub_selected_view == KBO_HUB_VIEW_MOD_INFO &&
        (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_README ||
         g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_LICENSE ||
         g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_CREDITS ||
         g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS ||
         g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_SETTINGS);
    ctx->is_roster_dashboard =
        (g_kbo_hub_selected_view == KBO_HUB_VIEW_MILITARY &&
         (g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_ROSTER ||
          g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_APPLICANTS ||
          g_kbo_hub_selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_RESULTS)) ||
        (g_kbo_hub_selected_view == KBO_HUB_VIEW_FOREIGN_RIGHTS) ||
        (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_QUOTA &&
         (g_kbo_hub_selected_foreign_subview == KBO_HUB_FOREIGN_SUBVIEW_ROSTER ||
          g_kbo_hub_selected_foreign_subview == KBO_HUB_FOREIGN_SUBVIEW_RIGHTS)) ||
        (g_kbo_hub_selected_view == KBO_HUB_VIEW_ASIAN_GAMES &&
         (g_kbo_hub_selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS ||
          g_kbo_hub_selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_SCHEDULE ||
          g_kbo_hub_selected_agames_subview == KBO_HUB_AGAMES_SUBVIEW_ROSTER)) ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_SECONDARY_DRAFT ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_FUTURES_LEAGUE ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_UPCOMING_FA ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_FA_CASES ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_REPUTATION ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_CBT;
    ctx->is_dashboard_panel =
        ctx->is_mod_dashboard ||
        ctx->is_roster_dashboard ||
        g_kbo_hub_selected_view == KBO_HUB_VIEW_SETTINGS;
    ctx->has_sub_tabs = kbo_webview_current_view_has_sub_tabs();
    ctx->ui_font_family = kbo_hub_language() == KBO_HUB_LANG_KO
        ? "'KBO Jeju Gothic','Jeju Gothic','Malgun Gothic',sans-serif"
        : "'Malgun Gothic',sans-serif";
    kbo_hub_copy_league_display_name(g_kbo_hub_selected_league_id, ctx->league_name, sizeof(ctx->league_name));
    kbo_hub_copy_team_display_name_by_id(g_kbo_hub_selected_team_id, ctx->team_name, sizeof(ctx->team_name), "No team");
    kbo_get_foreign_waiver_window_status_text(ctx->window_status, sizeof(ctx->window_status));

    uint32_t current_month = 0, current_day = 0;
    if (kbo_current_date_tick_latest_components(
            &ctx->current_year,
            &current_month,
            &current_day)) {
        kbo_hub_format_ootp_date(ctx->current_year, current_month, current_day, ctx->current_date_text, sizeof(ctx->current_date_text));
    } else {
        snprintf(ctx->current_date_text, sizeof(ctx->current_date_text), "DATE UNKNOWN");
    }
    kbo_hub_get_league_logo_path(g_kbo_hub_selected_league_id, ctx->current_year, ctx->league_logo_path, sizeof(ctx->league_logo_path));
    kbo_hub_get_team_logo_path(g_kbo_hub_selected_team_id, ctx->current_year, ctx->team_logo_path, sizeof(ctx->team_logo_path));

    char jeju_font_path[MAX_PATH] = {0};
    kbo_hub_font_asset_path("JejuGothic-Regular.ttf", jeju_font_path, sizeof(jeju_font_path));
    kbo_webview_copy_file_url(jeju_font_path, ctx->jeju_font_url, sizeof(ctx->jeju_font_url));
    kbo_hub_copy_team_bar_colors(
        g_kbo_hub_selected_team_id,
        ctx->team_bar_primary,
        sizeof(ctx->team_bar_primary),
        ctx->team_bar_secondary,
        sizeof(ctx->team_bar_secondary));
    kbo_webview_build_scrollbar_skin_css(ctx->scrollbar_css, sizeof(ctx->scrollbar_css), kbo_hub_skin_scrollbar_width());

    KboWindowTextBuffer extra_css;
    extra_css.data = ctx->scrollbar_css;
    extra_css.capacity = sizeof(ctx->scrollbar_css);
    extra_css.length = strlen(ctx->scrollbar_css);
    kbo_webview_append_roster_table_css(&extra_css);

    if (kbo_hub_current_mode_is_developer()) {
        kbo_log_runtimef(
            "KBO F2 hub html build start view=%d mod=%d foreign=%d military=%d fa=%d fa_comp=%d cbt=%d futures=%d secondary_draft=%d secondary_draft_season=%u league=%u team=%u year=%u has_sub_tabs=%d dashboard_panel=%d roster_dashboard=%d mod_dashboard=%d language=%d league_logo=%d team_logo=%d",
            g_kbo_hub_selected_view,
            g_kbo_hub_selected_mod_subview,
            g_kbo_hub_selected_foreign_subview,
            g_kbo_hub_selected_military_subview,
            g_kbo_hub_selected_fa_subview,
            g_kbo_hub_selected_fa_compensation_subview,
            g_kbo_hub_selected_cbt_subview,
            g_kbo_hub_selected_futures_subview,
            g_kbo_hub_selected_secondary_draft_subview,
            g_kbo_hub_selected_secondary_draft_season,
            g_kbo_hub_selected_league_id,
            g_kbo_hub_selected_team_id,
            ctx->current_year,
            ctx->has_sub_tabs,
            ctx->is_dashboard_panel,
            ctx->is_roster_dashboard,
            ctx->is_mod_dashboard,
            kbo_hub_language(),
            ctx->league_logo_path[0] != '\0',
            ctx->team_logo_path[0] != '\0');
    }
    return 1;
}

static void kbo_webview_append_hub_app(KboWindowTextBuffer* buffer, const KboWebViewHubRenderContext* ctx)
{
    if (buffer == NULL || ctx == NULL) {
        return;
    }

    kbo_window_text_appendf(
        buffer,
        "<div class='app %s' style='--team-primary:%s;--team-secondary:%s'><header class='top'><div class='identity'>",
        ctx->has_sub_tabs ? "hasSubTabs" : "noSubTabs",
        ctx->team_bar_primary,
        ctx->team_bar_secondary);
    const char* header_logo_path = ctx->team_logo_path[0] != '\0' ? ctx->team_logo_path : ctx->league_logo_path;
    if (header_logo_path[0] != '\0') {
        kbo_window_text_appendf(buffer, "<img class='logo' src='");
        kbo_webview_append_image_src(buffer, header_logo_path);
        kbo_window_text_appendf(buffer, "'>");
    }
    kbo_window_text_appendf(buffer, "<div class='brandBlock'><a class='brand' href='kbo://team'>");
    kbo_html_append_escaped(buffer, ctx->team_name);
    kbo_window_text_appendf(buffer, " <span>v</span></a><div class='date'><a href='kbo://league'>");
    kbo_html_append_escaped(buffer, ctx->league_name);
    kbo_window_text_appendf(buffer, "</a> / ");
    kbo_html_append_escaped(buffer, ctx->current_date_text);
    kbo_window_text_appendf(buffer, "</div></div></div>");
    kbo_window_text_appendf(buffer, "</header>");
    if (g_kbo_hub_open_dropdown == 1) {
        kbo_webview_append_league_dropdown(buffer, ctx->current_year);
    } else if (g_kbo_hub_open_dropdown == 2) {
        kbo_webview_append_team_dropdown(buffer, ctx->current_year);
    }

    kbo_window_text_appendf(buffer, "<nav class='mainTabs'>");
    kbo_webview_append_main_tabs(buffer);
    kbo_window_text_appendf(buffer, "</nav>");
    if (ctx->has_sub_tabs) {
        kbo_window_text_appendf(buffer, "<nav class='subTabs'>");
        kbo_webview_append_sub_tabs(buffer);
        kbo_window_text_appendf(buffer, "</nav>");
    }
    kbo_window_text_appendf(
        buffer,
        "<main class='panel %s'><div class='panelHead'><h1>",
        ctx->is_dashboard_panel ? "dashboardPanel" : "");
    kbo_html_append_escaped(buffer, kbo_hub_current_view_title());
    kbo_window_text_appendf(buffer, "</h1><p>");
    kbo_html_append_escaped(buffer, kbo_hub_current_view_subtitle());
    kbo_window_text_appendf(buffer, "</p></div><section class='content'>");

    KBO_PROFILE_BEGIN(profile_webview_selected_view);
    size_t selected_view_start = buffer->length;
    kbo_webview_append_selected_view(buffer, ctx->current_year, ctx->window_status);
    size_t selected_view_bytes = buffer->length >= selected_view_start
        ? buffer->length - selected_view_start
        : 0u;
    if (kbo_hub_current_mode_is_developer()) {
        kbo_log_runtimef(
            "KBO F2 hub selected view html appended view=%d mod=%d bytes=%llu total_bytes=%llu capacity=%llu truncated=%d",
            g_kbo_hub_selected_view,
            g_kbo_hub_selected_mod_subview,
            (unsigned long long)selected_view_bytes,
            (unsigned long long)buffer->length,
            (unsigned long long)buffer->capacity,
            buffer->length >= buffer->capacity - 1u ? 1 : 0);
    }
    KBO_PROFILE_END(profile_webview_selected_view, "webview.build_html.selected_view");
    kbo_window_text_appendf(buffer, "</section></main></div>");
}

static void kbo_webview_append_hub_diagnostic_script(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL || !kbo_hub_current_mode_is_developer()) {
        return;
    }

    kbo_window_text_appendf(
        buffer,
        "<script>(function(){"
        "function q(s){return document.querySelector(s)}"
        "function sz(n){if(!n)return 'missing';var r=n.getBoundingClientRect();return Math.round(r.width)+'x'+Math.round(r.height)}"
        "function bg(n){return n?getComputedStyle(n).backgroundColor:'missing'}"
        "function send(k,m){var href='kbo://render/'+k+'/'+encodeURIComponent(String(m)).slice(0,900);try{if(window.chrome&&window.chrome.webview&&window.chrome.webview.postMessage){window.chrome.webview.postMessage(href);return;}}catch(_){}try{location.href=href}catch(_){}}"
        "if(!window.__kboHubRenderErrorInstalled){window.__kboHubRenderErrorInstalled=1;window.addEventListener('error',function(e){send('error',e&&e.message?e.message:'unknown')});}"
        "window.__kboHubReportReady=function(view,mod){setTimeout(function(){var app=q('.app'),panel=q('.panel'),content=q('.content'),rights=q('.rights');"
        "send('ready','view='+view+' mod='+mod+' body='+sz(document.body)+' app='+sz(app)+' panel='+sz(panel)+' content='+sz(content)+' rights='+sz(rights)+' bg='+bg(document.body)+'/'+bg(app)+'/'+bg(panel)+' cards='+document.querySelectorAll('.card').length+' text='+document.body.innerText.length);"
        "},0);};"
        "window.__kboHubReportReady(%d,%d);"
        "})();</script>",
        g_kbo_hub_selected_view,
        g_kbo_hub_selected_mod_subview);
}

WCHAR* kbo_build_webview_hub_html(void)
{
    char* html = (char*)HeapAlloc(GetProcessHeap(), 0, KBO_WEBVIEW_HUB_HTML_CAP);
    if (html == NULL) {
        return NULL;
    }
    html[0] = '\0';
    KBO_PROFILE_BEGIN(profile_webview_build_html);
    KboWindowTextBuffer buffer;
    buffer.data = html;
    buffer.capacity = KBO_WEBVIEW_HUB_HTML_CAP;
    buffer.length = 0;

    KboWebViewHubRenderContext ctx;
    if (!kbo_webview_prepare_hub_render_context(&ctx)) {
        HeapFree(GetProcessHeap(), 0, html);
        KBO_PROFILE_END(profile_webview_build_html, "webview.build_html.total");
        return NULL;
    }

    kbo_window_text_appendf(&buffer,
        "<!doctype html><html><head><meta charset='utf-8'><style>"
        "@font-face{font-family:'KBO Jeju Gothic';font-style:normal;font-weight:400;src:url('%s') format('truetype')}"
        ":root{--bg:#1b1b1d;--header:#1d556c;--nav:#18191b;--active:#1d556c;--panel:#202124;--panel2:#303136;--ink:#fcfcfc;--muted:#9b9b9b;--orange:#de6d1f;--gold:#d6a44b;--line:rgba(255,255,255,.14);--team-primary:%s;--team-secondary:%s;--ui-font:%s}"
        "*{box-sizing:border-box;-webkit-user-select:none;user-select:none;-webkit-user-drag:none}html,body{height:100%;margin:0;overflow:hidden}"
        "body{background:var(--bg);color:var(--ink);font-family:var(--ui-font);font-size:%dpx;cursor:default}a{text-decoration:none;color:inherit;-webkit-user-drag:none}"
        "img{-webkit-user-drag:none;user-select:none}input,textarea,select{-webkit-user-select:auto;user-select:auto}"
        ".ootpRosterTable th[data-sort-type],a,button,.select,.ddItem,.switch,.action,.mainTab,.subTab{cursor:pointer}"
        ".app{height:100%;display:grid;grid-template-rows:64px 1fr;background:#1b1b1d}"
        ".top{background:var(--header);display:flex;align-items:center;justify-content:space-between;padding:0 18px 0 18px;border-bottom:1px solid rgba(255,255,255,.18)}"
        ".identity{display:flex;align-items:center;gap:10px;min-width:0}.logo{width:46px;height:46px;object-fit:contain;filter:drop-shadow(0 1px 1px rgba(0,0,0,.65))}"
        ".brand{font-family:var(--ui-font);font-weight:800;font-size:%dpx;color:#f5f1e7;line-height:1}"
        ".date{font-family:var(--ui-font);font-size:%dpx;font-weight:800;color:#cfd5d6;margin-top:4px;text-transform:uppercase;letter-spacing:0}.brandBlock{min-width:0}"
        ".captainPlate{height:22px;display:flex;align-items:center;justify-content:flex-end;gap:7px;margin-left:auto;min-width:0;max-width:220px;padding:0;border:0;border-radius:0;background:transparent;"
        "box-shadow:none;overflow:hidden;opacity:.88}"
        ".captainName{display:block;min-width:0;color:#d8d8d8;font-size:12px;font-weight:800;line-height:18px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".captainMark{display:inline-flex;align-items:center;justify-content:center;width:18px;height:18px;border:1px solid rgba(214,164,75,.62);border-radius:2px;background:rgba(214,164,75,.10);color:#d6a44b;"
        "font-family:var(--ui-font);font-size:11px;font-weight:900;line-height:18px;flex:none}"
        ".captainBadge{display:inline-flex;align-items:center;justify-content:center;width:16px;height:16px;margin-left:0;padding:0;border:1px solid rgba(214,164,75,.56);border-radius:2px;"
        "background:rgba(214,164,75,.10);color:#d6a44b;font-family:var(--ui-font);font-size:10px;font-weight:900;line-height:16px;vertical-align:middle;box-shadow:none}"
        ".selects{display:flex;gap:10px}"
        ".select{min-width:162px;height:34px;display:flex;align-items:center;justify-content:space-between;gap:8px;padding:0 10px;border:1px solid rgba(255,255,255,.22);border-radius:4px;"
        "background:rgba(0,0,0,.16);color:#e6e6e8;font-family:var(--ui-font);font-weight:800}.select img{width:26px;height:26px;object-fit:contain;flex:none}"
        ".select span:first-of-type{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".dropdown{position:absolute;z-index:20;top:72px;max-height:420px;overflow-y:auto;overflow-x:hidden;background:#242424;border:1px solid #333;border-radius:3px;box-shadow:0 8px 18px rgba(0,0,0,.55);"
        "padding:4px 0;scrollbar-gutter:stable}.leagueMenu{right:190px;width:304px}.teamMenu{right:12px;width:304px}"
        ".ddItem{height:24px;display:flex!important;flex-direction:row!important;align-items:center;justify-content:flex-start;gap:6px;padding:0 10px 0 5px;color:#f2f2f2;font-family:var(--ui-font);"
        "font-size:16px;font-weight:800;line-height:24px;white-space:nowrap}.ddItem:hover,.ddItem.selected{background:#30434b}"
        ".ddLogo{width:20px;height:24px;display:inline-flex;align-items:center;justify-content:center;flex:0 0 20px;overflow:hidden}"
        ".ddLogo img{display:block;width:auto;height:auto;max-width:18px!important;max-height:18px!important;object-fit:contain}"
        ".ddText{display:block;flex:1 1 auto;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".panel{margin:18px 16px 16px;background:var(--panel);border:1px solid var(--panel2);border-radius:5px;min-height:0;height:calc(100%% - 34px);display:grid;grid-template-rows:66px 1fr;overflow:hidden}"
        ".panelHead{background:var(--panel2);padding:10px 15px}.panelHead h1{margin:0;font-family:var(--ui-font);font-size:%dpx;font-weight:800}.panelHead p{margin:3px 0 0;color:var(--muted)}"
        ".content{padding:16px;min-height:0;height:100%%;display:flex;flex-direction:column;overflow:hidden}.rights,.card{flex:1;min-height:0;height:100%%}.rights{display:flex;flex-direction:column;gap:10px}"
        ".card{overflow-y:auto;overflow-x:hidden;border:1px solid #36383c;border-radius:4px;background:#202124;padding:16px}.reportbar{display:none}.muted{color:var(--muted);line-height:1.42}"
        ".help{display:flex;gap:8px;align-items:center;color:#b9b9b9}.help span{background:#242424;border:1px solid #343434;border-radius:4px;padding:4px 8px}"
        ".actions{display:flex;gap:8px;justify-content:flex-end}"
        ".action{display:inline-block;min-width:128px;text-align:center;color:#f4f4f4;border:1px solid #3a3a3a;border-radius:4px;background:#262626;font-family:var(--ui-font);font-weight:800;padding:8px 12px}"
        ".keep{background:#8d4b17;border-color:#c46b22}.release{background:#1d556c;border-color:#2e7896}.toggle{width:52px;text-align:center}"
        ".switch{display:inline-block;width:18px;height:18px;line-height:16px;margin-right:4px;text-align:center;color:#aaa;border:1px solid #343434;border-radius:3px;background:#181818;"
        "font-family:'Segoe UI Symbol',var(--ui-font);font-size:11px;font-weight:400;text-decoration:none}.switch:hover{color:#f0f0f0;border-color:#696969;background:#242424}"
        ".switch.keep,.switch.release{background:#181818;border-color:#343434}.tablewrap{flex:1;min-height:0;overflow-y:auto;overflow-x:hidden;border:1px solid #303030;border-radius:4px;background:#101010}"
        ".modReadme{display:grid!important;grid-template-columns:minmax(0,1.45fr) minmax(260px,.85fr);grid-template-rows:minmax(180px,1fr) minmax(120px,.55fr);gap:12px;height:100%%!important;min-height:0;"
        "overflow:hidden}"
        ".modReadme .card{height:auto!important;min-height:0;overflow-y:auto;overflow-x:hidden;scrollbar-gutter:auto;padding:14px 14px 16px;"
        "background:#242529;border:1px solid #3b3d42;border-radius:5px;box-shadow:inset 0 1px 0 rgba(255,255,255,.05)}"
        ".modContrib{grid-template-rows:minmax(150px,.68fr) minmax(220px,1fr)}"
        ".settingsGrid{display:grid!important;grid-template-columns:minmax(0,1fr);grid-template-rows:minmax(0,1fr);gap:12px;height:100%%!important;min-height:0;align-content:stretch;overflow:hidden!important}"
        ".settingsCard{height:100%%!important;min-height:0;padding:14px 14px 16px;background:#181818;border:1px solid #292929;border-radius:5px;box-shadow:none;overflow-y:auto!important;"
        "overflow-x:hidden!important;scrollbar-gutter:stable!important}.leagueSettingsCard{padding:14px 16px 18px}.settingsSection{margin-top:13px;padding-top:12px;border-top:1px solid #2b2b2b}"
        ".settingsSection:first-of-type{margin-top:0;padding-top:0;border-top:0}.settingsSectionHead{display:flex;align-items:center;justify-content:space-between;margin-bottom:8px}"
        ".settingsSection h3{margin:0;color:#d8d8d8;font-size:13px;font-weight:900;line-height:1.15;text-transform:uppercase}.settingsRows{display:grid;grid-template-columns:minmax(0,1fr);gap:4px}"
        ".leagueSettingsCard .settingRow{grid-template-columns:230px minmax(180px,340px);min-height:30px;margin-top:0}.leagueSettingsCard .settingLabel{color:#b0b0b0;line-height:1.25;white-space:normal}"
        ".leagueSettingsCard .salaryInput{max-width:340px}.flagGroup{margin-top:14px;padding-top:12px;border-top:1px solid #2b2b2b}"
        ".flagGroup h3{margin:0;color:#d8d8d8;font-size:13px;font-weight:900;text-transform:uppercase}.flagGroup p{margin:4px 0 8px;color:#9a9a9a;font-size:12px;line-height:1.35}"
        ".settingRow{display:grid;grid-template-columns:150px minmax(180px,360px);align-items:center;justify-content:start;gap:12px;margin-top:4px}"
        ".settingLabel{color:#9c9c9c;font-size:13px;font-weight:800;white-space:nowrap}"
        ".ootpSelect{width:100%%;height:28px;border:1px solid #3c3c3c;border-radius:4px;background:#202020;color:#f0f0f0;font-family:var(--ui-font);font-size:13px;font-weight:700;padding:0 8px}"
        ".ootpSelect:focus{outline:1px solid #777;outline-offset:0}.modCard{display:flex;flex-direction:column}.modCardMain{grid-row:1/span 2}"
        ".cardTitle{margin:0 0 12px;color:#9c9c9c;font-size:16px;font-weight:900;line-height:1.1;text-transform:uppercase}"
        ".modCard p{margin:0 0 10px;color:#eeeeee;line-height:1.48;white-space:normal;font-size:14px;font-weight:400}.modCard p:last-child{margin-bottom:0}"
        ".modLead{font-size:15px!important;font-weight:400!important;color:#fff!important}.buildList{display:grid;gap:6px;margin-top:2px;align-content:start}"
        ".buildRow{display:grid;grid-template-columns:88px minmax(0,1fr);gap:10px;align-items:baseline;color:#e5e5e5;font-size:13px;line-height:1.35}"
        ".buildLabel{color:#8f8f8f;font-weight:900;white-space:nowrap}.buildValue{font-weight:700;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".githubHero{display:flex;align-items:center;gap:13px;margin:2px 0 12px}.githubLogo{width:58px;height:58px;object-fit:contain;filter:invert(54%%) grayscale(1);opacity:.78;padding:4px;flex:none}"
        ".githubRepo{min-width:0}.githubRepo strong{display:block;font-size:17px;color:#f4f4f4;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".githubRepo span{display:block;margin-top:4px;color:#9e9e9e;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".githubLink{display:inline-flex;align-items:center;justify-content:center;align-self:flex-start;margin-top:auto;min-width:128px;height:32px;border:1px solid #3a3a3a;border-radius:4px;"
        "background:#262626;color:#f4f4f4;font-weight:900}.githubLink:hover{border-color:#686868;background:#303030}@media(max-height:640px){.app{grid-template-rows:78px 34px 34px minmax(0,1fr)!important}"
        ".app.noSubTabs{grid-template-rows:78px 34px minmax(0,1fr)!important}.top{padding:0 18px}.logo{width:58px!important;height:58px!important}.mainTabs{height:32px;margin-top:2px}"
        ".mainTab{height:24px;min-width:92px;padding:0 10px;font-size:13px}.subTabs{height:32px;padding:0 12px;gap:8px}.subTab{height:22px;line-height:21px;padding:0 12px;font-size:13px}"
        ".modReadme{grid-template-rows:minmax(0,1fr)!important}.modReadme .modCard:not(.modCardMain){display:none!important}"
        ".modReadme .modCardMain{grid-row:auto!important}}table{width:100%%;table-layout:fixed;border-collapse:separate;border-spacing:0;font-family:var(--ui-font);font-size:13px}"
        "th{position:sticky;top:0;background:#222;color:#dedede;text-align:left;padding:8px 10px;border-bottom:1px solid #393939;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        "td{padding:7px 10px;border-bottom:1px solid #242424;color:#d8d8d8;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}thead th:first-child{border-top-left-radius:3px}"
        "thead th:last-child{border-top-right-radius:3px}tbody tr:last-child td:first-child{border-bottom-left-radius:3px}tbody tr:last-child td:last-child{border-bottom-right-radius:3px}"
        "tr:nth-child(even) td{background:#151515}tr:hover td,tr.selected td{background:#243b45;color:#fff}.sel{width:48px;color:var(--orange);font-weight:900;white-space:nowrap}"
        ".pname{width:156px;max-width:156px;overflow:hidden;text-overflow:ellipsis;font-weight:800;color:#f3f0e8}.team{width:64px;max-width:64px;text-align:left;font-weight:800;color:#ddd}"
        ".flag{width:44px;text-align:center;cursor:help}.flag .roNatFlag{vertical-align:middle}"
        ".empty{border:1px dashed rgba(255,255,255,.22);border-radius:4px;padding:22px;color:var(--muted);background:#141414;white-space:normal}"
        ".ootpChoice{position:relative;width:100%%;height:28px;color:#f0f0f0;font-family:var(--ui-font);font-size:13px;font-weight:800}"
        ".ootpChoice summary{height:28px;display:flex;align-items:center;justify-content:space-between;gap:8px;padding:0 26px 0 8px;border:1px solid #3c3c3c;border-radius:4px;background:#202020;list-style:none;"
        "cursor:pointer;outline:0}.ootpChoice summary::-webkit-details-marker{display:none}"
        ".ootpChoice summary:after{content:'';position:absolute;right:9px;top:11px;border-left:4px solid transparent;border-right:4px solid transparent;border-top:5px solid #bcbcbc}"
        ".ootpChoice[open] summary{border-color:#777;background:#262626}.ootpChoice[open] summary:after{border-top:0;border-bottom:5px solid #d8d8d8}"
        ".ootpChoice summary span{display:block;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".ootpChoiceMenu{position:absolute;left:0;right:0;top:31px;z-index:50;max-height:220px;overflow-y:auto;overflow-x:hidden;padding:3px 0;border:1px solid #3c3c3c;border-radius:4px;background:#242424;"
        "box-shadow:0 8px 18px rgba(0,0,0,.55);scrollbar-gutter:stable}"
        ".ootpChoiceOption{display:block;height:24px;line-height:24px;padding:0 8px;color:#efefef;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".ootpChoiceOption:hover,.ootpChoiceOption.selected{background:#30434b;color:#fff}.settingRow .ootpChoice{max-width:360px}"
        ".app{height:100vh;grid-template-rows:96px 42px 40px minmax(0,1fr);background:#1b1b1d}.app.noSubTabs{grid-template-rows:96px 42px minmax(0,1fr)}"
        ".top{background:transparent;justify-content:flex-start;gap:18px;padding:0 24px;border-bottom:0}.identity{gap:18px}.logo{width:74px;height:74px}.brand{font-size:%dpx;letter-spacing:0;color:#dcdcdc}"
        ".brand span{color:#8f8f8f}.date{font-size:%dpx;color:#c9c9c9}.date a{color:#c9c9c9}.captainPlate{margin-left:auto}.selects{margin-left:0}"
        ".select{height:30px;min-width:150px;border-color:rgba(255,255,255,.16);background:rgba(0,0,0,.24)}"
        ".mainTabs{display:flex;align-items:center;justify-content:flex-start;align-self:end;height:38px;margin:4px 6px 0;background:var(--team-primary);border-bottom:1px solid rgba(0,0,0,.42);"
        "border-radius:5px 5px 0 0;overflow:hidden;padding:0 16px;gap:14px}"
        ".mainTab{display:flex;align-items:center;justify-content:center;flex:0 0 auto;min-width:86px;max-width:150px;height:26px;padding:0 18px;color:#fff;font-family:var(--ui-font);font-size:15px;"
        "font-weight:900;text-transform:uppercase;border:1px solid transparent;border-radius:4px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".mainTab.active{background:rgba(255,255,255,.14);border-color:rgba(255,255,255,.78);color:#fff}"
        ".subTabs{display:flex;align-items:center;align-self:start;height:38px;margin:0 6px 2px;background:var(--team-secondary);border-bottom:1px solid rgba(0,0,0,.58);border-radius:0 0 5px 5px;"
        "overflow:hidden;padding:0 16px;gap:16px}"
        ".subTab{height:24px;line-height:23px;padding:0 18px;color:#e6e6e6;font-family:var(--ui-font);font-size:15px;font-weight:900;text-transform:uppercase;border:1px solid transparent;border-radius:4px}"
        ".subTab.active{border-color:#d8d8d8;background:rgba(255,255,255,.13);color:#fff}"
        ".panel{height:auto!important;min-height:0!important;margin:12px 14px 14px;align-self:stretch;grid-template-rows:54px minmax(0,1fr);border-radius:4px}.panelHead{padding:8px 14px}"
        ".panelHead h1{font-size:%dpx}.panelHead p{font-size:%dpx}"
        ".panel.dashboardPanel{margin:6px 8px 8px;background:#191a1d;border:1px solid #2f3034;border-radius:4px;box-shadow:none;grid-template-rows:minmax(0,1fr);overflow:hidden}.dashboardPanel .panelHead{display:none}"
        ".dashboardPanel .content{display:block;height:100%%!important;padding:8px;overflow:hidden!important;scrollbar-gutter:auto}"
        ".content{height:auto!important;min-height:0;overflow-y:auto!important;overflow-x:hidden!important;scrollbar-gutter:stable}.rights{height:100%%!important;min-height:0}"
        ".tablewrap{flex:1 1 auto;min-height:0;scrollbar-gutter:stable}.card{height:100%%!important;min-height:0;scrollbar-gutter:stable}.dropdown{top:92px}.leagueMenu{left:140px;right:auto}"
        ".teamMenu{left:140px;right:auto}"
        "%s</style></head><body>",
        ctx.jeju_font_url,
        ctx.team_bar_primary,
        ctx.team_bar_secondary,
        ctx.ui_font_family,
        kbo_hub_skin_article_font_px() - 1,
        kbo_hub_skin_article_font_px() + 8,
        kbo_hub_skin_button_font_px() - 4,
        kbo_hub_skin_article_font_px() + 8,
        kbo_hub_skin_article_font_px() + 10,
        kbo_hub_skin_button_font_px() - 3,
        kbo_hub_skin_article_font_px() + 4,
        kbo_hub_skin_article_font_px() - 2,
        ctx.scrollbar_css);
    kbo_window_text_appendf(&buffer, "<div id='kbo-root'>");
    kbo_webview_append_hub_app(&buffer, &ctx);
    kbo_window_text_appendf(&buffer, "</div>");
    kbo_webview_append_roster_sort_script(&buffer);
    kbo_webview_append_hub_diagnostic_script(&buffer);
    kbo_window_text_appendf(&buffer, "</body></html>");

    KBO_PROFILE_BEGIN(profile_webview_wide);
    int wide_len = 0;
    WCHAR* wide = kbo_webview_utf8_to_wide_heap(html, &wide_len);
    if (kbo_hub_current_mode_is_developer()) {
        kbo_log_runtimef(
            "KBO F2 hub html build finish utf8_bytes=%llu wide_chars=%d wide_allocated=%d",
            (unsigned long long)buffer.length,
            wide_len,
            wide != NULL ? 1 : 0);
    }
    KBO_PROFILE_END(profile_webview_wide, "webview.build_html.utf8_to_wide");
    HeapFree(GetProcessHeap(), 0, html);
    KBO_PROFILE_END(profile_webview_build_html, "webview.build_html.total");
    return wide;
}

WCHAR* kbo_build_webview_hub_update_script(void)
{
    char* app = (char*)HeapAlloc(GetProcessHeap(), 0, KBO_WEBVIEW_HUB_HTML_CAP);
    if (app == NULL) {
        return NULL;
    }
    app[0] = '\0';

    KBO_PROFILE_BEGIN(profile_webview_build_html);
    KboWindowTextBuffer app_buffer;
    app_buffer.data = app;
    app_buffer.capacity = KBO_WEBVIEW_HUB_HTML_CAP;
    app_buffer.length = 0;

    KboWebViewHubRenderContext ctx;
    if (!kbo_webview_prepare_hub_render_context(&ctx)) {
        HeapFree(GetProcessHeap(), 0, app);
        KBO_PROFILE_END(profile_webview_build_html, "webview.build_html.total");
        return NULL;
    }
    kbo_webview_append_hub_app(&app_buffer, &ctx);
    KBO_PROFILE_END(profile_webview_build_html, "webview.build_html.total");

    size_t script_cap = (app_buffer.length * 3u) + KBO_WEBVIEW_HUB_UPDATE_EXTRA_CAP;
    if (script_cap < KBO_WEBVIEW_HUB_UPDATE_EXTRA_CAP || script_cap > (KBO_WEBVIEW_HUB_HTML_CAP * 4u)) {
        script_cap = KBO_WEBVIEW_HUB_HTML_CAP * 4u;
    }
    char* script = (char*)HeapAlloc(GetProcessHeap(), 0, script_cap);
    if (script == NULL) {
        HeapFree(GetProcessHeap(), 0, app);
        return NULL;
    }
    script[0] = '\0';

    KBO_PROFILE_BEGIN(profile_webview_update_script);
    KboWindowTextBuffer script_buffer;
    script_buffer.data = script;
    script_buffer.capacity = script_cap;
    script_buffer.length = 0;
    kbo_window_text_appendf(
        &script_buffer,
        "(function(){var root=document.getElementById('kbo-root');if(!root){return 'missing-root';}"
        "function runEmbeddedScripts(host){var scripts=host.querySelectorAll('script');"
        "for(var i=0;i<scripts.length;i++){var old=scripts[i],s=document.createElement('script');"
        "for(var j=0;j<old.attributes.length;j++){var a=old.attributes[j];s.setAttribute(a.name,a.value);}"
        "s.text=old.textContent||'';old.parentNode.replaceChild(s,old);}}"
        "root.innerHTML=");
    kbo_webview_append_js_string(&script_buffer, app);
    kbo_window_text_appendf(&script_buffer, ";document.documentElement.style.setProperty('--team-primary',");
    kbo_webview_append_js_string(&script_buffer, ctx.team_bar_primary);
    kbo_window_text_appendf(&script_buffer, ");document.documentElement.style.setProperty('--team-secondary',");
    kbo_webview_append_js_string(&script_buffer, ctx.team_bar_secondary);
    kbo_window_text_appendf(&script_buffer, ")");
    kbo_window_text_appendf(
        &script_buffer,
        ";runEmbeddedScripts(root);if(window.__kboHubAfterRender){window.__kboHubAfterRender();}");
    if (kbo_hub_current_mode_is_developer()) {
        kbo_window_text_appendf(
            &script_buffer,
            "if(window.__kboHubReportReady){window.__kboHubReportReady(%d,%d);}",
            g_kbo_hub_selected_view,
            g_kbo_hub_selected_mod_subview);
    }
    kbo_window_text_appendf(&script_buffer, "return 'ok';})();");
    KBO_PROFILE_END(profile_webview_update_script, "webview.build_html.update_script");

    KBO_PROFILE_BEGIN(profile_webview_wide);
    int wide_len = 0;
    WCHAR* wide = kbo_webview_utf8_to_wide_heap(script, &wide_len);
    if (kbo_hub_current_mode_is_developer()) {
        kbo_log_runtimef(
            "KBO F2 hub update script build finish app_bytes=%llu script_bytes=%llu wide_chars=%d wide_allocated=%d",
            (unsigned long long)app_buffer.length,
            (unsigned long long)script_buffer.length,
            wide_len,
            wide != NULL ? 1 : 0);
    }
    KBO_PROFILE_END(profile_webview_wide, "webview.build_html.utf8_to_wide");
    HeapFree(GetProcessHeap(), 0, script);
    HeapFree(GetProcessHeap(), 0, app);
    return wide;
}
