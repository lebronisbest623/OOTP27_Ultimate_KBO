#include "../internal/intl_established_fa_postscan_internal.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"

int kbo_intl_established_fa_quality_shaping_enabled(void)
{
    return kbo_get_foreign_fa_quality_cap_enabled_setting();
}

int kbo_intl_established_fa_pitcher_role_is_starter(uint8_t position_role)
{
    return position_role == 11u;
}

int kbo_intl_established_fa_pitcher_role_is_bullpen(uint8_t position_role)
{
    return position_role == 12u || position_role == 13u;
}

int kbo_intl_established_fa_position_is_catcher(uint8_t position_group, uint8_t position_role)
{
    return position_group == 2u || (position_group != 1u && position_role == 2u);
}

int32_t kbo_intl_established_fa_quality_score_cap(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role)
{
    if (asian_quota) {
        if (position_group == 1u) {
            if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                return kbo_economic_default_asian_quality_cap(KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_STARTER);
            }
            if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                return kbo_economic_default_asian_quality_cap(KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_BULLPEN);
            }
            return kbo_economic_default_asian_quality_cap(KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_PITCHER);
        }
        if (kbo_intl_established_fa_position_is_catcher(position_group, position_role)) {
            return kbo_economic_default_asian_quality_cap(KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_CATCHER);
        }
        return kbo_economic_default_asian_quality_cap(KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_HITTER);
    }

    if (position_group == 1u) {
        if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
            return kbo_get_foreign_fa_non_asian_quality_cap_value(
                KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_STARTER);
        }
        if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
            return kbo_get_foreign_fa_non_asian_quality_cap_value(
                KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_BULLPEN);
        }
        return kbo_get_foreign_fa_non_asian_quality_cap_value(
            KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_PITCHER);
    }
    if (kbo_intl_established_fa_position_is_catcher(position_group, position_role)) {
        return kbo_get_foreign_fa_non_asian_quality_cap_value(
            KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_CATCHER);
    }
    return kbo_get_foreign_fa_non_asian_quality_cap_value(
        KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_HITTER);
}

int16_t kbo_intl_established_fa_quality_field_cap(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role)
{
    int32_t score_cap = kbo_intl_established_fa_quality_score_cap(asian_quota, position_group, position_role);
    if (score_cap <= 0) {
        return 0;
    }
    return (int16_t)(score_cap / 100);
}

int16_t kbo_intl_established_fa_scale_value_i16(int16_t value, int32_t original_score, int32_t cap)
{
    if (value <= 0 || original_score <= 0 || cap <= 0 || original_score <= cap) {
        return value;
    }

    int64_t scaled = ((int64_t)value * (int64_t)cap) / (int64_t)original_score;
    if (scaled < 1) {
        scaled = 1;
    }
    if (scaled > value) {
        scaled = value;
    }
    if (scaled > 32767) {
        scaled = 32767;
    }
    return (int16_t)scaled;
}

int16_t kbo_intl_established_fa_clamp_value_i16(int16_t value, int16_t cap)
{
    if (cap <= 0 || value <= cap) {
        return value;
    }
    return cap;
}

int kbo_intl_established_fa_apply_quality_cap(
    uint8_t* player,
    int32_t cap,
    int16_t field_cap,
    int32_t original_score,
    int32_t* out_adjusted_score)
{
    if (out_adjusted_score != NULL) {
        *out_adjusted_score = original_score;
    }
    if (player == NULL || cap <= 0
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    int16_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int16_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int16_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int16_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);

    int needs_score_scale = original_score > cap;
    int needs_field_clamp = field_cap > 0
        && (overall > field_cap || talent > field_cap || ratings > field_cap || career > field_cap);
    if (!needs_score_scale && !needs_field_clamp) {
        return 0;
    }

    int16_t adjusted_overall = needs_score_scale
        ? kbo_intl_established_fa_scale_value_i16(overall, original_score, cap)
        : overall;
    int16_t adjusted_talent = needs_score_scale
        ? kbo_intl_established_fa_scale_value_i16(talent, original_score, cap)
        : talent;
    int16_t adjusted_ratings = needs_score_scale
        ? kbo_intl_established_fa_scale_value_i16(ratings, original_score, cap)
        : ratings;
    int16_t adjusted_career = needs_score_scale
        ? kbo_intl_established_fa_scale_value_i16(career, original_score, cap)
        : career;

    adjusted_overall = kbo_intl_established_fa_clamp_value_i16(adjusted_overall, field_cap);
    adjusted_talent = kbo_intl_established_fa_clamp_value_i16(adjusted_talent, field_cap);
    adjusted_ratings = kbo_intl_established_fa_clamp_value_i16(adjusted_ratings, field_cap);
    adjusted_career = kbo_intl_established_fa_clamp_value_i16(adjusted_career, field_cap);

    *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET) = adjusted_overall;
    *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET) = adjusted_talent;
    *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET) = adjusted_ratings;
    *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET) = adjusted_career;

    int32_t adjusted_score = kbo_foreign_waiver_value_score(player);
    if (out_adjusted_score != NULL) {
        *out_adjusted_score = adjusted_score;
    }
    return adjusted_score < original_score;
}

