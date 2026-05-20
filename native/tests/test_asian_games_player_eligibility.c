#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/custom_events/asian_games/player_eval/asian_games_player_eligibility.h"

static void test_military_exempt_player_remains_selectable(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] = 1u;

    assert(kbo_asian_games_player_status_allows_selection(player));
    assert(kbo_asian_games_player_military_unserved(player) == 0u);

    printf("test_military_exempt_player_remains_selectable: PASS\n");
}

static void test_military_unserved_player_gets_pending_marker(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    assert(kbo_asian_games_player_status_allows_selection(player));
    assert(kbo_asian_games_player_military_unserved(player) == 1u);

    printf("test_military_unserved_player_gets_pending_marker: PASS\n");
}

static void test_active_service_player_is_not_selectable(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = 1u;

    assert(!kbo_asian_games_player_status_allows_selection(player));

    printf("test_active_service_player_is_not_selectable: PASS\n");
}

static void test_roster_blocked_statuses_are_not_selectable(void)
{
    const size_t offsets[] = {
        OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET,
        OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET,
        OOTP27_PLAYER_INJURY_ACTIVE_OFFSET,
        OOTP27_PLAYER_DFA_FLAG_OFFSET,
        OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET,
    };

    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
        memset(player, 0, sizeof(player));
        player[offsets[i]] = 1u;
        assert(!kbo_asian_games_player_status_allows_selection(player));
    }

    printf("test_roster_blocked_statuses_are_not_selectable: PASS\n");
}

int main(void)
{
    test_military_exempt_player_remains_selectable();
    test_military_unserved_player_gets_pending_marker();
    test_active_service_player_is_not_selectable();
    test_roster_blocked_statuses_are_not_selectable();
    printf("All Asian Games player eligibility tests passed.\n");
    return 0;
}
