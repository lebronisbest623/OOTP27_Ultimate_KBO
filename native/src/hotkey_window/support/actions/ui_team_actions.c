#include "ui_team_actions.h"

#include "../../runtime/mode/hotkey_window_runtime_mode.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../team/control/team_human_control.h"

int kbo_hub_ui_team_action_available(uint32_t team_id, const char* source)
{
    if (kbo_hub_current_mode_is_developer()
            && kbo_get_allow_all_ui_team_actions_setting()) {
        return 1;
    }

    return team_id != 0u && kbo_team_is_human_controlled(team_id, source);
}
