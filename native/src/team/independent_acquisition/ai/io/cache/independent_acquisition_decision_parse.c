#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_decision_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../independent_acquisition_ai_internal.h"
#include "../../../../../core/files/save_paths/core_save_paths.h"

int kbo_independent_acquisition_decision_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INDEPENDENT_ACQUISITION_DECISION_FILE,
        out,
        out_size);
}

int kbo_independent_acquisition_json_u32(
    const char* line,
    const char* key,
    uint32_t* out)
{
    if (line == NULL || key == NULL || out == NULL) {
        return 0;
    }

    char token[64] = {0};
    snprintf(token, sizeof(token), "\"%s\":", key);
    const char* p = strstr(line, token);
    if (p == NULL) {
        return 0;
    }
    p += strlen(token);
    char* end = NULL;
    unsigned long value = strtoul(p, &end, 10);
    if (end == p) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

int kbo_independent_acquisition_parse_decision_line(
    const char* line,
    uint32_t* out_season,
    uint32_t* out_seller_team_id,
    uint32_t* out_player_id,
    uint32_t* out_transferred)
{
    if (line == NULL) {
        return 0;
    }

    uint32_t season = 0u;
    uint32_t seller_team_id = 0u;
    uint32_t player_id = 0u;
    uint32_t transferred = 0u;
    if (!kbo_independent_acquisition_json_u32(line, "season", &season)
            || !kbo_independent_acquisition_json_u32(line, "seller_team_id", &seller_team_id)
            || !kbo_independent_acquisition_json_u32(line, "player_id", &player_id)) {
        return 0;
    }
    kbo_independent_acquisition_json_u32(line, "transferred", &transferred);

    if (out_season != NULL) { *out_season = season; }
    if (out_seller_team_id != NULL) { *out_seller_team_id = seller_team_id; }
    if (out_player_id != NULL) { *out_player_id = player_id; }
    if (out_transferred != NULL) { *out_transferred = transferred; }
    return season != 0u && seller_team_id != 0u && player_id != 0u;
}
