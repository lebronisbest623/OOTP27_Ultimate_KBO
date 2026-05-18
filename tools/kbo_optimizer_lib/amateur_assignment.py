"""Amateur assignment optimizer mode."""

import csv

from ortools.sat.python import cp_model

from .amateur_batch import optimize_batch_rows
from .amateur_common import _candidate_weight, _team_max_players
from .csv_io import to_int as _to_int

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
