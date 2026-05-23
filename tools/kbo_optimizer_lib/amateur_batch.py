"""Batch amateur-assignment solver."""

from collections import Counter
import csv
import math

from ortools.graph.python import min_cost_flow

from .amateur_common import _candidate_weight
from .amateur_metrics import (
    _batch_draft_penalties,
    _player_role_quality_percentiles,
    _rank_fit_weight,
    _team_reputation_percentiles,
)
from .amateur_roles import (
    _batch_has_detailed_position_buckets,
    _batch_hitter_share,
    _batch_is_incoming,
    _batch_role_counts,
    _player_position_bucket,
)
from .amateur_role_capacities import _prepare_role_capacities
from .amateur_targets import _allocate_batch_team_targets, _collect_batch_team_info
from .constants import (
    AMATEUR_ROLE_BALANCE_TOLERANCES,
    ASSORTATIVE_RANK_WEIGHT,
    INCOMING_MAX_AVERAGE_MULTIPLIER,
)
from .csv_io import to_int as _to_int

def _tier_gap(row):
    return abs(_to_int(row, "player_tier") - _to_int(row, "team_tier"))

def _tier_allowed(row, max_tier_gap):
    return max_tier_gap is None or _tier_gap(row) <= max_tier_gap

def _incoming_team_cap(total_players, total_teams):
    average = total_players / max(1, total_teams)
    return max(1, int(math.ceil(average * INCOMING_MAX_AVERAGE_MULTIPLIER)))

def _tier_capacity_feasible(grouped, team_info, max_tier_gap, use_incoming_capacity, incoming_cap):
    if max_tier_gap is None:
        return True

    player_tiers = Counter()
    for player_rows in grouped.values():
        if not player_rows:
            continue
        tier = _to_int(player_rows[0], "player_tier", -1)
        if tier < 0:
            return True
        player_tiers[tier] += 1
    if not player_tiers:
        return True

    team_capacity_by_tier = Counter()
    team_min_fill_by_tier = Counter()
    unresolved_team_ids = set(team_info)
    for player_rows in grouped.values():
        for row in player_rows:
            team_id = _to_int(row, "team_id")
            if team_id not in unresolved_team_ids or _to_int(row, "rejected") != 0:
                continue
            tier = _to_int(row, "team_tier", -1)
            if tier < 0:
                return True
            info = team_info[team_id]
            capacity = incoming_cap if use_incoming_capacity else max(0, info["capacity"])
            if capacity > 0:
                team_capacity_by_tier[tier] += capacity
                team_min_fill_by_tier[tier] += max(0, int(info.get("min_fill", 0)))
            unresolved_team_ids.remove(team_id)
            if not unresolved_team_ids:
                break
        if not unresolved_team_ids:
            break
    if unresolved_team_ids:
        return True

    if sum(player_tiers.values()) > sum(team_capacity_by_tier.values()):
        return False

    player_tier_values = sorted(player_tiers)
    for mask in range(1, 1 << len(player_tier_values)):
        selected_player_tiers = [
            tier
            for index, tier in enumerate(player_tier_values)
            if mask & (1 << index)
        ]
        demand = sum(player_tiers[tier] for tier in selected_player_tiers)
        capacity = 0
        for team_tier, tier_capacity in team_capacity_by_tier.items():
            if any(abs(player_tier - team_tier) <= max_tier_gap for player_tier in selected_player_tiers):
                capacity += tier_capacity
        if demand > capacity:
            return False

    team_tier_values = sorted(team_min_fill_by_tier)
    for mask in range(1, 1 << len(team_tier_values)):
        selected_team_tiers = [
            tier
            for index, tier in enumerate(team_tier_values)
            if mask & (1 << index)
        ]
        minimum_fill = sum(team_min_fill_by_tier[tier] for tier in selected_team_tiers)
        if minimum_fill <= 0:
            continue
        eligible_players = 0
        for player_tier, tier_count in player_tiers.items():
            if any(abs(player_tier - team_tier) <= max_tier_gap for team_tier in selected_team_tiers):
                eligible_players += tier_count
        if minimum_fill > eligible_players:
            return False
    return True

def _prepare_flexible_assignment_context(grouped, force_incoming=None):
    total_players = len(grouped)
    if total_players <= 0:
        return {
            "total_players": total_players,
            "team_info": {},
        }

    team_percentiles = _team_reputation_percentiles(grouped)
    total_teams = max(1, len(team_percentiles))
    incoming_batch = _batch_is_incoming(grouped) if force_incoming is None else force_incoming
    detailed_roles = _batch_has_detailed_position_buckets(grouped)
    team_info = _collect_batch_team_info(grouped, team_percentiles, incoming_batch, detailed_roles)

    normal_capacity = sum(max(0, info["capacity"]) for info in team_info.values())
    use_incoming_capacity = incoming_batch or normal_capacity < total_players
    incoming_cap = _incoming_team_cap(total_players, len(team_info))
    return {
        "total_players": total_players,
        "team_percentiles": team_percentiles,
        "total_teams": total_teams,
        "incoming_batch": incoming_batch,
        "team_info": team_info,
        "normal_capacity": normal_capacity,
        "use_incoming_capacity": use_incoming_capacity,
        "incoming_cap": incoming_cap,
        "player_percentiles": None,
        "draft_penalties": None,
    }

