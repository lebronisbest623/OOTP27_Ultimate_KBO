#include "../../hotkey_window_webview.h"
#include "../../../player_hover/player_hover_manager_probe.h"
#include "../../../../support/assets/paths/ui_image_sources.h"
#include "../../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include <stdarg.h>

#define KBO_PLAYER_TOOLTIP_SCRIPT_BYTES (96u * 1024u)

static void kbo_append_rawf(char* out, size_t out_size, size_t* pos, const char* fmt, ...)
{
    if (out == NULL || out_size == 0u || pos == NULL || *pos >= out_size) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(out + *pos, out_size - *pos, fmt, args);
    va_end(args);
    if (written <= 0) {
        return;
    }

    size_t advance = (size_t)written;
    if (advance >= out_size - *pos) {
        *pos = out_size - 1u;
        out[*pos] = '\0';
        return;
    }

    *pos += advance;
}

static void kbo_append_js_literal(char* out, size_t out_size, size_t* pos, const char* text)
{
    if (out == NULL || out_size == 0u || pos == NULL || text == NULL) {
        return;
    }

    kbo_append_rawf(out, out_size, pos, "'");
    for (const unsigned char* p = (const unsigned char*)text; *p != '\0' && *pos + 8u < out_size; ++p) {
        unsigned char ch = *p;
        if (ch == '\\' || ch == '\'') {
            kbo_append_rawf(out, out_size, pos, "\\%c", (int)ch);
        } else if (ch == '<') {
            kbo_append_rawf(out, out_size, pos, "\\x3c");
        } else if (ch == '>') {
            kbo_append_rawf(out, out_size, pos, "\\x3e");
        } else if (ch == '\n') {
            kbo_append_rawf(out, out_size, pos, "\\n");
        } else if (ch == '\r') {
            kbo_append_rawf(out, out_size, pos, "\\r");
        } else if (ch == '\t') {
            kbo_append_rawf(out, out_size, pos, "\\t");
        } else if (ch < 0x20u) {
            kbo_append_rawf(out, out_size, pos, "\\x%02x", (unsigned int)ch);
        } else {
            out[*pos] = (char)ch;
            *pos += 1u;
            out[*pos] = '\0';
        }
    }
    kbo_append_rawf(out, out_size, pos, "'");
}

static int kbo_webview_execute_utf8_script(const char* script)
{
    if (script == NULL || script[0] == '\0' || g_kbo_webview == NULL) {
        return 0;
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, script, -1, NULL, 0);
    if (wide_len <= 0) {
        wide_len = MultiByteToWideChar(CP_ACP, 0, script, -1, NULL, 0);
        if (wide_len <= 0) {
            return 0;
        }
    }

    WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
    if (wide == NULL) {
        return 0;
    }

    int wrote = MultiByteToWideChar(CP_UTF8, 0, script, -1, wide, wide_len);
    if (wrote <= 0) {
        wrote = MultiByteToWideChar(CP_ACP, 0, script, -1, wide, wide_len);
    }
    HRESULT hr = E_FAIL;
    if (wrote > 0) {
        hr = ICoreWebView2_ExecuteScript(g_kbo_webview, wide, NULL);
    }
    HeapFree(GetProcessHeap(), 0, wide);
    return wrote > 0 && SUCCEEDED(hr);
}

static int kbo_find_player_portrait_src(uint32_t player_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u || player_id == 0u) {
        return 0;
    }
    out[0] = '\0';

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    char portrait_path[MAX_PATH] = {0};
    int written = snprintf(
        portrait_path,
        sizeof(portrait_path),
        "%s\\news\\html\\images\\person_pictures\\player_%u.png",
        save_path,
        player_id);
    if (written <= 0 || (size_t)written >= sizeof(portrait_path)) {
        return 0;
    }

    DWORD attrs = GetFileAttributesA(portrait_path);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
        return 0;
    }

    kbo_webview_copy_image_src(portrait_path, out, out_size);
    return out[0] != '\0';
}

static POINT kbo_player_tooltip_screen_anchor(HWND hwnd, int client_x, int client_y)
{
    POINT pt = {client_x + 14, client_y + 14};
    if (hwnd != NULL && IsWindow(hwnd)) {
        ClientToScreen(hwnd, &pt);
    }
    return pt;
}

