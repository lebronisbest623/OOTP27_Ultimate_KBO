"""Position, role, and role-capacity helpers for amateur assignment."""

import math
from collections import defaultdict

from .constants import (
    AMATEUR_MAX_HITTER_SHARE,
    AMATEUR_MIN_HITTER_SHARE,
    AMATEUR_POSITION_BUCKETS,
)
from .csv_io import to_int as _to_int

def _batch_explicit_modes(grouped):
    explicit_modes = set()
    for rows in grouped.values():
        if not rows:
            continue
        for row in rows:
            mode = (row.get("batch_mode") or "").strip().lower()
            if mode:
                explicit_modes.add(mode)
    return explicit_modes

def _batch_is_incoming(grouped):
    explicit_modes = _batch_explicit_modes(grouped)
    if any(mode in ("incoming", "incoming_attached", "post_original", "deferred_add", "freshman") for mode in explicit_modes):
        return True
    if any(mode in ("roster", "existing_roster", "seed") for mode in explicit_modes):
        return False

    return False

def _batch_source_counts_attached_to_rosters(grouped):
    explicit_modes = _batch_explicit_modes(grouped)
    if any(mode in ("incoming_attached", "post_original") for mode in explicit_modes):
        return True
    if any(mode in ("deferred_add", "freshman") for mode in explicit_modes):
        return False

    source_counts = _batch_source_counts(grouped)
    if not source_counts:
        return False

    team_player_counts = {}
    for rows in grouped.values():
        for row in rows:
            team_id = _to_int(row, "team_id")
            if team_id != 0 and team_id not in team_player_counts:
                team_player_counts[team_id] = max(0, _to_int(row, "player_count"))

    checked = 0
    attached = 0
    for team_id, (source_players, _) in source_counts.items():
        if team_id not in team_player_counts:
            continue
        checked += 1
        if team_player_counts[team_id] >= source_players:
            attached += 1
    return checked > 0 and attached == checked

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
    subtract_source_counts = not incoming_batch or _batch_source_counts_attached_to_rosters(grouped)
    source_counts = _batch_source_counts(grouped) if subtract_source_counts else {}
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
