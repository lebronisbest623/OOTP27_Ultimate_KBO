"""Ranking and scoring helpers for amateur batch assignment."""

from collections import defaultdict

from .constants import (
    BOTTOM_EDGE_BONUS,
    EXTREME_MISMATCH_PENALTY,
    OPTIONAL_SLOT_REPUTATION_WEIGHT,
    RANK_FIT_WEIGHT,
    RANK_MISMATCH_PENALTY,
    TOP_EDGE_BONUS,
)
from .csv_io import to_int as _to_int
from .amateur_roles import _player_position_bucket

def _rank_percentiles(items):
    if not items:
        return {}
    items = sorted(items)
    if len(items) == 1:
        return {items[0][1]: 0.5}
    divisor = len(items) - 1
    return {item_id: index / divisor for index, (_, item_id) in enumerate(items)}

def _player_quality_percentiles(grouped):
    items = []
    for player_id, rows in grouped.items():
        quality_score = max(_to_int(row, "quality_score") for row in rows)
        items.append((quality_score, player_id))
    return _rank_percentiles(items)

def _player_role_quality_percentiles(grouped):
    by_role = defaultdict(list)
    for player_id, rows in grouped.items():
        if not rows:
            continue
        quality_score = max(_to_int(row, "quality_score") for row in rows)
        by_role[_player_position_bucket(rows[0])].append((quality_score, player_id))
    percentiles = {}
    for items in by_role.values():
        percentiles.update(_rank_percentiles(items))
    return percentiles

def _batch_draft_penalties(grouped):
    penalties = {}
    for rows in grouped.values():
        for row in rows:
            team_id = _to_int(row, "team_id")
            if team_id == 0:
                continue
            stages = _to_int(row, "draft_penalty_stages")
            if stages > 0:
                penalties[team_id] = max(stages, penalties.get(team_id, 0))
    return penalties

def _team_reputation_percentiles(grouped):
    teams = {}
    for rows in grouped.values():
        for row in rows:
            team_id = _to_int(row, "team_id")
            if team_id == 0 or _to_int(row, "rejected") != 0:
                continue
            reputation = _to_int(row, "reputation")
            teams[team_id] = max(reputation, teams.get(team_id, reputation))
    return _rank_percentiles((reputation, team_id) for team_id, reputation in teams.items())

def _rank_fit_weight(player_percentile, team_percentile):
    distance = abs(player_percentile - team_percentile)
    fit = max(0.0, 1.0 - distance)
    weight = int(RANK_FIT_WEIGHT * fit * fit * fit)
    weight -= int(RANK_MISMATCH_PENALTY * distance * distance)

    if player_percentile >= 0.85:
        high_team_fit = max(0.0, (team_percentile - 0.65) / 0.35)
        weight += int(TOP_EDGE_BONUS * high_team_fit)
        if team_percentile < 0.50:
            weight -= int(EXTREME_MISMATCH_PENALTY * ((0.50 - team_percentile) / 0.50))
    elif player_percentile <= 0.15:
        low_team_fit = max(0.0, (0.35 - team_percentile) / 0.35)
        weight += int(BOTTOM_EDGE_BONUS * low_team_fit)
        if team_percentile > 0.50:
            weight -= int(EXTREME_MISMATCH_PENALTY * ((team_percentile - 0.50) / 0.50))

    return weight

def _optional_slot_bonus(team_percentile):
    return int(OPTIONAL_SLOT_REPUTATION_WEIGHT * team_percentile * team_percentile)