static void kbo_show_webview_player_tooltip_shell(
    HWND hwnd,
    const char* player_name,
    int screen_x,
    int screen_y,
    uint32_t hover_seq)
{
    char html[8192] = {0};
    size_t pos = 0u;
    kbo_append_rawf(
        html,
        sizeof(html),
        &pos,
        "<!doctype html><html><head><meta charset='utf-8'><style>"
        "html,body{margin:0;padding:0;background:transparent;overflow:hidden}"
        "#kboPlayerTooltip{box-sizing:border-box;display:inline-block;min-width:220px;min-height:42px;padding:10px 12px;"
        "background:#2c2d30;color:#fff;border:1px solid #f05024;box-shadow:0 8px 22px rgba(0,0,0,.75);"
        "font-family:'Malgun Gothic',sans-serif;font-size:14px;font-weight:800;white-space:nowrap;overflow:hidden}"
        "</style></head><body><div id='kboPlayerTooltip'></div><script>(function(){var hoverSeq=%u;"
        "var tip=document.getElementById('kboPlayerTooltip');tip.textContent=",
        hover_seq);
    kbo_append_js_literal(html, sizeof(html), &pos, player_name != NULL && player_name[0] != '\0' ? player_name : "Player");
    kbo_append_rawf(
        html,
        sizeof(html),
        &pos,
        ";function report(){var r=tip.getBoundingClientRect();var w=Math.ceil(r.width||tip.offsetWidth||220);"
        "var h=Math.ceil(r.height||tip.offsetHeight||42);location.href='kbo-tooltip://resize/'+hoverSeq+'/'+w+'/'+h;}"
        "report();setTimeout(report,0);})();</script></body></html>");
    if (!kbo_show_webview_player_tooltip_popup(hwnd, screen_x, screen_y, hover_seq, html)) {
        kbo_log_runtime_line("webview player tooltip shell popup failed");
    }
}