def _solve_batch_flexible_assignment(grouped, max_tier_gap=1, status_label="ok", force_incoming=None, context=None):
    if context is None:
        context = _prepare_flexible_assignment_context(grouped, force_incoming)

    total_players = context["total_players"]
    if total_players <= 0:
        return {}

    team_info = context["team_info"]
    if not team_info:
        return None

    team_percentiles = context["team_percentiles"]
    total_teams = context["total_teams"]
    incoming_batch = context["incoming_batch"]
    use_incoming_capacity = context["use_incoming_capacity"]
    incoming_cap = context["incoming_cap"]
    if not _tier_capacity_feasible(
            grouped,
            team_info,
            max_tier_gap,
            use_incoming_capacity,
            incoming_cap):
        return None

    player_percentiles = context.get("player_percentiles")
    if player_percentiles is None:
        player_percentiles = _player_role_quality_percentiles(grouped)
        context["player_percentiles"] = player_percentiles
    draft_penalties = context.get("draft_penalties")
    if draft_penalties is None:
        draft_penalties = _batch_draft_penalties(grouped)
        context["draft_penalties"] = draft_penalties

    solver = min_cost_flow.SimpleMinCostFlow()
    source = 0
    sink = 1
    next_node = 2

    def new_node():
        nonlocal next_node
        node = next_node
        next_node += 1
        return node

    team_nodes = {}
    node_supplies = {}
    required_total = 0

    def add_node_supply(node, amount):
        node_supplies[node] = node_supplies.get(node, 0) + amount

    for team_id, info in team_info.items():
        capacity = incoming_cap if use_incoming_capacity else max(0, info["capacity"])
        min_fill = max(0, min(int(info.get("min_fill", 0)), capacity))
        if capacity <= 0:
            continue
        if required_total + min_fill > total_players:
            return None
        team_node = new_node()
        team_nodes[team_id] = team_node
        if min_fill > 0:
            required_total += min_fill
            add_node_supply(team_node, -min_fill)
            add_node_supply(sink, min_fill)
        solver.add_arc_with_capacity_and_unit_cost(team_node, sink, capacity - min_fill, 0)

    assignment_arcs = []
    for player_id, player_rows in grouped.items():
        player_node = new_node()
        solver.add_arc_with_capacity_and_unit_cost(source, player_node, 1, 0)

        for row in player_rows:
            team_id = _to_int(row, "team_id")
            if team_id == 0 or _to_int(row, "rejected") != 0:
                continue
            if team_id not in team_nodes or not _tier_allowed(row, max_tier_gap):
                continue

            info = team_info[team_id]
            weight = _candidate_weight(row, -1, info["player_count"], not use_incoming_capacity)
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

            arc = solver.add_arc_with_capacity_and_unit_cost(
                player_node,
                team_nodes[team_id],
                1,
                -weight)
            assignment_arcs.append((arc, player_id, team_id, weight))

    if not assignment_arcs:
        return None

    add_node_supply(source, total_players)
    add_node_supply(sink, -total_players)
    for node, supply in node_supplies.items():
        solver.set_node_supply(node, supply)
    status = solver.solve()
    if status not in (solver.OPTIMAL, solver.FEASIBLE):
        return None

    assignments = {}
    for arc, player_id, team_id, weight in assignment_arcs:
        if solver.flow(arc) > 0:
            assignments[player_id] = (team_id, weight, status_label)
    return assignments

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

def _solve_batch_final_assignment_with_tolerance(
        grouped,
        role_tolerance,
        force_incoming=None,
        max_tier_gap=1,
        status_label="ok"):
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
            if not _tier_allowed(row, max_tier_gap):
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
            assignments[player_id] = (team_id, weight, status_label)
    return assignments

def _solve_batch_final_assignment(grouped):
    attempts = (
        (1, "ok"),
        (2, "ok_relaxed_tier2"),
        (None, "ok_relaxed"),
    )
    force_incoming_attempts = (None,) if _batch_is_incoming(grouped) else (None, True)
    contexts = {}
    for max_tier_gap, status_label in attempts:
        for force_incoming in force_incoming_attempts:
            if force_incoming not in contexts:
                contexts[force_incoming] = _prepare_flexible_assignment_context(grouped, force_incoming)
            assignments = _solve_batch_flexible_assignment(
                grouped,
                max_tier_gap,
                status_label,
                force_incoming=force_incoming,
                context=contexts[force_incoming])
            if assignments is not None:
                return assignments

    for max_tier_gap, status_label in attempts:
        for role_tolerance in AMATEUR_ROLE_BALANCE_TOLERANCES:
            assignments = _solve_batch_final_assignment_with_tolerance(
                grouped,
                role_tolerance,
                max_tier_gap=max_tier_gap,
                status_label=status_label)
            if assignments is not None:
                return assignments
        for role_tolerance in AMATEUR_ROLE_BALANCE_TOLERANCES:
            assignments = _solve_batch_final_assignment_with_tolerance(
                grouped,
                role_tolerance,
                True,
                max_tier_gap=max_tier_gap,
                status_label=status_label)
            if assignments is not None:
                return assignments
    return None

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
