#include "../../internal/amateur_assignment_internal.h"

int kbo_amateur_assignment_already_processed(uint32_t player_id, uint32_t team_id)
{
    if (player_id == 0u || team_id == 0u) {
        return 0;
    }

    uint32_t start = kbo_amateur_assignment_processed_hash_key(player_id, team_id)
        & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK;
    for (uint32_t probe = 0; probe < KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX; probe++) {
        KboAmateurAssignmentProcessed* slot =
            &g_kbo_amateur_assignment_processed_hash[(start + probe) & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK];
        if (slot->player_id == 0u) {
            return 0;
        }
        if (slot->player_id == player_id && slot->team_id == team_id) {
            return 1;
        }
    }
    return 1;
}

void kbo_amateur_assignment_mark_processed(uint32_t player_id, uint32_t team_id)
{
    if (player_id == 0u || team_id == 0u) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_amateur_assignment_processed_hash_count, 0, 0)
            > (LONG)(KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX * 3 / 4)) {
        kbo_amateur_assignment_clear_processed_cache();
    }

    uint32_t start = kbo_amateur_assignment_processed_hash_key(player_id, team_id)
        & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK;
    for (uint32_t probe = 0; probe < KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX; probe++) {
        KboAmateurAssignmentProcessed* hash_slot =
            &g_kbo_amateur_assignment_processed_hash[(start + probe) & KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK];
        if (hash_slot->player_id == player_id && hash_slot->team_id == team_id) {
            return;
        }
        if (hash_slot->player_id == 0u) {
            hash_slot->player_id = player_id;
            hash_slot->team_id = team_id;
            InterlockedIncrement(&g_kbo_amateur_assignment_processed_hash_count);
            break;
        }
    }

    LONG slot = InterlockedIncrement(&g_kbo_amateur_assignment_processed_count) - 1;
    if (slot < 0) {
        return;
    }
    if (slot >= KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX) {
        kbo_amateur_assignment_clear_processed_cache();
        slot = InterlockedIncrement(&g_kbo_amateur_assignment_processed_count) - 1;
    }
    g_kbo_amateur_assignment_processed[slot].player_id = player_id;
    g_kbo_amateur_assignment_processed[slot].team_id = team_id;
}

int kbo_amateur_player_is_hitter(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET, sizeof(uint8_t))) {
        return 0;
    }
    return player[OOTP27_PLAYER_POSITION_GROUP_OFFSET] != 1u;
}

uint32_t kbo_amateur_player_assignment_league_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0u;
    }

    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    if (current_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID || current_league_id == KBO_COLLEGE_LEAGUE_ID) {
        return current_league_id;
    }

    uint32_t original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    if (original_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID || original_league_id == KBO_COLLEGE_LEAGUE_ID) {
        return original_league_id;
    }
    return 0u;
}

uint32_t kbo_amateur_player_assignment_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0u;
    }

    uint32_t team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (team_id == 0u) {
        team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    }
    if (team_id == 0u) {
        team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    }
    return team_id;
}

int kbo_amateur_player_position_bucket(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET, sizeof(uint8_t))) {
        return 0;
    }
    switch (player[OOTP27_PLAYER_POSITION_GROUP_OFFSET]) {
    case 1u: return 0; /* P */
    case 2u: return 1; /* C */
    case 3u: return 2; /* 1B */
    case 4u: return 3; /* 2B */
    case 5u: return 4; /* 3B */
    case 6u: return 5; /* SS */
    case 7u: return 6; /* LF */
    case 8u: return 7; /* CF */
    case 9u: return 8; /* RF */
    case 10u: return 2; /* DH is folded into 1B for amateur balancing */
    default:
        return kbo_amateur_player_is_hitter(player) ? 2 : 0;
    }
}

const char* kbo_amateur_position_bucket_label(int bucket)
{
    switch (bucket) {
    case 0: return "P";
    case 1: return "C";
    case 2: return "1B";
    case 3: return "2B";
    case 4: return "3B";
    case 5: return "SS";
    case 6: return "LF";
    case 7: return "CF";
    case 8: return "RF";
    default: return "P";
    }
}

int32_t kbo_amateur_assignment_target_max_players(uint32_t league_id)
{
    const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
    return league_id == KBO_HIGH_SCHOOL_LEAGUE_ID
        ? policy->high_school_assignment_target_max_players
        : policy->college_assignment_target_max_players;
}

void kbo_read_amateur_quality_fields(
    uint8_t* player,
    int16_t* out_overall,
    int16_t* out_talent,
    int16_t* out_ratings,
    int16_t* out_career)
{
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_talent != NULL) { *out_talent = 0; }
    if (out_ratings != NULL) { *out_ratings = 0; }
    if (out_career != NULL) { *out_career = 0; }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET + sizeof(int16_t))) {
        return;
    }
    if (out_overall != NULL) { *out_overall = *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET); }
    if (out_talent != NULL) { *out_talent = *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET); }
    if (out_ratings != NULL) { *out_ratings = *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET); }
    if (out_career != NULL) { *out_career = *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET); }
}

int32_t kbo_amateur_quality_score(uint8_t* player)
{
    int16_t overall = 0;
    int16_t talent = 0;
    int16_t ratings = 0;
    int16_t career = 0;
    kbo_read_amateur_quality_fields(player, &overall, &talent, &ratings, &career);
    return overall + talent + ratings + career;
}

