"""Team target-count allocation for amateur batch assignment."""

import math

from .amateur_common import _team_max_players
from .amateur_roles import (
    _batch_source_counts,
    _batch_source_counts_attached_to_rosters,
    _batch_source_position_counts,
    _row_position_counts,
)
from .constants import (
    AMATEUR_MIN_HITTERS_PER_TEAM,
    AMATEUR_MIN_PITCHERS_PER_TEAM,
    KBO_COLLEGE_TEAM_MIN_PLAYERS,
    INCOMING_MAX_AVERAGE_MULTIPLIER,
    INCOMING_REPUTATION_FLOOR_WEIGHT,
    INCOMING_REPUTATION_POWER,
    INCOMING_SLOT_DECAY,
    KBO_COLLEGE_LEAGUE_ID,
    KBO_HIGH_SCHOOL_LEAGUE_ID,
    KBO_HIGH_SCHOOL_TEAM_MIN_PLAYERS,
)
from .csv_io import to_int as _to_int

def _clamp01(value):
    return max(0.0, min(1.0, value))

def _incoming_effective_percentile(info, total_teams):
    stages = max(0, int(info.get("draft_penalty_stages", 0)))
    return _clamp01(float(info.get("percentile", 0.5)) - stages / max(1, total_teams))

def _incoming_reputation_weight(info, total_teams):
    percentile = _incoming_effective_percentile(info, total_teams)
    return INCOMING_REPUTATION_FLOOR_WEIGHT + math.pow(percentile, INCOMING_REPUTATION_POWER)

def _team_min_players(league_id):
    if league_id == KBO_COLLEGE_LEAGUE_ID:
        return KBO_COLLEGE_TEAM_MIN_PLAYERS
    if league_id == KBO_HIGH_SCHOOL_LEAGUE_ID:
        return KBO_HIGH_SCHOOL_TEAM_MIN_PLAYERS
    return 18

def _team_role_minimums(league_id):
    if league_id not in (KBO_COLLEGE_LEAGUE_ID, KBO_HIGH_SCHOOL_LEAGUE_ID):
        return (0, 0)
    min_players = _team_min_players(league_id)
    return (
        min(AMATEUR_MIN_HITTERS_PER_TEAM, min_players),
        min(AMATEUR_MIN_PITCHERS_PER_TEAM, min_players),
    )

def _team_min_fill(info):
    min_players = _team_min_players(info["league_id"])
    min_hitters, min_pitchers = _team_role_minimums(info["league_id"])
    total_deficit = max(0, min_players - info["player_count"])
    hitter_deficit = max(0, min_hitters - info["hitter_count"])
    pitcher_deficit = max(0, min_pitchers - info["pitcher_count"])
    return max(total_deficit, hitter_deficit + pitcher_deficit)

def _make_min_fills_feasible(team_info, total_players):
    min_fill_total = sum(max(0, int(info.get("min_fill", 0))) for info in team_info.values())
    if min_fill_total <= total_players:
        return

    remaining = total_players
    ordered = sorted(
        team_info.items(),
        key=lambda item: (
            item[1]["player_count"],
            item[1]["hitter_count"],
            item[1]["pitcher_count"],
            -item[1]["min_fill"],
            item[0],
        ),
    )
    for _, info in ordered:
        requested = max(0, int(info.get("min_fill", 0)))
        assigned = min(requested, remaining)
        info["min_fill"] = assigned
        remaining -= assigned
    for _, info in ordered:
        if remaining <= 0:
            break
        open_slots = max(0, info["capacity"] - info["min_fill"])
        assigned = min(open_slots, remaining)
        info["min_fill"] += assigned
        remaining -= assigned

