#!/usr/bin/env python3
import csv
import math
import sys
from collections import defaultdict

from ortools.graph.python import min_cost_flow
from ortools.sat.python import cp_model


KBO_COLLEGE_LEAGUE_ID = 201
KBO_HIGH_SCHOOL_LEAGUE_ID = 203
KBO_COLLEGE_TEAM_MAX_PLAYERS = 40
KBO_HIGH_SCHOOL_TEAM_MAX_PLAYERS = 34
MIN_TEAM_FILL_BONUS = 80000
RANK_FIT_WEIGHT = 12000
RANK_MISMATCH_PENALTY = 4200
ASSORTATIVE_RANK_WEIGHT = 100000
TOP_EDGE_BONUS = 2800
BOTTOM_EDGE_BONUS = 2200
EXTREME_MISMATCH_PENALTY = 3600
OPTIONAL_SLOT_REPUTATION_WEIGHT = 30000
INCOMING_REPUTATION_FLOOR_WEIGHT = 0.03
INCOMING_REPUTATION_POWER = 3.0
INCOMING_SLOT_DECAY = 1.15
INCOMING_MAX_AVERAGE_MULTIPLIER = 2.65
AMATEUR_ROLE_BALANCE_TOLERANCES = (0.10, 0.15, 0.22, 0.50)
AMATEUR_MIN_HITTER_SHARE = 0.25
AMATEUR_MAX_HITTER_SHARE = 0.75
AMATEUR_POSITION_BUCKETS = ("P", "C", "1B", "2B", "3B", "SS", "LF", "CF", "RF")
ASIAN_GAMES_ROSTER_SIZE = 24
ASIAN_GAMES_ROLE_TARGETS = {
    "P": 11,
    "C": 2,
    "IF": 6,
    "OF": 5,
}
ASIAN_GAMES_ROLE_MINIMUMS = {
    "P": 9,
    "C": 2,
    "IF": 5,
    "OF": 4,
}
ASIAN_GAMES_ROLE_MAXIMUMS = {
    "P": 13,
    "C": 3,
    "IF": 8,
    "OF": 7,
}
ASIAN_GAMES_MAX_WILDCARDS = 3
ASIAN_GAMES_TEAM_MAX_PLAYERS = 3
ASIAN_GAMES_REQUIRED_ORG_BONUS = 250000
ASIAN_GAMES_ROLE_DEVIATION_PENALTY = 160000
MILITARY_SELECTION_TEAM_SPREAD_BONUS = 250
FA_PROTECTION_ROLE_BONUS = 25


def _to_int(row, key, default=0):
    try:
        return int(row.get(key, default) or default)
    except ValueError:
        return default


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


def _clamp01(value):
    return max(0.0, min(1.0, value))


def _incoming_effective_percentile(info, total_teams):
    stages = max(0, int(info.get("draft_penalty_stages", 0)))
    return _clamp01(float(info.get("percentile", 0.5)) - stages / max(1, total_teams))


def _incoming_reputation_weight(info, total_teams):
    percentile = _incoming_effective_percentile(info, total_teams)
    return INCOMING_REPUTATION_FLOOR_WEIGHT + math.pow(percentile, INCOMING_REPUTATION_POWER)


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


def _batch_is_incoming(grouped):
    explicit_modes = set()
    for rows in grouped.values():
        if not rows:
            continue
        for row in rows:
            mode = (row.get("batch_mode") or "").strip().lower()
            if mode:
                explicit_modes.add(mode)

    if any(mode in ("incoming", "deferred_add", "freshman") for mode in explicit_modes):
        return True
    if any(mode in ("roster", "existing_roster", "seed") for mode in explicit_modes):
        return False

    return False


def _normalize_position_bucket(value):
    bucket = (value or "").strip().upper()
    aliases = {
        "PITCHER": "P",
        "CATCHER": "C",
        "FIRST_BASE": "1B",
        "FIRSTBASE": "1B",
        "SECOND_BASE": "2B",
        "SECONDBASE": "2B",
        "THIRD_BASE": "3B",
        "THIRDBASE": "3B",
        "SHORTSTOP": "SS",
        "LEFT_FIELD": "LF",
        "LEFTFIELD": "LF",
        "CENTER_FIELD": "CF",
        "CENTERFIELD": "CF",
        "CENTRE_FIELD": "CF",
        "CENTREFIELD": "CF",
        "RIGHT_FIELD": "RF",
        "RIGHTFIELD": "RF",
        "DESIGNATED_HITTER": "1B",
        "DESIGNATEDHITTER": "1B",
        "DH": "1B",
        "IF": "",
        "OF": "",
    }
    bucket = aliases.get(bucket, bucket)
    return bucket if bucket in AMATEUR_POSITION_BUCKETS else ""


