"""Shared product constants for local KBO tooling."""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[1]
LOCAL_DATA_DIRECTORY_NAME = "OOTP-KBO"
OOTP_EXECUTABLE_NAME = "ootp27.exe"
PERF_DIRECTORY_NAME = "perf"
PERF_FILE_PATTERN = "kbo_perf_*.csv"
CURRENT_SAVE_PATH_FILE_TEMPLATE = "current_save_path_{pid}.txt"
LEAGUE_ROLES_SEED_PATH = REPO_ROOT / "data" / "seeds" / "core" / "league_roles.json"

DEFAULT_HIGH_SCHOOL_LEAGUE_ID = 203


def local_data_path(*parts: str) -> Path | None:
    local_appdata = os.environ.get("LOCALAPPDATA")
    if not local_appdata:
        return None
    return Path(local_appdata) / LOCAL_DATA_DIRECTORY_NAME / Path(*parts)


def load_league_roles() -> dict[str, Any]:
    try:
        with LEAGUE_ROLES_SEED_PATH.open("r", encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, json.JSONDecodeError):
        return {}
    return data if isinstance(data, dict) else {}


def league_role_id(key: str, fallback: int) -> int:
    value = load_league_roles().get(key)
    return value if isinstance(value, int) and value > 0 else fallback
