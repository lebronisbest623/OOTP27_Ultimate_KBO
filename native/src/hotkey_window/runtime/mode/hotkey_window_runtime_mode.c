#include "../hotkey_window_runtime_shared.h"
#include "hotkey_window_runtime_mode.h"

#define KBO_HUB_DEVELOPER_MODE_FILE "developer_mode.txt"

int kbo_hub_current_mode_is_developer(void)
{
    return g_kbo_hub_mode == KBO_HUB_MODE_DEVELOPER;
}

int kbo_hub_developer_mode_enabled(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_global_data_file(KBO_HUB_DEVELOPER_MODE_FILE, path, sizeof(path))) {
        return 0;
    }

    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

int kbo_hub_requested_mode_available(int mode)
{
    if (mode == KBO_HUB_MODE_DEVELOPER) {
        return kbo_hub_developer_mode_enabled();
    }
    return 1;
}

int kbo_hub_set_mode(int mode)
{
    int next_mode = mode == KBO_HUB_MODE_DEVELOPER
        ? KBO_HUB_MODE_DEVELOPER
        : KBO_HUB_MODE_RELEASE;
    if (!kbo_hub_requested_mode_available(next_mode)) {
        return 0;
    }
    g_kbo_hub_mode = next_mode;
    return 1;
}

int kbo_hub_view_available_for_current_mode(int view)
{
    (void)view;
    return 1;
}

int kbo_hub_mod_subview_available_for_current_mode(int subview)
{
    (void)subview;
    return 1;
}

const char* kbo_hub_mode_log_label(void)
{
    return kbo_hub_current_mode_is_developer() ? "developer" : "release";
}