def _player_position_bucket(row):
    bucket = _normalize_position_bucket(row.get("role_bucket") or "")
    if bucket:
        return bucket

    position_group = _to_int(row, "position_group", -1)
    if position_group == 1:
        return "P"
    if position_group == 2:
        return "C"
    if position_group == 3:
        return "1B"
    if position_group == 4:
        return "2B"
    if position_group == 5:
        return "3B"
    if position_group == 6:
        return "SS"
    if position_group == 7:
        return "LF"
    if position_group == 8:
        return "CF"
    if position_group == 9:
        return "RF"
    if position_group == 10:
        return "1B"
    return ""


def _batch_has_detailed_position_buckets(grouped):
    for rows in grouped.values():
        if not rows:
            continue
        row = rows[0]
        if row.get("role_bucket") or row.get("position_group"):
            bucket = _player_position_bucket(row)
            if bucket in AMATEUR_POSITION_BUCKETS and bucket != "P":
                return True
        for key in (
            "catcher_count",
            "first_base_count",
            "second_base_count",
            "third_base_count",
            "shortstop_count",
            "left_field_count",
            "center_field_count",
            "right_field_count",
            "designated_hitter_count",
        ):
            if key in row:
                return True
    return False


def _batch_hitter_share(grouped, incoming_batch=False):
    hitter_players = 0
    total_players = 0
    teams = {}
    source_counts = {} if incoming_batch else _batch_source_counts(grouped)
    for rows in grouped.values():
        if not rows:
            continue
        total_players += 1
        if _to_int(rows[0], "is_hitter") != 0:
            hitter_players += 1
        for row in rows:
            team_id = _to_int(row, "team_id")
            if team_id == 0 or _to_int(row, "rejected") != 0:
                continue
            outgoing_players, outgoing_hitters = source_counts.get(team_id, (0, 0))
            player_count = max(0, _to_int(row, "player_count") - outgoing_players)
            hitter_count = max(0, _to_int(row, "hitter_count") - outgoing_hitters)
            hitter_count = min(player_count, hitter_count)
            teams.setdefault(
                team_id,
                (player_count, hitter_count),
            )

    existing_players = sum(player_count for player_count, _ in teams.values())
    existing_hitters = sum(hitter_count for _, hitter_count in teams.values())
    denominator = existing_players + total_players
    if denominator <= 0:
        return 0.5
    share = (existing_hitters + hitter_players) / denominator
    return min(AMATEUR_MAX_HITTER_SHARE, max(AMATEUR_MIN_HITTER_SHARE, share))


def _batch_source_counts(grouped):
    counts = defaultdict(lambda: [0, 0])
    for rows in grouped.values():
        if not rows:
            continue
        source_team_id = _to_int(rows[0], "current_team_id")
        if source_team_id == 0:
            continue
        counts[source_team_id][0] += 1
        if _to_int(rows[0], "is_hitter") != 0:
            counts[source_team_id][1] += 1
    return {team_id: (values[0], values[1]) for team_id, values in counts.items()}


def _row_position_counts(row, player_count, hitter_count, detailed_roles):
    if detailed_roles:
        counts = {
            "P": max(0, _to_int(row, "pitcher_count")),
            "C": max(0, _to_int(row, "catcher_count")),
            "1B": max(0, _to_int(row, "first_base_count")) + max(0, _to_int(row, "designated_hitter_count")),
            "2B": max(0, _to_int(row, "second_base_count")),
            "3B": max(0, _to_int(row, "third_base_count")),
            "SS": max(0, _to_int(row, "shortstop_count")),
            "LF": max(0, _to_int(row, "left_field_count")),
            "CF": max(0, _to_int(row, "center_field_count")),
            "RF": max(0, _to_int(row, "right_field_count")),
        }
        if sum(counts.values()) > 0:
            return counts
        infielder_count = max(0, _to_int(row, "infielder_count"))
        outfielder_count = max(0, _to_int(row, "outfielder_count"))
        if infielder_count > 0 or outfielder_count > 0:
            counts["1B"] = infielder_count
            counts["LF"] = outfielder_count
            counts["P"] = max(0, player_count - hitter_count)
            counts["C"] = max(0, hitter_count - infielder_count - outfielder_count)
            return counts

    pitcher_count = max(0, player_count - hitter_count)
    return {"P": pitcher_count, "H": max(0, hitter_count)}


def _batch_source_position_counts(grouped):
    counts = defaultdict(lambda: defaultdict(int))
    for rows in grouped.values():
        if not rows:
            continue
        source_team_id = _to_int(rows[0], "current_team_id")
        if source_team_id == 0:
            continue
        counts[source_team_id][_player_position_bucket(rows[0])] += 1
    return {team_id: dict(role_counts) for team_id, role_counts in counts.items()}


