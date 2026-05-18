"""Military service selection optimizer mode."""

import csv
from collections import defaultdict

from ortools.sat.python import cp_model

from .constants import MILITARY_SELECTION_TEAM_SPREAD_BONUS
from .csv_io import to_int

def optimize_military_selection(request_path, result_path):
    with open(request_path, newline="", encoding="utf-8") as handle:
        rows = [row for row in csv.DictReader(handle) if to_int(row, "player_id") != 0]

    slots = max(0, max((to_int(row, "slots") for row in rows), default=0))
    target = min(slots, len(rows))
    if target <= 0:
        selected = []
    else:
        model = cp_model.CpModel()
        variables = [model.NewBoolVar(f"player_{to_int(row, 'player_id')}") for row in rows]
        model.Add(sum(variables) == target)

        by_team = defaultdict(list)
        objective_terms = []
        for row, var in zip(rows, variables):
            team_id = to_int(row, "original_team_id")
            if team_id != 0:
                by_team[team_id].append(var)
            objective_terms.append(var * (to_int(row, "score") * 1000))

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
            selected = sorted(rows, key=lambda row: (-to_int(row, "score"), to_int(row, "player_id")))[:target]

    selected.sort(key=lambda row: (-to_int(row, "score"), to_int(row, "player_id")))
    with open(result_path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["player_id", "rank", "status"])
        for rank, row in enumerate(selected, start=1):
            writer.writerow([to_int(row, "player_id"), rank, "selected"])
    return 0
