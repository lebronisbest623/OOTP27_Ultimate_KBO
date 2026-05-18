"""FA compensation protection-list optimizer mode."""

import csv
from collections import defaultdict

from ortools.sat.python import cp_model

from .constants import FA_PROTECTION_ROLE_BONUS
from .csv_io import to_int

def optimize_fa_compensation(request_path, result_path):
    with open(request_path, newline="", encoding="utf-8") as handle:
        rows = [row for row in csv.DictReader(handle) if to_int(row, "player_id") != 0]

    protect_count = max(0, max((to_int(row, "protect_count") for row in rows), default=0))
    protect_count = min(protect_count, len(rows))
    selected_ids = set()
    if protect_count > 0:
        model = cp_model.CpModel()
        variables = [model.NewBoolVar(f"player_{to_int(row, 'player_id')}") for row in rows]
        model.Add(sum(variables) == protect_count)
        objective_terms = []
        by_role = defaultdict(list)
        for row, var in zip(rows, variables):
            role = to_int(row, "role")
            by_role[role].append(var)
            objective_terms.append(var * (to_int(row, "score") * 1000))
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
                to_int(row, "player_id")
                for row, var in zip(rows, variables)
                if solver.Value(var)
            }
        else:
            selected_ids = {
                to_int(row, "player_id")
                for row in sorted(rows, key=lambda row: (-to_int(row, "score"), to_int(row, "player_id")))[:protect_count]
            }

    protected_rows = [row for row in rows if to_int(row, "player_id") in selected_ids]
    unprotected_rows = [row for row in rows if to_int(row, "player_id") not in selected_ids]
    protected_rows.sort(key=lambda row: (-to_int(row, "score"), to_int(row, "player_id")))
    unprotected_rows.sort(key=lambda row: (-to_int(row, "score"), to_int(row, "player_id")))

    with open(result_path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["player_id", "rank", "status"])
        rank = 1
        for row in protected_rows:
            writer.writerow([to_int(row, "player_id"), rank, "protected"])
            rank += 1
        for row in unprotected_rows:
            writer.writerow([to_int(row, "player_id"), rank, "unprotected"])
            rank += 1
    return 0