def _collect_batch_team_info(grouped, team_percentiles, incoming_batch=False, detailed_roles=False):
    team_info = {}
    subtract_source_counts = not incoming_batch or _batch_source_counts_attached_to_rosters(grouped)
    source_counts = _batch_source_counts(grouped) if subtract_source_counts else {}
    source_position_counts = _batch_source_position_counts(grouped) if subtract_source_counts else {}
    for rows in grouped.values():
        for row in rows:
            team_id = _to_int(row, "team_id")
            if team_id == 0 or _to_int(row, "rejected") != 0 or team_id in team_info:
                continue
            league_id = _to_int(row, "league_id")
            max_players = _team_max_players(league_id, _to_int(row, "target_max_players"))
            raw_player_count = max(0, _to_int(row, "player_count"))
            raw_hitter_count = max(0, min(raw_player_count, _to_int(row, "hitter_count")))
            role_counts = _row_position_counts(row, raw_player_count, raw_hitter_count, detailed_roles)
            outgoing_players, outgoing_hitters = source_counts.get(team_id, (0, 0))
            for role, outgoing_count in source_position_counts.get(team_id, {}).items():
                role_counts[role] = max(0, role_counts.get(role, 0) - outgoing_count)
            if subtract_source_counts:
                player_count = max(0, raw_player_count - outgoing_players)
                hitter_count = max(0, min(player_count, raw_hitter_count - outgoing_hitters))
            else:
                player_count = sum(role_counts.values())
                hitter_count = sum(count for role, count in role_counts.items() if role != "P")
            pitcher_count = max(0, min(player_count, role_counts.get("P", player_count - hitter_count)))
            if not incoming_batch and max_players > 0 and player_count >= max_players:
                continue
            if incoming_batch:
                capacity = len(grouped)
            else:
                capacity = max(0, max_players - player_count) if max_players > 0 else len(grouped)
            if capacity <= 0:
                continue
            min_hitters, min_pitchers = _team_role_minimums(league_id)
            team_info[team_id] = {
                "node": None,
                "role_nodes": {},
                "role_capacities": {},
                "capacity": capacity,
                "target_count": 0,
                "league_id": league_id,
                "player_count": player_count,
                "hitter_count": hitter_count,
                "pitcher_count": pitcher_count,
                "min_hitters": min_hitters,
                "min_pitchers": min_pitchers,
                "role_counts": role_counts,
                "reputation": _to_int(row, "reputation"),
                "percentile": team_percentiles.get(team_id, 0.5),
                "draft_penalty_stages": _to_int(row, "draft_penalty_stages"),
            }
            team_info[team_id]["min_fill"] = max(0, min(capacity, _team_min_fill(team_info[team_id])))
    return team_info

def _allocate_incoming_batch_team_targets(team_info, total_players):
    if not team_info:
        return False
    for info in team_info.values():
        info["target_count"] = 0
    if total_players <= 0:
        return True

    ordered = sorted(team_info.items(), key=lambda item: item[0])
    team_count = len(ordered)
    average = total_players / team_count
    _make_min_fills_feasible(team_info, total_players)
    min_fill_total = sum(max(0, info.get("min_fill", 0)) for _, info in ordered)
    remaining = total_players
    if 0 < min_fill_total <= total_players:
        for _, info in ordered:
            info["target_count"] = max(0, info.get("min_fill", 0))
        remaining -= min_fill_total

    max_per_team = max(
        max((info["target_count"] for _, info in ordered), default=0),
        int(math.ceil(average * INCOMING_MAX_AVERAGE_MULTIPLIER)),
        1,
    )
    weights = {
        team_id: _incoming_reputation_weight(info, team_count)
        for team_id, info in ordered
    }
    while remaining > 0:
        candidates = []
        for team_id, info in ordered:
            if info["target_count"] >= max_per_team:
                continue
            next_slot = info["target_count"] + 1
            score = weights[team_id] / math.pow(next_slot, INCOMING_SLOT_DECAY)
            candidates.append((
                score,
                _incoming_effective_percentile(info, team_count),
                info["reputation"],
                -info["target_count"],
                team_id,
                info,
            ))
        if not candidates:
            return False

        _, _, _, _, _, selected = max(candidates)
        selected["target_count"] += 1
        remaining -= 1
    return sum(info["target_count"] for info in team_info.values()) == total_players

def _allocate_batch_team_targets(team_info, total_players, incoming_batch=False):
    if not team_info:
        return False
    if incoming_batch:
        return _allocate_incoming_batch_team_targets(team_info, total_players)

    total_capacity = sum(info["capacity"] for info in team_info.values())
    if total_capacity < total_players:
        return False

    _make_min_fills_feasible(team_info, total_players)
    for info in team_info.values():
        info["target_count"] = 0

    min_total = sum(info["min_fill"] for info in team_info.values())
    if min_total <= total_players:
        for info in team_info.values():
            info["target_count"] = info["min_fill"]
        remaining = total_players - min_total
    else:
        remaining = total_players

    ordered = sorted(
        team_info.items(),
        key=lambda item: (item[1]["percentile"], item[1]["reputation"], item[0]),
        reverse=True,
    )
    for _, info in ordered:
        if remaining <= 0:
            break
        open_slots = info["capacity"] - info["target_count"]
        if open_slots <= 0:
            continue
        added = min(open_slots, remaining)
        info["target_count"] += added
        remaining -= added

    if remaining > 0:
        return False
    return sum(info["target_count"] for info in team_info.values()) == total_players