def _batch_role_counts(grouped):
    counts = defaultdict(int)
    for rows in grouped.values():
        if rows:
            counts[_player_position_bucket(rows[0])] += 1
    return dict(counts)


def _collect_batch_team_info(grouped, team_percentiles, incoming_batch=False, detailed_roles=False):
    team_info = {}
    source_position_counts = {} if incoming_batch else _batch_source_position_counts(grouped)
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
            for role, outgoing_count in source_position_counts.get(team_id, {}).items():
                role_counts[role] = max(0, role_counts.get(role, 0) - outgoing_count)
            player_count = sum(role_counts.values())
            hitter_count = sum(count for role, count in role_counts.items() if role != "P")
            if not incoming_batch and max_players > 0 and player_count >= max_players:
                continue
            if incoming_batch:
                capacity = len(grouped)
            else:
                capacity = max(0, max_players - player_count) if max_players > 0 else len(grouped)
            if capacity <= 0:
                continue
            min_fill = max(0, min(capacity, _team_min_players(league_id) - player_count))
            team_info[team_id] = {
                "node": None,
                "role_nodes": {},
                "role_capacities": {},
                "capacity": capacity,
                "target_count": 0,
                "min_fill": min_fill,
                "league_id": league_id,
                "player_count": player_count,
                "hitter_count": hitter_count,
                "role_counts": role_counts,
                "reputation": _to_int(row, "reputation"),
                "percentile": team_percentiles.get(team_id, 0.5),
                "draft_penalty_stages": _to_int(row, "draft_penalty_stages"),
            }
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


def _role_bounds(info, hitter_share, tolerance):
    target_count = info["target_count"]
    if target_count <= 0:
        return (0, 0)

    final_total = info["player_count"] + target_count
    current_hitters = info["hitter_count"]
    min_share = max(0.0, hitter_share - tolerance)
    max_share = min(1.0, hitter_share + tolerance)
    min_final_hitters = int(math.floor(final_total * min_share))
    max_final_hitters = int(math.ceil(final_total * max_share))

    min_assigned_hitters = min(target_count, max(0, min_final_hitters - current_hitters))
    max_assigned_hitters = min(target_count, max(0, max_final_hitters - current_hitters))
    if max_assigned_hitters < min_assigned_hitters:
        max_assigned_hitters = min_assigned_hitters
    return (min_assigned_hitters, max_assigned_hitters)


def _role_need_priority(info, hitter_share):
    final_total = info["player_count"] + info["target_count"]
    if final_total <= 0:
        return 0.0
    share_without_new_hitter = info["hitter_count"] / final_total
    return hitter_share - share_without_new_hitter


def _position_role_need_priority(info, role, role_share):
    final_total = info["player_count"] + info["target_count"]
    if final_total <= 0:
        return 0.0
    current_role_count = info.get("role_counts", {}).get(role, 0)
    assigned_role_count = info.get("role_capacities", {}).get(role, 0)
    return role_share - ((current_role_count + assigned_role_count) / final_total)


def _prepare_position_bucket_capacities(team_info, role_counts):
    total_players = sum(max(0, count) for count in role_counts.values())
    if total_players <= 0:
        return False
    if sum(info["target_count"] for info in team_info.values()) != total_players:
        return False

    roles = [role for role in AMATEUR_POSITION_BUCKETS if role_counts.get(role, 0) > 0]
    roles.extend(
        sorted(
            role for role, count in role_counts.items()
            if count > 0 and role not in roles
        )
    )
    for info in team_info.values():
        info["role_capacities"] = {role: 0 for role in roles}

    remaining_slots = {
        team_id: max(0, info["target_count"])
        for team_id, info in team_info.items()
    }
    role_order = sorted(
        roles,
        key=lambda role: (
            total_players + role_counts.get(role, 0) if role == "P" else role_counts.get(role, 0),
            AMATEUR_POSITION_BUCKETS.index(role) if role in AMATEUR_POSITION_BUCKETS else len(AMATEUR_POSITION_BUCKETS),
        ),
    )

    for role in role_order:
        role_total = max(0, role_counts.get(role, 0))
        role_share = role_total / total_players
        for _ in range(role_total):
            candidates = [
                (team_id, info)
                for team_id, info in team_info.items()
                if remaining_slots.get(team_id, 0) > 0
            ]
            if not candidates:
                return False
            team_id, info = max(
                candidates,
                key=lambda item: (
                    _position_role_need_priority(item[1], role, role_share),
                    item[1]["percentile"],
                    item[1]["reputation"],
                    -item[1]["role_capacities"].get(role, 0),
                    item[0],
                ),
            )
            info["role_capacities"][role] = info["role_capacities"].get(role, 0) + 1
            remaining_slots[team_id] -= 1

    return all(slots == 0 for slots in remaining_slots.values())