static void kbo_show_webview_player_tooltip(
    HWND hwnd,
    uint8_t* player,
    uint32_t player_id,
    uint32_t team_id,
    uint32_t league_id,
    const char* player_name,
    int client_x,
    int client_y,
    uint32_t hover_seq)
{
    (void)player;
    (void)league_id;

    POINT screen_anchor = kbo_player_tooltip_screen_anchor(hwnd, client_x, client_y);
    kbo_show_webview_player_tooltip_shell(hwnd, player_name, screen_anchor.x, screen_anchor.y, hover_seq);

    char payload[16000] = {0};
    if (!kbo_capture_ootp_player_tooltip_payload(player_id, payload, sizeof(payload))) {
        kbo_log_runtimef("webview player tooltip skipped reason=capture_failed player=%u", player_id);
        return;
    }

    char portrait_src[32768] = {0};
    kbo_find_player_portrait_src(player_id, portrait_src, sizeof(portrait_src));
    char team_primary[16] = "#f05024";
    char team_secondary[16] = "#303135";
    uint32_t color_team_id = g_kbo_hub_selected_team_id != 0u ? g_kbo_hub_selected_team_id : team_id;
    kbo_hub_copy_team_bar_colors(color_team_id, team_primary, sizeof(team_primary), team_secondary, sizeof(team_secondary));

    char* simple_script = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, KBO_PLAYER_TOOLTIP_SCRIPT_BYTES);
    if (simple_script == NULL) {
        kbo_log_runtimef("webview player tooltip skipped reason=script_alloc_failed player=%u", player_id);
        return;
    }
    size_t simple_pos = 0u;
    kbo_append_rawf(
        simple_script,
        KBO_PLAYER_TOOLTIP_SCRIPT_BYTES,
        &simple_pos,
        "<!doctype html><html><head><meta charset='utf-8'><style>html,body{margin:0;padding:0;background:transparent;overflow:hidden}</style>"
        "</head><body><div id='kboPlayerTooltip'></div><script>(function(){try{var hoverSeq=%u;var payload=",
        hover_seq);
    kbo_append_js_literal(simple_script, KBO_PLAYER_TOOLTIP_SCRIPT_BYTES, &simple_pos, payload);
    kbo_append_rawf(simple_script, KBO_PLAYER_TOOLTIP_SCRIPT_BYTES, &simple_pos, ";var playerName=");
    kbo_append_js_literal(simple_script, KBO_PLAYER_TOOLTIP_SCRIPT_BYTES, &simple_pos, player_name != NULL ? player_name : "");
    kbo_append_rawf(simple_script, KBO_PLAYER_TOOLTIP_SCRIPT_BYTES, &simple_pos, ";var portraitSrc=");
    kbo_append_js_literal(
        simple_script,
        KBO_PLAYER_TOOLTIP_SCRIPT_BYTES,
        &simple_pos,
        portrait_src);
    kbo_append_rawf(simple_script, KBO_PLAYER_TOOLTIP_SCRIPT_BYTES, &simple_pos, ";var teamPrimary=");
    kbo_append_js_literal(simple_script, KBO_PLAYER_TOOLTIP_SCRIPT_BYTES, &simple_pos, team_primary);
    kbo_append_rawf(simple_script, KBO_PLAYER_TOOLTIP_SCRIPT_BYTES, &simple_pos, ";var teamSecondary=");
    kbo_append_js_literal(simple_script, KBO_PLAYER_TOOLTIP_SCRIPT_BYTES, &simple_pos, team_secondary);
    kbo_append_rawf(
        simple_script,
        KBO_PLAYER_TOOLTIP_SCRIPT_BYTES,
        &simple_pos,
        ";var playerId=%u;function esc(s){return String(s==null?'':s).replace(/[&<>\\\"]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','\\\"':'&quot;'}[c]||c;});}"
        "function section(name,next){var a=payload.indexOf(name);if(a<0){return '';}a+=name.length;var b=next?payload.indexOf(next,a):-1;return (b>=0?payload.slice(a,b):payload.slice(a)).trim();}"
        "function clean(text){var full=String(playerName||'').trim();var id=String(playerId||'');var bits={};full.split(/\\s+/).forEach(function(p){if(p){bits[p]=1;}});return text.split(/\\r?\\n/).map(function(s){return s.trim();}).filter(function(s){if(!s){return false;}if(s.indexOf('$APPDATA')>=0||s.indexOf('C:/')>=0||s.indexOf('Documents/Out of the Park')>=0||s.indexOf('person_pictures')>=0){return false;}if(s==='player_popup'||s==='background'||s==='grid_title'||s==='png'||s==='.png'||s==='rb'||s==='player_'){return false;}if(/^player_\\d+(?:\\.png)?$/i.test(s)){return false;}if(id&&s===id){return false;}if(full&&s===full){return false;}if(bits[s]){return false;}return true;});}"
        "function jerseyNumber(lines){for(var i=0;i<lines.length;i++){var m=String(lines[i]).match(/^#(\\d{1,3})$/);if(m){return +m[1];}if(lines[i]==='#'&&i+1<lines.length&&/^\\d{1,3}$/.test(lines[i+1])){return +lines[i+1];}}return 0;}"
        "function ratingRows(lines,mem){var rows=[],seen={},jersey=jerseyNumber(lines);function add(label,a,b){if(!(a>0)){return;}if(b&&!(b>0)){b=0;}if(label!=='OVR'&&label!=='POT'&&a===jersey&&!b){return;}var key=label+'|'+a+'|'+(b||'');if(seen[key]){return;}seen[key]=1;rows.push(b?[label,a,b]:[label,a]);}mem.split(/\\r?\\n/).forEach(function(s){var m=s.trim().match(/^(OVR|POT)\\s+(\\d{1,4})$/);if(m){add(m[1],+m[2],0);}});var labels={OVR:1,POT:1,STU:1,MOV:1,CON:1,STA:1,POW:1,EYE:1,DEF:1};var p='',nums=[],slash=false;function flush(){if(p&&nums.length){if(slash&&nums.length>1){add(p,nums[0],nums[1]);}else{add(p,nums[0],0);}}p='';nums=[];slash=false;}for(var i=0;i<lines.length&&lines[i]!=='Year';i++){var t=lines[i];if(labels[t]){flush();p=t;continue;}if(p&&(t.charAt(0)==='#'||t.indexOf('Age ')===0||t.indexOf('| Age ')>=0)){flush();continue;}if(p&&t==='/'){slash=true;continue;}if(p&&/^\\d+$/.test(t)){var v=+t;if(v<=0){continue;}if(p==='OVR'||p==='POT'){add(p,v,0);p='';nums=[];slash=false;}else if(!nums.length){nums.push(v);}else if(slash){nums.push(v);flush();}else{flush();}}}flush();return rows;}"
        "function statWidth(h){return {Year:47,TM:43,LG:44,G:30,AB:35,HR:30,RBI:34,AVG:45,OBP:45,SLG:45,W:31,L:31,SV:34,IP:47,BB:35,K:35,ERA:48}[h]||34;}"
        "function statTable(lines){var statHeaders={Year:1,TM:1,LG:1,G:1,AB:1,HR:1,RBI:1,AVG:1,OBP:1,SLG:1,W:1,L:1,SV:1,IP:1,BB:1,K:1,ERA:1};function isYear(s){return /^\\d{4}$/.test(s);}function isNum(s){return /^\\d+$/.test(s)||/^\\.\\d+$/.test(s)||/^\\d+\\.\\d+$/.test(s);}function isOrgHeader(h){return h==='TM'||h==='LG';}function dedupeDecimalStats(nums,need){if(nums.length<=need){return nums;}var out=[];for(var n=0;n<nums.length;n++){var v=nums[n];out.push(v);if(String(v).indexOf('.')>=0&&n+1<nums.length&&nums[n+1]===v){n++;}}return out;}var hi=lines.indexOf('Year');if(hi<0){return '';}var headers=[];var i=hi;for(;i<lines.length;i++){if(isYear(lines[i])){break;}if(statHeaders[lines[i]]){headers.push(lines[i]);}}var orgCols=0;for(var h=1;h<headers.length&&isOrgHeader(headers[h]);h++){orgCols++;}var rows=[];while(i<lines.length){if(!isYear(lines[i])){i++;continue;}var year=lines[i++];var seg=[];while(i<lines.length&&!isYear(lines[i])){seg.push(lines[i++]);}var org=[];var j=0;for(;j<seg.length&&org.length<orgCols;j++){var ot=seg[j];if(isNum(ot)||ot==='.'||isYear(ot)){break;}org.push(ot);}var nums=[];for(;j<seg.length;j++){var t=seg[j];if(t==='.'&&nums.length&&j+1<seg.length&&/^\\d+$/.test(seg[j+1])){nums[nums.length-1]+='.'+seg[j+1];j++;continue;}if(t==='.'&&j+1<seg.length&&/^\\d+$/.test(seg[j+1])){nums.push('.'+seg[j+1]);j++;continue;}if(isNum(t)){nums.push(t);}}var need=Math.max(0,headers.length-1-orgCols);nums=dedupeDecimalStats(nums,need);var r=[year];for(var oc=0;oc<orgCols;oc++){r.push(org[oc]||'-');}r=r.concat(nums.slice(0,need));while(r.length<headers.length){r.push('');}if(r.length>1+orgCols){rows.push(r);}}rows=rows.slice(-3);if(!headers.length||!rows.length){return '';}return '<table><colgroup>'+headers.map(function(h){return '<col style=\"width:'+statWidth(h)+'px\">';}).join('')+'</colgroup><thead><tr>'+headers.map(function(h){return '<th>'+esc(h)+'</th>';}).join('')+'</tr></thead><tbody>'+rows.map(function(r){return '<tr>'+headers.map(function(h,idx){return '<td>'+esc(r[idx]||'')+'</td>';}).join('')+'</tr>';}).join('')+'</tbody></table>';}"
        "function maxRating(rows){var m=0;for(var i=0;i<rows.length;i++){m=Math.max(m,+rows[i][1]||0,+rows[i][2]||0);}return m>100?250:100;}"
        "function visualRating(v){return Math.max(0,Math.min(100,+v||0));}"
        "function ratingColor(v){var n=visualRating(v);return n>=90?'#00a8ff':n>=80?'#13b7c9':n>=50?'#2fc51a':n>=35?'#ffd22d':n>=20?'#f08a1f':'#d84a35';}"
        "function overallColor(v){var n=visualRating(v);return n>=70?'#00a8ff':n>=50?'#2fc51a':n>=40?'#ffd22d':n>=25?'#f08a1f':'#d84a35';}"
        "function barPct(v){return Math.max(4,Math.min(100,Math.round(visualRating(v))));}"
        "function valueHtml(r){var ca=ratingColor(r[1]);if(r[2]){var cb=ratingColor(r[2]);return '<b><span style=\"color:'+ca+'\">'+esc(r[1])+'</span><i> / </i><span style=\"color:'+cb+'\">'+esc(r[2])+'</span></b>';}return '<b><span style=\"color:'+ca+'\">'+esc(r[1])+'</span></b>';}"
        "function barHtml(r){var a=barPct(r[1]);var ca=ratingColor(r[1]);if(r[2]){var b=barPct(r[2]);var cb=ratingColor(r[2]);var max=Math.max(a,b);return '<div class=\"kboBar\"><em class=\"kboBarFill kboBarPotential\" style=\"width:'+max+'%%;background:'+cb+'\"></em><em class=\"kboBarFill\" style=\"width:'+a+'%%;background:'+ca+'\"></em></div>';}return '<div class=\"kboBar\"><em class=\"kboBarFill\" style=\"width:'+a+'%%;background:'+ca+'\"></em></div>';}"
        "function row(r){return '<div class=\"kboRatingRow\"><span>'+esc(r[0])+'</span>'+valueHtml(r)+barHtml(r)+'</div>';}"
        "function overallRow(ratings){var o=null,p=null,rest=[];for(var i=0;i<ratings.length;i++){if(ratings[i][0]==='OVR'&&!o){o=ratings[i];}else if(ratings[i][0]==='POT'&&!p){p=ratings[i];}else{rest.push(ratings[i]);}}var h='';if(o||p){h='<div class=\"kboOverall\"><span>OVR</span><b style=\"color:'+overallColor(o?o[1]:0)+'\">'+esc(o?o[1]:'')+'</b><span>POT</span><b style=\"color:'+overallColor(p?p[1]:0)+'\">'+esc(p?p[1]:'')+'</b></div>';}return {html:h,rest:rest};}"
        "function firstTeam(lines){var y=lines.indexOf('Year');for(var i=y+1;i<lines.length;i++){if(/^\\d{4}$/.test(lines[i])){return lines[i+1]||'';}}return '';}"
        "function titleLine(lines){var pos='';for(var i=0;i<lines.length&&lines[i]!=='OVR';i++){if(/^(SP|RP|CL|C|1B|2B|3B|SS|LF|CF|RF|DH)$/.test(lines[i])){pos=lines[i];break;}}var tm=firstTeam(lines);return (pos?pos+' ':'')+(playerName||'Player')+(tm?', '+tm:'');}"
        "var cap=section('render text append capture','memory display ratings');var mem=section('memory display ratings','');var lines=clean(cap);var allRatings=ratingRows(lines,mem);var ratingScale=maxRating(allRatings);var grouped=overallRow(allRatings);var ratings=grouped.rest.slice(0,4);var stats=statTable(lines);var title=titleLine(lines);"
        "var meta='';for(var i=0;i<lines.length;i++){if(lines[i].indexOf('| Age ')>=0||lines[i].indexOf('Age ')===0){meta=lines[i];break;}}"
        "var tip=document.getElementById('kboPlayerTooltip');"
        "var portrait=portraitSrc?'<img class=\"kboPortrait\" decoding=\"async\" src=\"'+esc(portraitSrc)+'\" onerror=\\'this.style.visibility=\"hidden\"\\'>':'<div class=\"kboPortrait\"></div>';"
        "tip.innerHTML='<div class=\"kboTop\">'+portrait+'<div class=\"kboInfo\"><div class=\"kboName\">'+esc(title)+'</div>'+grouped.html+'<div class=\"kboRatings\">'+ratings.map(row).join('')+'</div></div></div><div class=\"kboMeta\">'+esc(meta)+'</div>'+stats;"
        "function kboSendCommand(href){try{location.href=href;}catch(_){}}"
        "tip.style.cssText='width:430px;height:auto;background:#303135;color:#f3f3f3;border:1px solid #151515;box-shadow:0 8px 22px rgba(0,0,0,.72);font-family:\\'Malgun Gothic\\',sans-serif;font-size:13px;pointer-events:none;text-align:left;display:block;overflow:hidden';"
        "var css=document.getElementById('kboPlayerTipStyle');if(!css){css=document.createElement('style');css.id='kboPlayerTipStyle';document.head.appendChild(css);}css.textContent='#kboPlayerTooltip *{box-sizing:border-box}#kboPlayerTooltip .kboTop{display:grid;grid-template-columns:106px 1fr;gap:8px;height:137px;padding:7px 7px 0;background:linear-gradient(#3c3d41,#333438)}#kboPlayerTooltip .kboPortrait{width:98px;height:106px;object-fit:contain;object-position:center bottom;align-self:end;background:transparent}#kboPlayerTooltip .kboInfo{min-width:0;padding-right:3px;overflow:hidden}#kboPlayerTooltip .kboName{text-align:center;color:#aeb1b8;font-weight:800;font-size:14px;height:20px;line-height:20px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}#kboPlayerTooltip .kboOverall{height:19px;display:grid;grid-template-columns:32px 54px 32px 1fr;gap:6px;align-items:center;color:#a8abb0;font-weight:800}#kboPlayerTooltip .kboOverall b{text-align:right;font-weight:900}#kboPlayerTooltip .kboRatings{display:grid;grid-template-rows:repeat(4,19px);gap:2px;margin-top:1px;overflow:hidden}#kboPlayerTooltip .kboRatingRow{height:19px;display:grid;grid-template-columns:36px 58px minmax(132px,1fr);gap:6px;align-items:center;color:#b8bac0;font-weight:800}#kboPlayerTooltip .kboRatingRow b{text-align:right;font-weight:900;white-space:nowrap;overflow:hidden}#kboPlayerTooltip .kboRatingRow b i{font-style:normal;color:#d9d9d9;font-weight:800}#kboPlayerTooltip .kboBar{position:relative;display:block!important;width:100%%;height:8px;background:#292a2d!important;border-radius:5px;overflow:hidden;box-shadow:inset 0 1px 0 rgba(255,255,255,.05)}#kboPlayerTooltip .kboBarFill{position:absolute;left:0;top:0;display:block!important;height:100%%;min-width:4px;border-radius:5px;box-shadow:inset 0 1px 0 rgba(255,255,255,.18)}#kboPlayerTooltip .kboBarPotential{z-index:1}#kboPlayerTooltip .kboBarFill:not(.kboBarPotential){z-index:2}#kboPlayerTooltip .kboMeta{height:25px;line-height:25px;text-align:center;background:#303135;color:#fff;font-weight:800;font-size:14px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}#kboPlayerTooltip table{width:100%%;border-collapse:collapse;table-layout:fixed;font-size:13px;line-height:1}#kboPlayerTooltip th{height:22px;background:'+teamPrimary+';color:#fff;text-align:left;padding:0 3px;font-weight:900;white-space:nowrap;overflow:hidden;text-overflow:clip}#kboPlayerTooltip td{height:22px;background:#303135;color:#f4f4f4;padding:0 3px;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:clip}#kboPlayerTooltip tbody tr:nth-child(even) td{background:#393a3e}#kboPlayerTooltip th:nth-child(n+4),#kboPlayerTooltip td:nth-child(n+4){text-align:right}';"
        "var barCount=tip.querySelectorAll('.kboBar').length;var fillCount=tip.querySelectorAll('.kboBarFill').length;var firstPct=ratings.length?barPct(ratings[0][1]):0;"
        "function report(){var r=tip.getBoundingClientRect();var w=Math.ceil(r.width||tip.offsetWidth||430);var h=Math.ceil(r.height||tip.offsetHeight||220);"
        "kboSendCommand('kbo-tooltip://resize/'+hoverSeq+'/'+w+'/'+h+'/'+playerId+'/'+ratings.length+'/'+barCount+'/'+fillCount+'/'+ratingScale+'/'+firstPct);}"
        "report();setTimeout(report,0);"
        "}catch(e){var tip=document.getElementById('kboPlayerTooltip');if(tip){tip.textContent='tooltip render error: '+e.message;tip.style.cssText='width:430px;min-height:42px;padding:10px;background:#4b1f1f;color:#fff;font-family:\\'Malgun Gothic\\',sans-serif;font-size:13px';}try{location.href='kbo-tooltip://resize/'+hoverSeq+'/430/42';}catch(_){}}})();</script></body></html>",
        player_id);

    if (simple_pos + 1u >= KBO_PLAYER_TOOLTIP_SCRIPT_BYTES) {
        HeapFree(GetProcessHeap(), 0, simple_script);
        kbo_log_runtimef("webview player tooltip skipped reason=script_full player=%u", player_id);
        return;
    }

    if (!kbo_show_webview_player_tooltip_popup(hwnd, screen_anchor.x, screen_anchor.y, hover_seq, simple_script)) {
        kbo_log_runtimef("webview player tooltip popup failed player=%u", player_id);
    }
    HeapFree(GetProcessHeap(), 0, simple_script);
}

