"""Asian Games roster optimizer mode."""

import csv
from collections import defaultdict

from ortools.sat.python import cp_model

from .constants import (
    ASIAN_GAMES_MAX_WILDCARDS,
    ASIAN_GAMES_REQUIRED_ORG_BONUS,
    ASIAN_GAMES_ROLE_DEVIATION_PENALTY,
    ASIAN_GAMES_ROLE_MAXIMUMS,
    ASIAN_GAMES_ROLE_MINIMUMS,
    ASIAN_GAMES_ROLE_TARGETS,
    ASIAN_GAMES_ROSTER_SIZE,
    ASIAN_GAMES_TEAM_MAX_PLAYERS,
)
from .csv_io import to_int

def _role_bucket(row):
    return (row.get("role_bucket") or "").strip().upper()

def _solve_asian_games_model(rows, hard_required_orgs, hard_role_minimums):
    if not rows:
        return None

    roster_size = min(ASIAN_GAMES_ROSTER_SIZE, len(rows))
    model = cp_model.CpModel()
    variables = [model.NewBoolVar(f"player_{to_int(row, 'player_id')}") for row in rows]
    model.Add(sum(variables) == roster_size)

    wildcard_vars = []
    by_role = defaultdict(list)
    by_org = defaultdict(list)
    required_orgs = set()
    for row, var in zip(rows, variables):
        role = _role_bucket(row)
        by_role[role].append(var)
        org_id = to_int(row, "org_team_id")
        if org_id != 0:
            by_org[org_id].append(var)
        if to_int(row, "required_org") != 0 and org_id != 0:
            required_orgs.add(org_id)
        if to_int(row, "age") > 24:
            wildcard_vars.append(var)

    if wildcard_vars:
        model.Add(sum(wildcard_vars) <= ASIAN_GAMES_MAX_WILDCARDS)

    for org_id, org_vars in by_org.items():
        model.Add(sum(org_vars) <= ASIAN_GAMES_TEAM_MAX_PLAYERS)
        if hard_required_orgs and org_id in required_orgs:
            model.Add(sum(org_vars) >= 1)

    objective_terms = []
    for row, var in zip(rows, variables):
        score = to_int(row, "score")
        age = to_int(row, "age")
        role = _role_bucket(row)
        org_id = to_int(row, "org_team_id")
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
    selected.sort(key=lambda item: (-to_int(item[1], "score"), to_int(item[1], "player_id")))
    return selected

def optimize_asian_games_roster(request_path, result_path):
    with open(request_path, newline="", encoding="utf-8") as handle:
        rows = [row for row in csv.DictReader(handle) if to_int(row, "player_id") != 0]

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
            writer.writerow([to_int(row, "player_id"), rank, "selected"])
    return 0
