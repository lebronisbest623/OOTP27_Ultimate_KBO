"""Command-line and stdin server entry points for the KBO optimizer."""

import sys

from .amateur_assignment import optimize as optimize_amateur_assignment
from .asian_games_roster import optimize_asian_games_roster
from .fa_compensation import optimize_fa_compensation
from .military_selection import optimize_military_selection

def optimize_mode(mode, request_path, result_path):
    if mode == "amateur_assignment":
        return optimize_amateur_assignment(request_path, result_path)
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

def main(argv=None):
    args = sys.argv[1:] if argv is None else argv
    if len(args) == 1 and args[0] == "--server":
        return serve()
    if len(args) == 4 and args[0] == "--mode":
        return optimize_mode(args[1], args[2], args[3])
    if len(args) != 2:
        raise SystemExit("usage: kbo_optimizer.py [--mode MODE] REQUEST_CSV RESULT_CSV")
    return optimize_amateur_assignment(args[0], args[1])