def _spread_incoming_hitter_targets(team_info, hitter_players):
    for info in team_info.values():
        info["incoming_hitter_target"] = 0
    if hitter_players <= 0:
        return True

    ordered = sorted(
        (
            info
            for info in team_info.values()
            if info["target_count"] > 0
        ),
        key=lambda info: (info["role_need_priority"], info["percentile"], info["reputation"]),
        reverse=True,
    )
    if not ordered:
        return False

    remaining = hitter_players
    while remaining > 0:
        placed = False
        for info in ordered:
            if info["incoming_hitter_target"] >= info["target_count"]:
                continue
            info["incoming_hitter_target"] += 1
            remaining -= 1
            placed = True
            if remaining <= 0:
                break
        if not placed:
            return False
    return True


def _prepare_role_capacities(team_info, hitter_share, tolerance, role_counts, incoming_batch=False, detailed_roles=False):
    if detailed_roles:
        return _prepare_position_bucket_capacities(team_info, role_counts)

    hitter_players = sum(count for role, count in role_counts.items() if role != "P")
    for team_id, info in team_info.items():
        min_hitters, max_hitters = _role_bounds(info, hitter_share, tolerance)
        info["min_assigned_hitters"] = min_hitters
        info["max_assigned_hitters"] = max_hitters
        info["role_need_priority"] = _role_need_priority(info, hitter_share)

    if incoming_batch:
        if not _spread_incoming_hitter_targets(team_info, hitter_players):
            return False
        for info in team_info.values():
            hitter_target = info["incoming_hitter_target"]
            info["min_assigned_hitters"] = hitter_target
            info["max_assigned_hitters"] = hitter_target
            info["hitter_capacity"] = hitter_target
            info["pitcher_capacity"] = info["target_count"] - hitter_target
            info["role_capacities"] = {
                "H": hitter_target,
                "P": info["target_count"] - hitter_target,
            }
        return True

    total_min_hitters = sum(info["min_assigned_hitters"] for info in team_info.values())
    if total_min_hitters > hitter_players:
        excess = total_min_hitters - hitter_players
        reducible = []
        for team_id, info in team_info.items():
            for _ in range(info["min_assigned_hitters"]):
                reducible.append((info["role_need_priority"], info["percentile"], team_id))
        reducible.sort()
        for _, _, team_id in reducible:
            if excess <= 0:
                break
            info = team_info[team_id]
            if info["min_assigned_hitters"] <= 0:
                continue
            info["min_assigned_hitters"] -= 1
            excess -= 1

    total_max_hitters = sum(info["max_assigned_hitters"] for info in team_info.values())
    if total_max_hitters < hitter_players:
        shortage = hitter_players - total_max_hitters
        expandable = []
        for team_id, info in team_info.items():
            open_slots = info["target_count"] - info["max_assigned_hitters"]
            for _ in range(max(0, open_slots)):
                expandable.append((-info["role_need_priority"], -info["percentile"], team_id))
        expandable.sort()
        for _, _, team_id in expandable:
            if shortage <= 0:
                break
            info = team_info[team_id]
            if info["max_assigned_hitters"] >= info["target_count"]:
                continue
            info["max_assigned_hitters"] += 1
            shortage -= 1

    total_min_hitters = sum(info["min_assigned_hitters"] for info in team_info.values())
    total_max_hitters = sum(info["max_assigned_hitters"] for info in team_info.values())
    if total_min_hitters > hitter_players or total_max_hitters < hitter_players:
        return False

    for info in team_info.values():
        min_hitters = info["min_assigned_hitters"]
        max_hitters = info["max_assigned_hitters"]
        target_count = info["target_count"]
        if min_hitters < 0 or max_hitters < min_hitters or max_hitters > target_count:
            return False
        info["hitter_capacity"] = max_hitters
        info["pitcher_capacity"] = target_count - min_hitters
        info["role_capacities"] = {
            "H": max_hitters,
            "P": target_count - min_hitters,
        }
    return True