int kbo_intl_established_fa_postscan_csv_empty(HANDLE file)
{
    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size)) {
        return 0;
    }
    return size.QuadPart == 0;
}

int kbo_intl_established_fa_postscan_open_csv(HANDLE* out_file, char* out_path, size_t out_path_size)
{
    if (out_file == NULL || out_path == NULL || out_path_size == 0) {
        return 0;
    }
    *out_file = INVALID_HANDLE_VALUE;
    out_path[0] = '\0';

    if (!kbo_get_save_scoped_data_file("intl_established_fa_postscan.csv", out_path, out_path_size)) {
        return 0;
    }

    HANDLE file = CreateFileA(
        out_path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    if (kbo_intl_established_fa_postscan_csv_empty(file)) {
        const char* header =
            "date,batch_id,before_count,after_count,expected_count,index,player_id,nation_id,asian_quota,age,"
            "pos_group,pos_role,current_team_id,active_team_id,current_league_id,draft_league_id,"
            "generation_flags,generation_context,generation_grade,generation_special,"
            "overall,talent,ratings,career,value_score\r\n";
        DWORD written = 0;
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    *out_file = file;
    return 1;
}

void kbo_intl_established_fa_postscan_write_csv_row(
    HANDLE file,
    const char* date,
    LONG batch_id,
    int32_t before_count,
    int32_t after_count,
    int32_t expected_count,
    int32_t index,
    uint8_t* player)
{
    if (file == INVALID_HANDLE_VALUE || player == NULL) {
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    int asian_quota = kbo_nation_is_asian_quota_candidate(nation_id);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    uint8_t position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    uint8_t generation_flags = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_FLAGS_OFFSET);
    uint8_t generation_context = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET);
    uint8_t generation_grade = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_GRADE_OFFSET);
    uint8_t generation_special = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_SPECIAL_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    int32_t value_score = kbo_foreign_waiver_value_score(player);

    char line[512] = {0};
    int len = snprintf(
        line,
        sizeof(line),
        "%s,%ld,%d,%d,%d,%d,%u,%u,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d\r\n",
        date != NULL ? date : "00000000",
        batch_id,
        before_count,
        after_count,
        expected_count,
        index,
        player_id,
        nation_id,
        asian_quota,
        (int)age,
        position_group,
        position_role,
        current_team_id,
        active_team_id,
        current_league_id,
        draft_league_id,
        generation_flags,
        generation_context,
        generation_grade,
        generation_special,
        overall,
        talent,
        ratings,
        career,
        value_score);
    if (len <= 0 || (size_t)len >= sizeof(line)) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, line, (DWORD)len, &written, NULL);
}

void kbo_intl_established_fa_postscan_schedule(
    int32_t original_count,
    int32_t expected_count,
    int multiplier,
    uint32_t primary_league_id,
    uint32_t fallback_league_id)
{
    if (expected_count <= 0) {
        return;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t max_player_id = 0u;
    int vector_ready = find_kbo_global_player_vector(&player_vector, &player_count, NULL);
    if (vector_ready) {
        for (int32_t i = 0; i < player_count; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) {
                continue;
            }
            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            if (player_id > max_player_id) {
                max_player_id = player_id;
            }
        }
    }

    uint32_t today = 0u;
    kbo_current_date_tick_latest_published_date(&today);

    LONG batch_id = InterlockedIncrement(&g_kbo_intl_established_fa_postscan.batch_id);
    g_kbo_intl_established_fa_postscan.before_count = vector_ready ? player_count : 0;
    g_kbo_intl_established_fa_postscan.before_max_player_id = max_player_id;
    g_kbo_intl_established_fa_postscan.original_count = original_count;
    g_kbo_intl_established_fa_postscan.expected_count = expected_count;
    g_kbo_intl_established_fa_postscan.multiplier = multiplier;
    g_kbo_intl_established_fa_postscan.primary_league_id = primary_league_id;
    g_kbo_intl_established_fa_postscan.fallback_league_id = fallback_league_id;
    g_kbo_intl_established_fa_postscan.scheduled_date = today;
    g_kbo_intl_established_fa_postscan.due_tick = GetTickCount64() + KBO_INTL_ESTABLISHED_FA_POSTSCAN_DELAY_MS;
    g_kbo_intl_established_fa_postscan.attempts = 0;
    InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 1);
    kbo_intl_established_fa_postscan_persist_state(
        &g_kbo_intl_established_fa_postscan,
        "intl_established_fa_postscan_schedule");

    kbo_log_runtimef(
        "international established FA postscan scheduled batch=%ld league_id=%u/%u before_count=%d before_max_player=%u original=%d multiplier=%d expected=%d date=%u vector_ready=%d",
        batch_id,
        primary_league_id,
        fallback_league_id,
        vector_ready ? player_count : 0,
        max_player_id,
        original_count,
        multiplier,
        expected_count,
        today,
        vector_ready ? 1 : 0);
}

