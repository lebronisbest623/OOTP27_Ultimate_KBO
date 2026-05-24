#include "../../hotkey_window_runtime_content.h"

void kbo_webview_append_developer_render_probe_script(KboWindowTextBuffer* buffer)
{
    if (!kbo_hub_current_mode_is_developer()) {
        return;
    }
    kbo_window_text_appendf(
        buffer,
        "<script>(function(){"
        "function q(s){return document.querySelector(s)}"
        "function sz(n){if(!n)return 'missing';var r=n.getBoundingClientRect();return Math.round(r.width)+'x'+Math.round(r.height)}"
        "function bg(n){return n?getComputedStyle(n).backgroundColor:'missing'}"
        "function send(k,m){var href='kbo://render/'+k+'/'+encodeURIComponent(String(m)).slice(0,900);try{if(window.chrome&&window.chrome.webview&&window.chrome.webview.postMessage){window.chrome.webview.postMessage(href);return;}}catch(_){}try{location.href=href}catch(_){}}"
        "window.addEventListener('error',function(e){send('error',e&&e.message?e.message:'unknown')});"
        "setTimeout(function(){var app=q('.app'),panel=q('.panel'),content=q('.content'),rights=q('.rights');"
        "send('ready','view=%d mod=%d body='+sz(document.body)+' app='+sz(app)+' panel='+sz(panel)+' content='+sz(content)+' rights='+sz(rights)+' bg='+bg(document.body)+'/'+bg(app)+'/'+bg(panel)+' cards='+document.querySelectorAll('.card').length+' text='+document.body.innerText.length);"
        "},0);"
        "})();</script>",
        g_kbo_hub_selected_view,
        g_kbo_hub_selected_mod_subview);
}
