#include "../hotkey_window_runtime_content.h"

const char* kbo_hub_nav_label(int index)
{
    switch (index) {
    case KBO_HUB_VIEW_MOD_INFO:       return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "모드 정보");
    case KBO_HUB_VIEW_MILITARY:       return kbo_hub_text("\xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80",  "군경팀");
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c",  "보류권");
    case KBO_HUB_VIEW_ASIAN_QUOTA:    return kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98", "외국인 선수");
    case KBO_HUB_VIEW_ASIAN_GAMES:    return kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84", "아시안게임");
    case KBO_HUB_VIEW_UPCOMING_FA:    return "";
    case KBO_HUB_VIEW_FA_CASES:       return kbo_hub_text("FA", "FA");
    case KBO_HUB_VIEW_SETTINGS:       return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95",    "설정");
    case KBO_HUB_VIEW_REPUTATION:     return kbo_hub_text("\xed\x8f\x89\xed\x8c\x90", "평판");
    case KBO_HUB_VIEW_CBT:            return kbo_hub_text("\xea\xb2\xbd\xec\x9f\x81\xea\xb7\xa0\xed\x98\x95\xec\x84\xb8", "경쟁균형세");
    case KBO_HUB_VIEW_FUTURES_LEAGUE: return kbo_hub_text("\xea\xb5\xac\xeb\xa7\xa4", "구매");
    default:                          return "";
    }
}

const char* kbo_hub_foreign_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_FOREIGN_SUBVIEW_ROSTER:
        return kbo_hub_text("\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "로스터");
    case KBO_HUB_FOREIGN_SUBVIEW_RIGHTS:
        return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c", "보류권");
    default:
        return "";
    }
}

const char* kbo_hub_agames_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS:
        return kbo_hub_text("\xeb\x8c\x80\xed\x9a\x8c", "대회");
    case KBO_HUB_AGAMES_SUBVIEW_SCHEDULE:
        return kbo_hub_text("\xec\x9d\xbc\xec\xa0\x95", "일정");
    case KBO_HUB_AGAMES_SUBVIEW_ROSTER:
        return kbo_hub_text("\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "로스터");
    default:
        return "";
    }
}

const char* kbo_hub_fa_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_FA_SUBVIEW_MARKET:
        return kbo_hub_text("\xec\x8b\x9c\xec\x9e\xa5", "시장");
    case KBO_HUB_FA_SUBVIEW_COMPENSATION:
        return kbo_hub_text("\xeb\xb3\xb4\xec\x83\x81", "보상");
    case KBO_HUB_FA_SUBVIEW_RIGHTS_EXERCISE:
        return kbo_hub_text("\xea\xb6\x8c\xeb\xa6\xac \xed\x96\x89\xec\x82\xac", "권리 행사");
    default:
        return "";
    }
}

const char* kbo_hub_military_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_MILITARY_SUBVIEW_ROSTER:
        return kbo_hub_text("\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "로스터");
    case KBO_HUB_MILITARY_SUBVIEW_APPLICANTS:
        return kbo_hub_text("\xec\xa7\x80\xec\x9b\x90\xec\x9e\x90", "지원자");
    case KBO_HUB_MILITARY_SUBVIEW_RESULTS:
        return kbo_hub_text("\xeb\xb0\x9c\xed\x91\x9c \xea\xb2\xb0\xea\xb3\xbc", "발표 결과");
    default:
        return "";
    }
}

const char* kbo_hub_futures_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_FUTURES_SUBVIEW_OFFER:
        return "제안";
    case KBO_HUB_FUTURES_SUBVIEW_PENDING:
        return "대기";
    case KBO_HUB_FUTURES_SUBVIEW_RESULT:
        return "결과";
    default:
        return "";
    }
}

const char* kbo_hub_mod_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_MOD_SUBVIEW_README:
        return kbo_hub_text("\xeb\xa6\xac\xeb\x93\x9c\xeb\xaf\xb8", "리드미");
    case KBO_HUB_MOD_SUBVIEW_LICENSE:
        return kbo_hub_text("\xeb\x9d\xbc\xec\x9d\xb4\xec\x84\xa0\xec\x8a\xa4", "라이선스");
    case KBO_HUB_MOD_SUBVIEW_CREDITS:
        return kbo_hub_text("\xed\x81\xac\xeb\xa0\x88\xeb\x94\xa7", "크레딧");
    case KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS:
        return kbo_hub_text("\xea\xb8\xb0\xec\x97\xac", "기여");
    case KBO_HUB_MOD_SUBVIEW_SETTINGS:
        return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95", "설정");
    default:
        return "";
    }
}

const char* kbo_hub_settings_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_SETTINGS_SUBVIEW_LEAGUE:
        return kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "리그");
    default:
        return "";
    }
}
void kbo_hub_ensure_valid_selection(void);
void kbo_refresh_hotkey_window(void);
void kbo_hub_apply_fixed_window_placement(HWND hwnd, int preserve_position);

