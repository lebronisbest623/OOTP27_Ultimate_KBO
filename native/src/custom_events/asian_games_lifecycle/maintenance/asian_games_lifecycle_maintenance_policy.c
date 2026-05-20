#include "asian_games_lifecycle_maintenance_policy.h"
#include "../../../core/dates/core_text_date.h"

static uint32_t kbo_asian_games_lifecycle_date_serial(uint32_t yyyymmdd)
{
    return kbo_date_serial(
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

int kbo_asian_games_restricted_hold_active(uint32_t today_yyyymmdd, uint32_t departure_yyyymmdd, uint32_t return_yyyymmdd)
{
    uint32_t today_serial = kbo_asian_games_lifecycle_date_serial(today_yyyymmdd);
    uint32_t departure_serial = kbo_asian_games_lifecycle_date_serial(departure_yyyymmdd);
    uint32_t return_serial = kbo_asian_games_lifecycle_date_serial(return_yyyymmdd);
    if (today_serial == 0u || departure_serial == 0u || return_serial == 0u) {
        return 0;
    }
    return today_serial >= departure_serial && today_serial < return_serial;
}

int32_t kbo_asian_games_restricted_days_left(uint32_t today_yyyymmdd, uint32_t return_yyyymmdd)
{
    uint32_t today_serial = kbo_asian_games_lifecycle_date_serial(today_yyyymmdd);
    uint32_t return_serial = kbo_asian_games_lifecycle_date_serial(return_yyyymmdd);
    if (today_serial == 0u || return_serial == 0u || return_serial <= today_serial) {
        return 0;
    }
    uint32_t days_left = return_serial - today_serial + 1u;
    return days_left > (uint32_t)INT16_MAX ? INT16_MAX : (int32_t)days_left;
}

int kbo_asian_games_roster_entry_needs_restricted_hold(const KboAsianGamesRosterEntry* entry, uint32_t today_yyyymmdd)
{
    if (entry == NULL
            || entry->player_id == 0u
            || entry->departed == 0u
            || entry->returned != 0u) {
        return 0;
    }
    return kbo_asian_games_restricted_hold_active(today_yyyymmdd, entry->departure_date, entry->return_date);
}
