#include <assert.h>
#include "../src/custom_events/asian_games_lifecycle/maintenance/asian_games_lifecycle_maintenance_policy.h"

static KboAsianGamesRosterEntry make_departed_entry(void)
{
    KboAsianGamesRosterEntry entry = {0};
    entry.player_id = 1356u;
    entry.departure_date = 20260919u;
    entry.return_date = 20261004u;
    entry.departed = 1u;
    entry.returned = 0u;
    return entry;
}

int main(void)
{
    KboAsianGamesRosterEntry entry = make_departed_entry();

    assert(kbo_asian_games_roster_entry_needs_restricted_hold(&entry, 20260918u) == 0);
    assert(kbo_asian_games_roster_entry_needs_restricted_hold(&entry, 20260919u) == 1);
    assert(kbo_asian_games_roster_entry_needs_restricted_hold(&entry, 20261003u) == 1);
    assert(kbo_asian_games_roster_entry_needs_restricted_hold(&entry, 20261004u) == 0);

    entry.returned = 1u;
    assert(kbo_asian_games_roster_entry_needs_restricted_hold(&entry, 20260924u) == 0);

    entry = make_departed_entry();
    entry.departed = 0u;
    assert(kbo_asian_games_roster_entry_needs_restricted_hold(&entry, 20260924u) == 0);

    assert(kbo_asian_games_restricted_days_left(20260919u, 20261004u) == 16);
    assert(kbo_asian_games_restricted_days_left(20260924u, 20261004u) == 11);
    assert(kbo_asian_games_restricted_days_left(20261003u, 20261004u) == 2);
    assert(kbo_asian_games_restricted_days_left(20261004u, 20261004u) == 0);
    assert(kbo_asian_games_restricted_days_left(20261301u, 20261004u) == 0);

    return 0;
}
