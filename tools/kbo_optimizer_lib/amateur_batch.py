"""Batch amateur-assignment solver."""

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

def _solve_batch_flexible_assignment(grouped, max_tier_gap=1, status_label="ok"):
    total_players = len(grouped)
    if total_players <= 0:
        return {}

    player_percentiles = _player_role_quality_percentiles(grouped)
    team_percentiles = _team_reputation_percentiles(grouped)
    draft_penalties = _batch_draft_penalties(grouped)
    total_teams = max(1, len(team_percentiles))
    incoming_batch = _batch_is_incoming(grouped)
    detailed_roles = _batch_has_detailed_position_buckets(grouped)
    team_info = _collect_batch_team_info(grouped, team_percentiles, incoming_batch, detailed_roles)
    if not team_info:
        return None

    normal_capacity = sum(max(0, info["capacity"]) for info in team_info.values())
    use_incoming_capacity = incoming_batch or normal_capacity < total_players
    incoming_cap = _incoming_team_cap(total_players, len(team_info))

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
    for team_id, info in team_info.items():
        capacity = incoming_cap if use_incoming_capacity else max(0, info["capacity"])
        if capacity <= 0:
            continue
        team_node = new_node()
        team_nodes[team_id] = team_node
        solver.add_arc_with_capacity_and_unit_cost(team_node, sink, capacity, 0)

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
    for max_tier_gap, status_label in attempts:
        assignments = _solve_batch_flexible_assignment(grouped, max_tier_gap, status_label)
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