def _solve_batch_min_cost_flow(choices, source_limits, source_hitter_limits, team_capacity):
    total_supply = sum(max(0, limit) for limit in source_limits.values())
    if total_supply <= 0 or not choices:
        return {}

    solver = min_cost_flow.SimpleMinCostFlow()
    source = 0
    sink = 1
    next_node = 2

    def new_node():
        nonlocal next_node
        node = next_node
        next_node += 1
        return node

    source_nodes = {}
    source_hitter_nodes = {}
    player_nodes = {}
    team_nodes = {}
    assignment_arcs = []

    for team_id, limit in source_limits.items():
        limit = max(0, limit)
        if limit <= 0:
            continue
        source_node = new_node()
        source_nodes[team_id] = source_node
        solver.add_arc_with_capacity_and_unit_cost(source, source_node, limit, 0)
        solver.add_arc_with_capacity_and_unit_cost(source_node, sink, limit, 0)

        hitter_limit = max(0, min(limit, source_hitter_limits.get(team_id, limit)))
        hitter_node = new_node()
        source_hitter_nodes[team_id] = hitter_node
        solver.add_arc_with_capacity_and_unit_cost(source_node, hitter_node, hitter_limit, 0)

    for player_id, row, weight in choices:
        source_team_id = _to_int(row, "current_team_id")
        source_node = source_nodes.get(source_team_id)
        if source_node is None:
            continue

        player_node = player_nodes.get(player_id)
        if player_node is None:
            player_node = new_node()
            player_nodes[player_id] = player_node
            if _to_int(row, "is_hitter") != 0:
                hitter_node = source_hitter_nodes.get(source_team_id)
                if hitter_node is None:
                    continue
                solver.add_arc_with_capacity_and_unit_cost(hitter_node, player_node, 1, 0)
            else:
                solver.add_arc_with_capacity_and_unit_cost(source_node, player_node, 1, 0)

        target_team_id = _to_int(row, "team_id")
        target_capacity = team_capacity.get(target_team_id, total_supply)
        if target_capacity <= 0:
            continue
        team_node = team_nodes.get(target_team_id)
        if team_node is None:
            team_node = new_node()
            team_nodes[target_team_id] = team_node
            solver.add_arc_with_capacity_and_unit_cost(team_node, sink, target_capacity, 0)

        arc = solver.add_arc_with_capacity_and_unit_cost(player_node, team_node, 1, -weight)
        assignment_arcs.append((arc, player_id, target_team_id, weight))

    if not assignment_arcs:
        return {}

    solver.set_node_supply(source, total_supply)
    solver.set_node_supply(sink, -total_supply)
    status = solver.solve()
    if status not in (solver.OPTIMAL, solver.FEASIBLE):
        return None

    assignments = {}
    for arc, player_id, target_team_id, weight in assignment_arcs:
        if solver.flow(arc) > 0:
            assignments[player_id] = (target_team_id, weight, "ok")
    return assignments


def _team_min_players(league_id):
    return 24 if league_id == KBO_COLLEGE_LEAGUE_ID else 18


def _solve_batch_final_assignment_with_tolerance(grouped, role_tolerance, force_incoming=None):
    solver = min_cost_flow.SimpleMinCostFlow()
    source = 0
    sink = 1
    next_node = 2

    def new_node():
        nonlocal next_node
        node = next_node
        next_node += 1
        return node

    player_nodes = {}
    assignment_arcs = []
    player_percentiles = _player_role_quality_percentiles(grouped)
    team_percentiles = _team_reputation_percentiles(grouped)
    draft_penalties = _batch_draft_penalties(grouped)
    total_teams = max(1, len(team_percentiles))
    incoming_batch = _batch_is_incoming(grouped) if force_incoming is None else force_incoming
    detailed_roles = _batch_has_detailed_position_buckets(grouped)
    hitter_share = _batch_hitter_share(grouped, incoming_batch)
    team_info = _collect_batch_team_info(grouped, team_percentiles, incoming_batch, detailed_roles)

    total_players = len(grouped)
    role_counts = _batch_role_counts(grouped)
    if not _allocate_batch_team_targets(team_info, total_players, incoming_batch):
        return None
    if not _prepare_role_capacities(team_info, hitter_share, role_tolerance, role_counts, incoming_batch, detailed_roles):
        return None

    for info in team_info.values():
        role_capacities = {
            role: capacity
            for role, capacity in info.get("role_capacities", {}).items()
            if capacity > 0
        }
        if sum(role_capacities.values()) < info["target_count"]:
            return None

        team_node = new_node()
        info["node"] = team_node
        info["role_nodes"] = {}
        for role, capacity in role_capacities.items():
            role_node = new_node()
            info["role_nodes"][role] = role_node
            solver.add_arc_with_capacity_and_unit_cost(role_node, team_node, capacity, 0)
        if info["target_count"] > 0:
            solver.add_arc_with_capacity_and_unit_cost(team_node, sink, info["target_count"], 0)

    for player_id, player_rows in grouped.items():
        player_node = new_node()
        player_nodes[player_id] = player_node
        solver.add_arc_with_capacity_and_unit_cost(source, player_node, 1, 0)

        for row in player_rows:
            team_id = _to_int(row, "team_id")
            if team_id == 0 or _to_int(row, "rejected") != 0:
                continue
            info = team_info.get(team_id)
            if info is None or info["target_count"] <= 0:
                continue
            role_node = info["role_nodes"].get(_player_position_bucket(row))
            if role_node is None:
                continue
            weight = _candidate_weight(row, -1, info["player_count"], not incoming_batch)
            penalty_stages = draft_penalties.get(team_id, 0)
            adjusted_team_percentile = max(
                0.0, team_percentiles.get(team_id, 0.5) - penalty_stages / total_teams
            )
            weight += _rank_fit_weight(
                player_percentiles.get(player_id, 0.5),
                adjusted_team_percentile,
            )
            weight += int(
                ASSORTATIVE_RANK_WEIGHT
                * player_percentiles.get(player_id, 0.5)
                * adjusted_team_percentile
            )

            arc = solver.add_arc_with_capacity_and_unit_cost(player_node, role_node, 1, -weight)
            assignment_arcs.append((arc, player_id, team_id, weight))

    if not assignment_arcs:
        return {}

    solver.set_node_supply(source, total_players)
    solver.set_node_supply(sink, -total_players)
    status = solver.solve()
    if status not in (solver.OPTIMAL, solver.FEASIBLE):
        return None

    assignments = {}
    for arc, player_id, team_id, weight in assignment_arcs:
        if solver.flow(arc) > 0:
            assignments[player_id] = (team_id, weight, "ok")
    return assignments


