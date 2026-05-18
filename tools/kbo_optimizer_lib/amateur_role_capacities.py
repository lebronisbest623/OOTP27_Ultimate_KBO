"""Role capacity allocation helpers for amateur assignment."""

import math

from .constants import AMATEUR_POSITION_BUCKETS

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
