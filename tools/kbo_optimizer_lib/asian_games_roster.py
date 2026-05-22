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

ASIAN_GAMES_MILITARY_UNSERVED_BONUS = 2_500_000
ASIAN_GAMES_WILDCARD_UNSERVED_BONUS = 30_000_000
ASIAN_GAMES_WILDCARD_SERVED_PENALTY = 30_000_000


def _role_bucket(row):
    return (row.get("role_bucket") or "").strip().upper()


def _policy_int(rows, key, fallback, min_value=None, max_value=None):
    for row in rows:
        raw = row.get(key)
        if raw is not None and str(raw).strip() != "":
            value = to_int(row, key)
            if min_value is not None:
                value = max(min_value, value)
            if max_value is not None:
                value = min(max_value, value)
            return value
    return fallback


def _row_is_wildcard(row, wildcard_age_min=None):
    raw = row.get("wildcard")
    if raw is not None and str(raw).strip() != "":
        return to_int(row, "wildcard") != 0
    if wildcard_age_min is not None:
        return to_int(row, "age") >= wildcard_age_min
    return to_int(row, "age") > 24


def _row_is_military_unserved(row):
    raw = row.get("military_unserved")
    return raw is not None and str(raw).strip() != "" and to_int(row, "military_unserved") != 0


def _solve_asian_games_model(rows, hard_required_orgs, hard_role_minimums):
    if not rows:
        return None

    roster_size = min(
        _policy_int(rows, "policy_roster_size", ASIAN_GAMES_ROSTER_SIZE, 1, len(rows)),
        len(rows),
    )
    max_wildcards = _policy_int(
        rows,
        "policy_max_wildcards",
        ASIAN_GAMES_MAX_WILDCARDS,
        0,
        roster_size,
    )
    team_max_players = _policy_int(
        rows,
        "policy_team_max_players",
        ASIAN_GAMES_TEAM_MAX_PLAYERS,
        0,
        roster_size,
    )
    wildcard_age_min = _policy_int(
        rows,
        "policy_wildcard_age_min",
        24,
        0,
        80,
    )
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
        if _row_is_wildcard(row, wildcard_age_min):
            wildcard_vars.append(var)

    if wildcard_vars:
        if max_wildcards <= 0:
            model.Add(sum(wildcard_vars) == 0)
        elif len(wildcard_vars) >= max_wildcards and roster_size >= max_wildcards:
            model.Add(sum(wildcard_vars) == max_wildcards)
        else:
            model.Add(sum(wildcard_vars) <= max_wildcards)

    for org_id, org_vars in by_org.items():
        model.Add(sum(org_vars) <= team_max_players)
        if hard_required_orgs and org_id in required_orgs:
            model.Add(sum(org_vars) >= 1)

    objective_terms = []
    for row, var in zip(rows, variables):
        score = to_int(row, "score")
        role = _role_bucket(row)
        org_id = to_int(row, "org_team_id")
        weight = score * 100
        is_wildcard = _row_is_wildcard(row, wildcard_age_min)
        is_unserved = _row_is_military_unserved(row)
        if not is_wildcard:
            weight += 25000
        if is_unserved:
            weight += ASIAN_GAMES_MILITARY_UNSERVED_BONUS
        if is_wildcard and is_unserved:
            weight += ASIAN_GAMES_WILDCARD_UNSERVED_BONUS
        elif is_wildcard:
            weight -= ASIAN_GAMES_WILDCARD_SERVED_PENALTY
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