def _solve_batch_final_assignment(grouped):
    for role_tolerance in AMATEUR_ROLE_BALANCE_TOLERANCES:
        assignments = _solve_batch_final_assignment_with_tolerance(grouped, role_tolerance)
        if assignments is not None:
            return assignments
    for role_tolerance in AMATEUR_ROLE_BALANCE_TOLERANCES:
        assignments = _solve_batch_final_assignment_with_tolerance(grouped, role_tolerance, True)
        if assignments is not None:
            return assignments
    return None


def optimize(request_path, result_path):
    with open(request_path, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    if not rows:
        return 2
    if len({ _to_int(row, "player_id") for row in rows }) > 1:
        return optimize_batch_rows(rows, result_path)

    current_team_id = _to_int(rows[0], "current_team_id")
    player_tier = _to_int(rows[0], "player_tier")

    prelim = []
    target_player_count = None
    for row in rows:
        team_id = _to_int(row, "team_id")
        reputation = _to_int(row, "reputation")
        if team_id == 0 or team_id == current_team_id:
            continue
        if _to_int(row, "rejected") != 0:
            continue
        if abs(player_tier - _to_int(row, "team_tier")) > 1:
            continue
        player_count = _to_int(row, "player_count")
        target_max_players = _team_max_players(
            _to_int(row, "league_id"),
            _to_int(row, "target_max_players"),
        )
        if target_max_players > 0 and player_count >= target_max_players:
            continue
        if target_player_count is None or player_count < target_player_count:
            target_player_count = player_count
        prelim.append(row)

    if target_player_count is None:
        target_player_count = -1

    candidates = []
    for row in prelim:
        weight = _candidate_weight(row, target_player_count)
        if weight > 0:
            candidates.append((row, weight))

    if not candidates:
        with open(result_path, "w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(["target_team_id", "weight", "status"])
            writer.writerow([0, 0, "no_candidate"])
        return 0

    model = cp_model.CpModel()
    variables = [model.NewBoolVar(f"team_{_to_int(row, 'team_id')}") for row, _ in candidates]
    model.Add(sum(variables) == 1)
    model.Maximize(sum(var * weight for var, (_, weight) in zip(variables, candidates)))

    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = 0.25
    solver.parameters.num_search_workers = 1
    status = solver.Solve(model)

    best_index = 0
    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        for i, var in enumerate(variables):
            if solver.Value(var):
                best_index = i
                break
    else:
        best_index = max(range(len(candidates)), key=lambda i: candidates[i][1])

    row, weight = candidates[best_index]
    with open(result_path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["target_team_id", "weight", "status"])
        writer.writerow([_to_int(row, "team_id"), weight, "ok"])
    return 0


def optimize_batch_rows(rows, result_path):
    grouped = {}
    for row in rows:
        player_id = _to_int(row, "player_id")
        if player_id != 0:
            grouped.setdefault(player_id, []).append(row)

    assignments = _solve_batch_final_assignment(grouped)
    if assignments is None:
        assignments = {}

    with open(result_path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["player_id", "target_team_id", "weight", "status"])
        for player_id in sorted(grouped):
            team_id, weight, row_status = assignments.get(player_id, (0, 0, "no_candidate"))
            writer.writerow([player_id, team_id, weight, row_status])
    return 0


def _role_bucket(row):
    return (row.get("role_bucket") or "").strip().upper()


def _solve_asian_games_model(rows, hard_required_orgs, hard_role_minimums):
    if not rows:
        return None

    roster_size = min(ASIAN_GAMES_ROSTER_SIZE, len(rows))
    model = cp_model.CpModel()
    variables = [model.NewBoolVar(f"player_{_to_int(row, 'player_id')}") for row in rows]
    model.Add(sum(variables) == roster_size)

    wildcard_vars = []
    by_role = defaultdict(list)
    by_org = defaultdict(list)
    required_orgs = set()
    for row, var in zip(rows, variables):
        role = _role_bucket(row)
        by_role[role].append(var)
        org_id = _to_int(row, "org_team_id")
        if org_id != 0:
            by_org[org_id].append(var)
        if _to_int(row, "required_org") != 0 and org_id != 0:
            required_orgs.add(org_id)
        if _to_int(row, "age") > 24:
            wildcard_vars.append(var)

    if wildcard_vars:
        model.Add(sum(wildcard_vars) <= ASIAN_GAMES_MAX_WILDCARDS)

    for org_id, org_vars in by_org.items():
        model.Add(sum(org_vars) <= ASIAN_GAMES_TEAM_MAX_PLAYERS)
        if hard_required_orgs and org_id in required_orgs:
            model.Add(sum(org_vars) >= 1)

    objective_terms = []
    for row, var in zip(rows, variables):
        score = _to_int(row, "score")
        age = _to_int(row, "age")
        role = _role_bucket(row)
        org_id = _to_int(row, "org_team_id")
        weight = score * 100
        if age <= 24:
            weight += 25000
        if org_id in required_orgs:
            weight += ASIAN_GAMES_REQUIRED_ORG_BONUS
        if role in ASIAN_GAMES_ROLE_TARGETS:
            weight += 1000
        objective_terms.append(var * weight)

    for role, target in ASIAN_GAMES_ROLE_TARGETS.items():
        role_sum = sum(by_role.get(role, []))
        if hard_role_minimums:
            model.Add(role_sum >= ASIAN_GAMES_ROLE_MINIMUMS[role])
            model.Add(role_sum <= ASIAN_GAMES_ROLE_MAXIMUMS[role])
        deviation = model.NewIntVar(0, ASIAN_GAMES_ROSTER_SIZE, f"{role}_deviation")
        model.AddAbsEquality(deviation, role_sum - target)
        objective_terms.append(deviation * -ASIAN_GAMES_ROLE_DEVIATION_PENALTY)

    model.Maximize(sum(objective_terms))
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = 2.0
    solver.parameters.num_search_workers = 8
    status = solver.Solve(model)
    if status not in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        return None

    selected = []
    for index, (row, var) in enumerate(zip(rows, variables)):
        if solver.Value(var):
            selected.append((index, row))
    selected.sort(key=lambda item: (-_to_int(item[1], "score"), _to_int(item[1], "player_id")))
    return selected


def optimize_asian_games_roster(request_path, result_path):
    with open(request_path, newline="", encoding="utf-8") as handle:
        rows = [row for row in csv.DictReader(handle) if _to_int(row, "player_id") != 0]

    selected = None
    for hard_required, hard_roles in ((True, True), (False, True), (False, False)):
        selected = _solve_asian_games_model(rows, hard_required, hard_roles)
        if selected:
            break

    with open(result_path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["player_id", "rank", "status"])
        if not selected:
            return 0
        for rank, (_, row) in enumerate(selected, start=1):
            writer.writerow([_to_int(row, "player_id"), rank, "selected"])
    return 0


def optimize_military_selection(request_path, result_path):
    with open(request_path, newline="", encoding="utf-8") as handle:
        rows = [row for row in csv.DictReader(handle) if _to_int(row, "player_id") != 0]

    slots = max(0, max((_to_int(row, "slots") for row in rows), default=0))
    target = min(slots, len(rows))
    if target <= 0:
        selected = []
    else:
        model = cp_model.CpModel()
        variables = [model.NewBoolVar(f"player_{_to_int(row, 'player_id')}") for row in rows]
        model.Add(sum(variables) == target)

        by_team = defaultdict(list)
        objective_terms = []
        for row, var in zip(rows, variables):
            team_id = _to_int(row, "original_team_id")
            if team_id != 0:
                by_team[team_id].append(var)
            objective_terms.append(var * (_to_int(row, "score") * 1000))

        for team_vars in by_team.values():
            team_selected = sum(team_vars)
            spread = model.NewBoolVar(f"team_spread_{len(objective_terms)}")
            model.Add(team_selected >= 1).OnlyEnforceIf(spread)
            model.Add(team_selected == 0).OnlyEnforceIf(spread.Not())
            objective_terms.append(spread * MILITARY_SELECTION_TEAM_SPREAD_BONUS)

        model.Maximize(sum(objective_terms))
        solver = cp_model.CpSolver()
        solver.parameters.max_time_in_seconds = 1.0
        solver.parameters.num_search_workers = 4
        status = solver.Solve(model)
        if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
            selected = [row for row, var in zip(rows, variables) if solver.Value(var)]
        else:
            selected = sorted(rows, key=lambda row: (-_to_int(row, "score"), _to_int(row, "player_id")))[:target]

    selected.sort(key=lambda row: (-_to_int(row, "score"), _to_int(row, "player_id")))
    with open(result_path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["player_id", "rank", "status"])
        for rank, row in enumerate(selected, start=1):
            writer.writerow([_to_int(row, "player_id"), rank, "selected"])
    return 0


def optimize_fa_compensation(request_path, result_path):
    with open(request_path, newline="", encoding="utf-8") as handle:
        rows = [row for row in csv.DictReader(handle) if _to_int(row, "player_id") != 0]

    protect_count = max(0, max((_to_int(row, "protect_count") for row in rows), default=0))
    protect_count = min(protect_count, len(rows))
    selected_ids = set()
    if protect_count > 0:
        model = cp_model.CpModel()
        variables = [model.NewBoolVar(f"player_{_to_int(row, 'player_id')}") for row in rows]
        model.Add(sum(variables) == protect_count)
        objective_terms = []
        by_role = defaultdict(list)
        for row, var in zip(rows, variables):
            role = _to_int(row, "role")
            by_role[role].append(var)
            objective_terms.append(var * (_to_int(row, "score") * 1000))
        for role_vars in by_role.values():
            covered = model.NewBoolVar(f"role_covered_{len(objective_terms)}")
            model.Add(sum(role_vars) >= 1).OnlyEnforceIf(covered)
            model.Add(sum(role_vars) == 0).OnlyEnforceIf(covered.Not())
            objective_terms.append(covered * FA_PROTECTION_ROLE_BONUS)
        model.Maximize(sum(objective_terms))
        solver = cp_model.CpSolver()
        solver.parameters.max_time_in_seconds = 1.0
        solver.parameters.num_search_workers = 4
        status = solver.Solve(model)
        if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
            selected_ids = {
                _to_int(row, "player_id")
                for row, var in zip(rows, variables)
                if solver.Value(var)
            }
        else:
            selected_ids = {
                _to_int(row, "player_id")
                for row in sorted(rows, key=lambda row: (-_to_int(row, "score"), _to_int(row, "player_id")))[:protect_count]
            }

    protected_rows = [row for row in rows if _to_int(row, "player_id") in selected_ids]
    unprotected_rows = [row for row in rows if _to_int(row, "player_id") not in selected_ids]
    protected_rows.sort(key=lambda row: (-_to_int(row, "score"), _to_int(row, "player_id")))
    unprotected_rows.sort(key=lambda row: (-_to_int(row, "score"), _to_int(row, "player_id")))

    with open(result_path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["player_id", "rank", "status"])
        rank = 1
        for row in protected_rows:
            writer.writerow([_to_int(row, "player_id"), rank, "protected"])
            rank += 1
        for row in unprotected_rows:
            writer.writerow([_to_int(row, "player_id"), rank, "unprotected"])
            rank += 1
    return 0


def optimize_mode(mode, request_path, result_path):
    if mode == "amateur_assignment":
        return optimize(request_path, result_path)
    if mode == "asian_games_roster":
        return optimize_asian_games_roster(request_path, result_path)
    if mode == "military_selection":
        return optimize_military_selection(request_path, result_path)
    if mode == "fa_compensation":
        return optimize_fa_compensation(request_path, result_path)
    raise ValueError(f"unknown optimizer mode: {mode}")


def serve():
    for line in sys.stdin:
        parts = line.rstrip("\n").split("\t")
        if len(parts) == 2:
            mode = "amateur_assignment"
            request_path, result_path = parts
        elif len(parts) == 3:
            mode, request_path, result_path = parts
        else:
            print("ERR bad_request", flush=True)
            continue
        try:
            code = optimize_mode(mode, request_path, result_path)
        except Exception as exc:
            print(f"ERR {type(exc).__name__}", flush=True)
            continue
        print(f"OK {code}", flush=True)
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--server":
        raise SystemExit(serve())
    if len(sys.argv) == 5 and sys.argv[1] == "--mode":
        raise SystemExit(optimize_mode(sys.argv[2], sys.argv[3], sys.argv[4]))
    if len(sys.argv) != 3:
        raise SystemExit("usage: kbo_optimizer.py [--mode MODE] REQUEST_CSV RESULT_CSV")
    raise SystemExit(optimize(sys.argv[1], sys.argv[2]))
