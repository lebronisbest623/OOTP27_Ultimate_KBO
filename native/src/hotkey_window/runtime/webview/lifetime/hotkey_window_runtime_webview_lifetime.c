#include "../hotkey_window_webview_internal.h"

int kbo_webview_team_action_allowed(uint32_t team_id, const char* source)
{
    if (kbo_hub_current_mode_is_developer()
            && kbo_get_allow_all_ui_team_actions_setting()) {
        return 1;
    }

    if (team_id != 0 && kbo_team_is_human_controlled(team_id, source)) {
        return 1;
    }

    kbo_log_runtimef(
        "webview team action blocked reason=team_not_human_controlled source=%s team=%u",
        source != NULL ? source : "",
        team_id);
    return 0;
}

HRESULT STDMETHODCALLTYPE kbo_webview_nav_qi(ICoreWebView2NavigationStartingEventHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2NavigationStartingEventHandler)) {
        *ppv = This;
        ICoreWebView2NavigationStartingEventHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE kbo_webview_nav_addref(ICoreWebView2NavigationStartingEventHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewNavHandler*)This)->ref);
}

ULONG STDMETHODCALLTYPE kbo_webview_nav_release(ICoreWebView2NavigationStartingEventHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewNavHandler*)This)->ref);
    if (value < 1) { ((KboWebViewNavHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewNavHandler*)This)->ref;
}

