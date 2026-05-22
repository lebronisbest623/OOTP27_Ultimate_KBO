#include "../internal/intl_established_fa_postscan_internal.h"

void kbo_intl_established_fa_postscan_reset_observed_players(void)
{
    InterlockedExchange((volatile LONG*)&g_kbo_intl_established_fa_observed_player_count, 0);
    memset(
        g_kbo_intl_established_fa_observed_players,
        0,
        sizeof(g_kbo_intl_established_fa_observed_players));
}

void kbo_intl_established_fa_postscan_note_observed_player(uintptr_t player_ptr)
{
    if (player_ptr == 0) {
        return;
    }

    LONG slot = InterlockedIncrement((volatile LONG*)&g_kbo_intl_established_fa_observed_player_count);
    if (slot <= 0) {
        return;
    }
    if (slot <= KBO_INTL_ESTABLISHED_FA_OBSERVED_PLAYER_MAX) {
        g_kbo_intl_established_fa_observed_players[slot - 1] = player_ptr;
    } else if (slot == KBO_INTL_ESTABLISHED_FA_OBSERVED_PLAYER_MAX + 1) {
        kbo_log_runtimef(
            "international established FA observed player capture full max=%d",
            KBO_INTL_ESTABLISHED_FA_OBSERVED_PLAYER_MAX);
    }
}

int kbo_intl_established_fa_postscan_observed_player_count(void)
{
    LONG count = InterlockedCompareExchange(
        (volatile LONG*)&g_kbo_intl_established_fa_observed_player_count,
        0,
        0);
    if (count <= 0) {
        return 0;
    }
    if (count > KBO_INTL_ESTABLISHED_FA_OBSERVED_PLAYER_MAX) {
        return KBO_INTL_ESTABLISHED_FA_OBSERVED_PLAYER_MAX;
    }
    return (int)count;
}

int kbo_intl_established_fa_postscan_player_was_observed(uint8_t* player)
{
    if (player == NULL) {
        return 0;
    }

    uintptr_t player_ptr = (uintptr_t)player;
    int count = kbo_intl_established_fa_postscan_observed_player_count();
    for (int i = 0; i < count; i++) {
        if (g_kbo_intl_established_fa_observed_players[i] == player_ptr) {
            return 1;
        }
    }
    return 0;
}
