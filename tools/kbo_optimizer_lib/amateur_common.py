"""Common amateur-assignment scoring helpers."""

from .constants import (
    KBO_COLLEGE_LEAGUE_ID,
    KBO_COLLEGE_TEAM_MAX_PLAYERS,
    KBO_HIGH_SCHOOL_LEAGUE_ID,
    KBO_HIGH_SCHOOL_TEAM_MAX_PLAYERS,
)
from .csv_io import to_int as _to_int

def _team_max_players(league_id, requested_max):
    if league_id == KBO_HIGH_SCHOOL_LEAGUE_ID:
        hard_max = KBO_HIGH_SCHOOL_TEAM_MAX_PLAYERS
    elif league_id == KBO_COLLEGE_LEAGUE_ID:
        hard_max = KBO_COLLEGE_TEAM_MAX_PLAYERS
    else:
        hard_max = requested_max
    if requested_max <= 0:
        return hard_max
    return min(requested_max, hard_max) if hard_max > 0 else requested_max

def _candidate_weight(row, target_player_count, player_count_override=None, enforce_capacity=True):
    player_tier = _to_int(row, "player_tier")
    team_tier = _to_int(row, "team_tier")

    target_max_players = _team_max_players(
        _to_int(row, "league_id"),
        _to_int(row, "target_max_players"),
    )
    player_count = _to_int(row, "player_count") if player_count_override is None else player_count_override
    if enforce_capacity and target_max_players > 0 and player_count >= target_max_players:
        return 0

    target_rep = _to_int(row, "target_reputation")
    reputation = _to_int(row, "reputation")
    distance = abs(reputation - target_rep)
    base = max(8, 96 - distance * 4)

    tier_delta = team_tier - player_tier
    tier_gap = abs(tier_delta)
    if tier_delta == 0:
        multiplier = 150
    elif tier_gap == 1 and tier_delta > 0:
        multiplier = 115
    elif tier_gap == 1:
        multiplier = 65
    elif tier_delta > 0:
        multiplier = 35
    else:
        multiplier = 18 if player_tier >= 4 else 25

    weight = max(1, (base * multiplier) // 100)
    if target_player_count >= 0 and player_count > target_player_count:
        overage = player_count - target_player_count
        weight = max(1, weight - overage * 8)
    if enforce_capacity and target_max_players > 0 and player_count >= target_max_players - 2:
        weight = max(1, weight // 2)
    return weight