int32_t kbo_amateur_assignment_target_reputation(uint32_t league_id, int32_t quality_score)
{
    const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
    int32_t target = policy->target_reputation_tier0;
    if (quality_score >= policy->quality_tier5_cutoff) {
        target = policy->target_reputation_tier5;
    } else if (quality_score >= policy->quality_tier4_cutoff) {
        target = policy->target_reputation_tier4;
    } else if (quality_score >= policy->quality_tier3_cutoff) {
        target = policy->target_reputation_tier3;
    } else if (quality_score >= policy->quality_tier2_cutoff) {
        target = policy->target_reputation_tier2;
    } else if (quality_score >= policy->quality_tier1_cutoff) {
        target = policy->target_reputation_tier1;
    } else {
        target = policy->target_reputation_tier0;
    }

    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        target += policy->college_target_reputation_adjustment;
    }
    if (target < policy->target_reputation_min) {
        target = policy->target_reputation_min;
    } else if (target > policy->target_reputation_max) {
        target = policy->target_reputation_max;
    }
    int32_t league_reputation_max = league_id == KBO_COLLEGE_LEAGUE_ID
        ? policy->college_reputation_max
        : policy->high_school_reputation_max;
    if (target > league_reputation_max) {
        target = league_reputation_max;
    }
    return target;
}

uint32_t kbo_amateur_assignment_pick_hash(uint32_t player_id, uint32_t league_id, int32_t quality_score, int32_t target)
{
    uint32_t hash = 2166136261u;
    hash = (hash ^ player_id) * 16777619u;
    hash = (hash ^ league_id) * 16777619u;
    hash = (hash ^ (uint32_t)quality_score) * 16777619u;
    hash = (hash ^ (uint32_t)target) * 16777619u;
    hash = (hash ^ 0x9e3779b9u) * 16777619u;
    return hash;
}

int kbo_amateur_assignment_target_rejected(uint32_t league_id, uint32_t team_id)
{
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }
    LONG count = InterlockedCompareExchange(&g_kbo_amateur_assignment_rejected_target_count, 0, 0);
    if (count > KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX) {
        count = KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX;
    }
    for (LONG i = 0; i < count; i++) {
        if (g_kbo_amateur_assignment_rejected_targets[i].player_id == league_id
                && g_kbo_amateur_assignment_rejected_targets[i].team_id == team_id) {
            return 1;
        }
    }
    return 0;
}

void kbo_amateur_assignment_mark_rejected_target(uint32_t league_id, uint32_t team_id)
{
    if (league_id == 0u || team_id == 0u || kbo_amateur_assignment_target_rejected(league_id, team_id)) {
        return;
    }
    LONG slot = InterlockedIncrement(&g_kbo_amateur_assignment_rejected_target_count) - 1;
    if (slot < 0 || slot >= KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX) {
        return;
    }
    g_kbo_amateur_assignment_rejected_targets[slot].player_id = league_id;
    g_kbo_amateur_assignment_rejected_targets[slot].team_id = team_id;
}

int kbo_amateur_assignment_team_tier(uint32_t league_id, uint8_t reputation)
{
    const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
    int32_t rep = (int32_t)reputation;
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        if (rep >= policy->college_team_tier5_rep_min) { return 5; }
        if (rep >= policy->college_team_tier4_rep_min) { return 4; }
        if (rep >= policy->college_team_tier3_rep_min) { return 3; }
        if (rep >= policy->college_team_tier2_rep_min) { return 2; }
        if (rep >= policy->college_team_tier1_rep_min) { return 1; }
        return 0;
    }
    if (rep >= policy->high_school_team_tier5_rep_min) { return 5; }
    if (rep >= policy->high_school_team_tier4_rep_min) { return 4; }
    if (rep >= policy->high_school_team_tier3_rep_min) { return 3; }
    if (rep >= policy->high_school_team_tier2_rep_min) { return 2; }
    if (rep >= policy->high_school_team_tier1_rep_min) { return 1; }
    return 0;
}

int kbo_amateur_assignment_player_tier(uint32_t league_id, int32_t quality_score)
{
    (void)league_id;
    const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
    int tier = 0;
    if (quality_score >= policy->quality_tier5_cutoff) {
        tier = 5;
    } else if (quality_score >= policy->quality_tier4_cutoff) {
        tier = 4;
    } else if (quality_score >= policy->quality_tier3_cutoff) {
        tier = 3;
    } else if (quality_score >= policy->quality_tier2_cutoff) {
        tier = 2;
    } else if (quality_score >= policy->quality_tier1_cutoff) {
        tier = 1;
    } else {
        tier = 0;
    }
    return tier;
}

int kbo_amateur_assignment_tier_allowed(int player_tier, int team_tier)
{
    if (player_tier < 0 || team_tier < 0) {
        return 0;
    }
    return abs(player_tier - team_tier) <= 1;
}

int kbo_amateur_assignment_effective_player_tier(int player_tier, int max_team_tier)
{
    if (max_team_tier < 0) {
        return player_tier;
    }
    if (player_tier > max_team_tier) {
        return max_team_tier;
    }
    return player_tier;
}

int kbo_amateur_player_age_eligible(uint32_t league_id, int16_t age)
{
    const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return age >= policy->college_age_min && age <= policy->college_age_max;
    }
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return age >= policy->high_school_age_min && age <= policy->high_school_age_max;
    }
    return 0;
}

int kbo_amateur_assignment_debug_csv_empty(HANDLE file)
{
    LARGE_INTEGER size;
    if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &size)) {
        return 1;
    }
    return size.QuadPart == 0;
}

void kbo_amateur_assignment_debug_write_text(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE || text == NULL) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
}

void kbo_amateur_assignment_debug_write_csv_text(HANDLE file, const char* text)
{
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    kbo_amateur_assignment_debug_write_text(file, "\"");
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            if (*p == '"') {
                kbo_amateur_assignment_debug_write_text(file, "\"\"");
            } else {
                char ch[2] = { *p, '\0' };
                kbo_amateur_assignment_debug_write_text(file, ch);
            }
        }
    }
    kbo_amateur_assignment_debug_write_text(file, "\"");
}