static void kbo_hide_webview_player_tooltip(void)
{
    kbo_webview_execute_utf8_script(
        "(function(){var tip=document.getElementById('kboPlayerTooltip');if(tip){tip.style.display='none';}})();");
}

static int kbo_parse_u32_segment(const char** cursor, uint32_t* out)
{
    if (cursor == NULL || *cursor == NULL || out == NULL) {
        return 0;
    }

    char* end = NULL;
    unsigned long value = strtoul(*cursor, &end, 10);
    if (end == *cursor || value > 0xfffffffful) {
        return 0;
    }

    *out = (uint32_t)value;
    *cursor = end;
    if (**cursor == '/') {
        *cursor += 1;
    }
    return 1;
}

static void kbo_request_player_hover_payload(HWND hwnd, uint32_t player_id, int client_x, int client_y, uint32_t hover_seq)
{
    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    uint8_t* player = kbo_find_player_by_id(player_id, &team_id, &league_id);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        kbo_log_runtimef(
            "webview player hover ignored reason=player_not_found player=%u x=%d y=%d",
            player_id,
            client_x,
            client_y);
        return;
    }

    char player_name[128] = {0};
    kbo_copy_player_display_name(player, player_name, sizeof(player_name));

    kbo_log_runtimef(
        "webview player hover payload player=%u seq=%u name=\"%s\" team=%u league=%u hwnd=%p x=%d y=%d manager=%p",
        player_id,
        hover_seq,
        player_name,
        team_id,
        league_id,
        hwnd,
        client_x,
        client_y,
        (void*)kbo_player_hover_manager_ptr());

    kbo_show_webview_player_tooltip(hwnd, player, player_id, team_id, league_id, player_name, client_x, client_y, hover_seq);
}

