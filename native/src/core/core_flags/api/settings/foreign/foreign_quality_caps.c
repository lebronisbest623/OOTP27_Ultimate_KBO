#include "../../flags_api.h"

#include "../../../localappdata/localappdata_reader.h"
#include "../economic/economic_defaults.h"

#include <stdint.h>

static const char* KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_KEYS[KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_COUNT] = {
    "foreign_fa_non_asian_starter_quality_cap",
    "foreign_fa_non_asian_bullpen_quality_cap",
    "foreign_fa_non_asian_pitcher_quality_cap",
    "foreign_fa_non_asian_hitter_quality_cap",
    "foreign_fa_non_asian_catcher_quality_cap"
};

int32_t kbo_clamp_foreign_fa_quality_cap_value(int32_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > KBO_FOREIGN_FA_QUALITY_CAP_MAX) {
        return KBO_FOREIGN_FA_QUALITY_CAP_MAX;
    }
    return value;
}

int32_t kbo_get_foreign_fa_non_asian_quality_cap_value(int index)
{
    if (index < 0 || index >= KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_COUNT) {
        return 0;
    }

    int value = kbo_economic_default_non_asian_quality_cap(index);
    if (!kbo_read_localappdata_setting_int_value(
            KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_KEYS[index],
            &value)) {
        value = kbo_economic_default_non_asian_quality_cap(index);
    }
    return kbo_clamp_foreign_fa_quality_cap_value(value);
}

int kbo_set_foreign_fa_non_asian_quality_cap_value(int index, int32_t value)
{
    if (index < 0 || index >= KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_COUNT) {
        return 0;
    }
    return kbo_write_localappdata_setting_int_value(
        KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_KEYS[index],
        kbo_clamp_foreign_fa_quality_cap_value(value));
}
