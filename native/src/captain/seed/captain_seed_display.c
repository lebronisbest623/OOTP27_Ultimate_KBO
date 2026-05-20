#include "display/captain_seed_display_resolve.h"
#include "../../core/sync/spin_lock.h"

static KboSpinLock g_kbo_captain_display_cache_lock = KBO_SPIN_LOCK_INIT;
static KboCaptainDisplayCache g_kbo_captain_display_cache;

static void kbo_lock_captain_display_cache(void)
{
    kbo_spin_lock(&g_kbo_captain_display_cache_lock);
}

static void kbo_unlock_captain_display_cache(void)
{
    kbo_spin_unlock(&g_kbo_captain_display_cache_lock);
}

static int kbo_captain_display_cache_matches(
    const KboCaptainDisplayCache* cache,
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    const char* loaded_key)
{
    return cache != NULL
        && cache->season == season
        && cache->league_id == league_id
        && cache->team_id == team_id
        && strcmp(cache->loaded_key, loaded_key != NULL ? loaded_key : "") == 0;
}

static void kbo_copy_captain_display_outputs(
    const KboCaptainDisplayCache* display,
    char* out_name,
    size_t out_name_size,
    uint32_t* out_player_id,
    char* out_source,
    size_t out_source_size)
{
    if (out_name != NULL && out_name_size > 0u) {
        snprintf(out_name, out_name_size, "%s", display != NULL ? display->player_name : "");
    }
    if (out_player_id != NULL) {
        *out_player_id = display != NULL ? display->player_id : 0u;
    }
    if (out_source != NULL && out_source_size > 0u) {
        snprintf(out_source, out_source_size, "%s", display != NULL ? display->source : "");
    }
}


int kbo_get_captain_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    char* out_name,
    size_t out_name_size,
    uint32_t* out_player_id,
    char* out_source,
    size_t out_source_size)
{
    if (out_name != NULL && out_name_size > 0u) {
        out_name[0] = '\0';
    }
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (out_source != NULL && out_source_size > 0u) {
        out_source[0] = '\0';
    }
    if (season == 0u || team_id == 0u) {
        return 0;
    }

    kbo_ensure_captain_seeds_loaded();

    char loaded_key[MAX_PATH * 6] = {0};
    kbo_captain_display_loaded_key(season, loaded_key, sizeof(loaded_key));

    kbo_lock_captain_display_cache();
    if (kbo_captain_display_cache_matches(
            &g_kbo_captain_display_cache,
            season,
            league_id,
            team_id,
            loaded_key)
            && (out_player_id == NULL
                || g_kbo_captain_display_cache.player_id != 0u
                || !g_kbo_captain_display_cache.found)) {
        KboCaptainDisplayCache cached = g_kbo_captain_display_cache;
        kbo_unlock_captain_display_cache();
        kbo_copy_captain_display_outputs(
            &cached,
            out_name,
            out_name_size,
            out_player_id,
            out_source,
            out_source_size);
        return cached.found;
    }
    kbo_unlock_captain_display_cache();

    KboCaptainDisplayCache computed;
    kbo_captain_compute_display_for_team(season, league_id, team_id, loaded_key, &computed);

    kbo_lock_captain_display_cache();
    g_kbo_captain_display_cache = computed;
    kbo_unlock_captain_display_cache();

    kbo_copy_captain_display_outputs(
        &computed,
        out_name,
        out_name_size,
        out_player_id,
        out_source,
        out_source_size);
    return computed.found;
}

int kbo_captain_player_is_team_captain(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }
    uint32_t captain_player_id = 0u;
    if (!kbo_get_captain_for_team(
            season,
            league_id,
            team_id,
            NULL,
            0u,
            &captain_player_id,
            NULL,
            0u)) {
        return 0;
    }
    return captain_player_id != 0u && captain_player_id == player_id;
}