static void kbo_hide_player_hover_payload(HWND hwnd, uint32_t player_id)
{
    (void)hwnd;
    kbo_clear_ootp_player_hover_popup(player_id);
    kbo_hide_webview_player_tooltip_popup(0u);
    kbo_hide_webview_player_tooltip();
    kbo_log_runtimef("webview player hover hide player=%u", player_id);
}

int kbo_webview_handle_player_hover_command(const char* cmd, HWND hwnd)
{
    if (cmd == NULL) {
        return 0;
    }

    if (strncmp(cmd, "player-hover/debug/", 19) == 0) {
        const char* cursor = cmd + 19;
        uint32_t hover_seq = 0u;
        uint32_t player_id = 0u;
        uint32_t ratings = 0u;
        uint32_t bars = 0u;
        uint32_t fills = 0u;
        uint32_t scale = 0u;
        uint32_t first_pct = 0u;
        (void)hwnd;
        if (!kbo_parse_u32_segment(&cursor, &hover_seq)
                || !kbo_parse_u32_segment(&cursor, &player_id)) {
            return 1;
        }
        (void)kbo_parse_u32_segment(&cursor, &ratings);
        (void)kbo_parse_u32_segment(&cursor, &bars);
        (void)kbo_parse_u32_segment(&cursor, &fills);
        (void)kbo_parse_u32_segment(&cursor, &scale);
        (void)kbo_parse_u32_segment(&cursor, &first_pct);
        kbo_log_runtimef(
            "webview player tooltip render debug seq=%u player=%u ratings=%u bars=%u fills=%u scale=%u first_pct=%u",
            hover_seq,
            player_id,
            ratings,
            bars,
            fills,
            scale,
            first_pct);
        return 1;
    }

    if (strncmp(cmd, "player-hover/show/", 18) == 0) {
        const char* cursor = cmd + 18;
        uint32_t player_id = 0u;
        uint32_t client_x = 0u;
        uint32_t client_y = 0u;
        uint32_t hover_seq = 0u;
        if (!kbo_parse_u32_segment(&cursor, &player_id)
                || !kbo_parse_u32_segment(&cursor, &client_x)
                || !kbo_parse_u32_segment(&cursor, &client_y)
                || player_id == 0u) {
            return 1;
        }
        (void)kbo_parse_u32_segment(&cursor, &hover_seq);

        kbo_request_player_hover_payload(hwnd, player_id, (int)client_x, (int)client_y, hover_seq);
        return 1;
    }

    if (strncmp(cmd, "player-hover/hide/", 18) == 0) {
        const char* cursor = cmd + 18;
        uint32_t player_id = 0u;
        if (kbo_parse_u32_segment(&cursor, &player_id) && player_id != 0u) {
            kbo_hide_player_hover_payload(hwnd, player_id);
        }
        return 1;
    }

    return 0;
}
