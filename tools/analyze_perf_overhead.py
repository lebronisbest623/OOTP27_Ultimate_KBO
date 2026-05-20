#!/usr/bin/env python3
"""Summarize KBO profiler output for one-season overhead studies.

The runtime profiler records inclusive zone timings inside OOTP. This script
aggregates those CSV rows, optionally compares a mod-on run against a baseline
wall-clock run, and can multiply profiler call counts by offline benchmark costs
when a mapping file is supplied.

Future split targets: input discovery/CSV parsing, benchmark mapping, coverage
accounting, Markdown report rendering, and replay-plan export.
"""

from __future__ import annotations

import argparse
import csv
import glob
import json
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from pathlib import Path
from typing import Iterable

from kbo_product import PERF_DIRECTORY_NAME, PERF_FILE_PATTERN, local_data_path


@dataclass
class ZoneStats:
    zone: str
    calls: int = 0
    total_us: int = 0
    slow_calls: int = 0
    max_us: int = 0
    rows: int = 0

    @property
    def avg_us(self) -> float:
        return self.total_us / self.calls if self.calls else 0.0

    @property
    def total_ms(self) -> float:
        return self.total_us / 1000.0


@dataclass
class PerfSummary:
    path: Path
    zones: dict[str, ZoneStats] = field(default_factory=dict)
    first_ts: datetime | None = None
    last_ts: datetime | None = None
    rows: int = 0
    total_calls: int = 0
    total_us: int = 0

    @property
    def duration_seconds(self) -> float:
        if self.first_ts is None or self.last_ts is None:
            return 0.0
        return max(0.0, (self.last_ts - self.first_ts).total_seconds())


@dataclass
class ZoneCoverage:
    stats: ZoneStats
    status: str
    rule_label: str = ""
    bench_case: str = ""
    cost_field: str = ""
    cost_us: float = 0.0
    fidelity: str = ""


def parse_timestamp(text: str) -> datetime:
    return datetime.strptime(text, "%Y-%m-%d %H:%M:%S.%f")


def parse_int(text: str | None) -> int:
    if text is None or text == "":
        return 0
    return int(text)


def latest_path(patterns: Iterable[str]) -> Path | None:
    candidates: list[Path] = []
    for pattern in patterns:
        candidates.extend(Path(path) for path in glob.glob(pattern))
    if not candidates:
        return None
    return max(candidates, key=lambda path: path.stat().st_mtime)


def default_perf_path() -> Path | None:
    perf_pattern = local_data_path(PERF_DIRECTORY_NAME, PERF_FILE_PATTERN)
    if perf_pattern is None:
        return None
    return latest_path([str(perf_pattern)])


def default_bench_path(repo_root: Path) -> Path | None:
    return latest_path([str(repo_root / "native" / "bench" / "results" / "kbo_bench_*.csv")])


def read_perf(path: Path, start: datetime | None, end: datetime | None, last_minutes: float | None) -> PerfSummary:
    raw_rows: list[dict[str, str]] = []
    first_seen: datetime | None = None
    last_seen: datetime | None = None

    with path.open("r", newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            if not row or row.get("timestamp") in (None, "", "timestamp"):
                continue
            try:
                ts = parse_timestamp(row["timestamp"])
            except (KeyError, ValueError):
                continue
            first_seen = ts if first_seen is None or ts < first_seen else first_seen
            last_seen = ts if last_seen is None or ts > last_seen else last_seen
            row["_parsed_ts"] = ts.isoformat(sep=" ")
            raw_rows.append(row)

    if last_minutes is not None and last_seen is not None:
        start = max(start or datetime.min, last_seen - timedelta(minutes=last_minutes))

    summary = PerfSummary(path=path)
    for row in raw_rows:
        ts = datetime.fromisoformat(row["_parsed_ts"])
        if start is not None and ts < start:
            continue
        if end is not None and ts > end:
            continue

        zone = row.get("zone", "").strip()
        if not zone:
            continue
        delta_calls = parse_int(row.get("delta_calls"))
        delta_us = parse_int(row.get("delta_us"))
        delta_slow = parse_int(row.get("delta_slow_calls"))
        max_us = parse_int(row.get("max_us"))

        stats = summary.zones.get(zone)
        if stats is None:
            stats = ZoneStats(zone=zone)
            summary.zones[zone] = stats
        stats.calls += delta_calls
        stats.total_us += delta_us
        stats.slow_calls += delta_slow
        stats.max_us = max(stats.max_us, max_us)
        stats.rows += 1

        summary.rows += 1
        summary.total_calls += delta_calls
        summary.total_us += delta_us
        summary.first_ts = ts if summary.first_ts is None or ts < summary.first_ts else summary.first_ts
        summary.last_ts = ts if summary.last_ts is None or ts > summary.last_ts else summary.last_ts

    return summary


def read_bench(path: Path | None) -> dict[str, dict[str, float]]:
    if path is None or not path.exists():
        return {}
    cases: dict[str, dict[str, float]] = {}
    with path.open("r", newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            case = row.get("case", "").strip()
            if not case:
                continue
            cases[case] = {
                "iterations": float(row.get("iterations", "0") or 0),
                "total_ms": float(row.get("total_ms", "0") or 0),
                "avg_us": float(row.get("avg_us", "0") or 0),
                "p50_us": float(row.get("p50_us", "0") or 0),
                "p95_us": float(row.get("p95_us", "0") or 0),
                "max_us": float(row.get("max_us", "0") or 0),
            }
    return cases


def load_mapping(path: Path | None) -> list[dict[str, str]]:
    if path is None or not path.exists():
        return []
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, list):
        raise ValueError("mapping JSON must be a list")
    return [item for item in data if isinstance(item, dict)]


def group_key(zone: str, depth: int) -> str:
    parts = zone.split(".")
    return ".".join(parts[: max(1, min(depth, len(parts)))])


def zone_matches(zone: str, rule: dict[str, str]) -> bool:
    exact = rule.get("zone")
    prefix = rule.get("zone_prefix")
    if exact:
        return zone == exact
    if prefix:
        return zone.startswith(prefix)
    return False


def mapping_label(rule: dict[str, str]) -> str:
    return rule.get("zone") or rule.get("zone_prefix") or ""


def resolve_bench_case(bench: dict[str, dict[str, float]], rule: dict[str, str]) -> str:
    exact = rule.get("bench_case", "")
    if exact and exact in bench:
        return exact

    prefix = rule.get("bench_case_prefix", "")
    suffix = rule.get("bench_case_suffix", "")
    if not prefix:
        return ""

    candidates = [
        case for case in bench
        if case.startswith(prefix) and (not suffix or case.endswith(suffix))
    ]
    if not candidates:
        return ""

    snapshot_cases = [case for case in candidates if ".snapshot_" in case]
    if snapshot_cases:
        return sorted(snapshot_cases)[0]
    return sorted(candidates)[0]


def build_zone_coverage(
    zones_by_time: list[ZoneStats],
    bench: dict[str, dict[str, float]],
    mapping: list[dict[str, str]],
) -> list[ZoneCoverage]:
    coverage: list[ZoneCoverage] = []
    for stats in zones_by_time:
        row = ZoneCoverage(stats=stats, status="unmapped")
        for rule in mapping:
            if not zone_matches(stats.zone, rule):
                continue

            label = mapping_label(rule)
            bench_case = resolve_bench_case(bench, rule) if bench else ""
            if bench_case and bench_case in bench:
                cost_field = rule.get("cost", "avg_us")
                row = ZoneCoverage(
                    stats=stats,
                    status="mapped",
                    rule_label=label,
                    bench_case=bench_case,
                    cost_field=cost_field,
                    cost_us=bench[bench_case].get(cost_field, 0.0),
                    fidelity=rule.get("fidelity", ""),
                )
                break

            row = ZoneCoverage(
                stats=stats,
                status="matched_rule_missing_bench" if bench else "matched_rule_no_bench_csv",
                rule_label=label,
                fidelity=rule.get("fidelity", ""),
            )
            break
        coverage.append(row)
    return coverage


def format_seconds(seconds: float) -> str:
    if seconds >= 3600:
        return f"{seconds / 3600:.2f}h"
    if seconds >= 60:
        return f"{seconds / 60:.2f}m"
    return f"{seconds:.2f}s"


def markdown_table(headers: list[str], rows: list[list[str]]) -> list[str]:
    if not rows:
        return []
    lines = ["| " + " | ".join(headers) + " |", "| " + " | ".join("---" for _ in headers) + " |"]
    lines.extend("| " + " | ".join(row) + " |" for row in rows)
    return lines


def estimate_ratio_status(estimated_us: float, profiled_us: int) -> str:
    if profiled_us <= 0:
        return "no-profile"
    ratio = estimated_us / float(profiled_us)
    if ratio < 0.1 or ratio > 10.0:
        return "divergent"
    if ratio < 0.5 or ratio > 2.0:
        return "rough"
    return "aligned"


def estimate_ratio_label(estimated_us: float, profiled_us: int) -> tuple[str, str]:
    if profiled_us <= 0:
        return "", "no profiler time"
    ratio = estimated_us / float(profiled_us)
    status = estimate_ratio_status(estimated_us, profiled_us)
    if status == "divergent":
        return f"{ratio:.3g}x", "diverges; use profiler"
    if status == "rough":
        return f"{ratio:.3g}x", "rough"
    return f"{ratio:.3g}x", "aligned"


def accounting_for_row(row: ZoneCoverage) -> tuple[str, float, str]:
    if row.status == "mapped" and row.cost_field:
        estimated_us = row.stats.calls * row.cost_us
        status = estimate_ratio_status(estimated_us, row.stats.total_us)
        if status != "aligned":
            return f"profile_fallback_{status}", float(row.stats.total_us), f"bench/profile {status}"
        return "bench", estimated_us, status
    if row.status == "matched_rule_missing_bench":
        return "profile_fallback_missing_bench", float(row.stats.total_us), "mapping has no bench result"
    if row.status == "matched_rule_no_bench_csv":
        return "profile_fallback_no_bench_csv", float(row.stats.total_us), "no bench csv"
    return "profile_fallback_unmapped", float(row.stats.total_us), "no bench mapping"


def write_zone_csv(path: Path, zones: list[ZoneStats]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["zone", "calls", "total_ms", "avg_us", "max_us", "slow_calls", "rows"])
        for stats in zones:
            writer.writerow([
                stats.zone,
                stats.calls,
                f"{stats.total_ms:.3f}",
                f"{stats.avg_us:.3f}",
                stats.max_us,
                stats.slow_calls,
                stats.rows,
            ])


def write_coverage_csv(path: Path, coverage: list[ZoneCoverage]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "zone",
            "status",
            "mapped_rule",
            "bench_case",
            "bench_cost_field",
            "bench_cost_us",
            "fidelity",
            "estimated_ms",
            "bench_profile_ratio",
            "accounting_source",
            "accounted_ms",
            "accounting_read",
            "calls",
            "total_ms",
            "avg_us",
            "max_us",
            "slow_calls",
            "rows",
        ])
        for row in coverage:
            stats = row.stats
            estimated_ms = stats.calls * row.cost_us / 1000.0 if row.cost_field else 0.0
            ratio = estimated_ms / stats.total_ms if row.cost_field and stats.total_ms > 0 else 0.0
            accounting_source, accounted_us, accounting_read = accounting_for_row(row)
            writer.writerow([
                stats.zone,
                row.status,
                row.rule_label,
                row.bench_case,
                row.cost_field,
                f"{row.cost_us:.6f}" if row.cost_field else "",
                row.fidelity,
                f"{estimated_ms:.3f}" if row.cost_field else "",
                f"{ratio:.6f}" if row.cost_field and stats.total_ms > 0 else "",
                accounting_source,
                f"{accounted_us / 1000.0:.3f}",
                accounting_read,
                stats.calls,
                f"{stats.total_ms:.3f}",
                f"{stats.avg_us:.3f}",
                stats.max_us,
                stats.slow_calls,
                stats.rows,
            ])


def write_replay_plan(path: Path, zones_by_time: list[ZoneStats], mapping: list[dict[str, str]]) -> int:
    plans: dict[tuple[str, str], dict[str, object]] = {}
    for rule in mapping:
        exact_case = rule.get("bench_case", "")
        prefix_case = rule.get("bench_case_prefix", "")
        selector = exact_case or prefix_case
        if not selector:
            continue
        match = "exact" if exact_case else "prefix"
        matched = [stats for stats in zones_by_time if zone_matches(stats.zone, rule)]
        calls = sum(item.calls for item in matched)
        if calls <= 0:
            continue

        key = (selector, match)
        row = plans.get(key)
        if row is None:
            row = {
                "iterations": 0,
                "profiled_us": 0,
                "zones": set(),
                "rules": set(),
            }
            plans[key] = row

        row["iterations"] = int(row["iterations"]) + calls
        row["profiled_us"] = int(row["profiled_us"]) + sum(item.total_us for item in matched)
        zones = row["zones"]
        rules = row["rules"]
        if isinstance(zones, set):
            zones.update(item.zone for item in matched)
        if isinstance(rules, set):
            rules.add(mapping_label(rule))

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "case_selector",
            "match",
            "iterations",
            "profiled_ms",
            "zone_count",
            "zones",
            "rules",
        ])
        for (selector, match), row in sorted(plans.items()):
            zones = row["zones"] if isinstance(row["zones"], set) else set()
            rules = row["rules"] if isinstance(row["rules"], set) else set()
            writer.writerow([
                selector,
                match,
                int(row["iterations"]),
                f"{int(row['profiled_us']) / 1000.0:.3f}",
                len(zones),
                ";".join(sorted(zones)),
                ";".join(sorted(rules)),
            ])
    return len(plans)


def build_report(
    summary: PerfSummary,
    bench_path: Path | None,
    bench: dict[str, dict[str, float]],
    mapping: list[dict[str, str]],
    args: argparse.Namespace,
    zone_csv_path: Path,
    coverage_csv_path: Path,
) -> str:
    zones_by_time = sorted(summary.zones.values(), key=lambda item: item.total_us, reverse=True)
    coverage = build_zone_coverage(zones_by_time, bench, mapping)
    mapped_rows = [row for row in coverage if row.status == "mapped"]
    mapped_zones = {row.stats.zone for row in mapped_rows}
    mapped_cases = {row.bench_case for row in mapped_rows if row.bench_case}
    mapped_rules = {row.rule_label for row in mapped_rows if row.rule_label}
    mapped_calls = sum(row.stats.calls for row in mapped_rows)
    mapped_us = sum(row.stats.total_us for row in mapped_rows)
    mapped_bench_us = sum(row.stats.calls * row.cost_us for row in mapped_rows)
    unmapped_us = max(0.0, float(summary.total_us - mapped_us))
    raw_bench_replacement_us = unmapped_us + mapped_bench_us
    accounting_rows = [(row, *accounting_for_row(row)) for row in coverage]
    bench_accounted = [item for item in accounting_rows if item[1] == "bench"]
    profile_accounted = [item for item in accounting_rows if item[1].startswith("profile_fallback")]
    divergent_rows = [item[0] for item in accounting_rows if item[1] == "profile_fallback_divergent"]
    mapped_profile_fallback = [item for item in profile_accounted if item[0].status == "mapped"]
    unmapped_profile_fallback = [item for item in profile_accounted if item[0].status != "mapped"]
    accounted_calls = sum(item[0].stats.calls for item in accounting_rows)
    accounted_profile_us = sum(item[0].stats.total_us for item in accounting_rows)
    accounted_us = sum(item[2] for item in accounting_rows)
    bench_accounted_profile_us = sum(item[0].stats.total_us for item in bench_accounted)
    bench_accounted_us = sum(item[2] for item in bench_accounted)
    profile_accounted_us = sum(item[2] for item in profile_accounted)
    mapped_profile_fallback_us = sum(item[2] for item in mapped_profile_fallback)
    unmapped_profile_fallback_us = sum(item[2] for item in unmapped_profile_fallback)
    guarded_total_us = accounted_us
    mapped_zone_pct = (len(mapped_zones) * 100.0 / len(summary.zones)) if summary.zones else 0.0
    mapped_call_pct = (mapped_calls * 100.0 / summary.total_calls) if summary.total_calls else 0.0
    mapped_time_pct = (mapped_us * 100.0 / summary.total_us) if summary.total_us else 0.0
    accounted_zone_pct = (len(accounting_rows) * 100.0 / len(summary.zones)) if summary.zones else 0.0
    accounted_call_pct = (accounted_calls * 100.0 / summary.total_calls) if summary.total_calls else 0.0
    accounted_time_pct = (accounted_profile_us * 100.0 / summary.total_us) if summary.total_us else 0.0
    bench_accounted_time_pct = (bench_accounted_profile_us * 100.0 / summary.total_us) if summary.total_us else 0.0
    profile_accounted_time_pct = (profile_accounted_us * 100.0 / summary.total_us) if summary.total_us else 0.0

    groups: dict[str, ZoneStats] = {}
    for stats in zones_by_time:
        key = group_key(stats.zone, args.group_depth)
        group = groups.get(key)
        if group is None:
            group = ZoneStats(zone=key)
            groups[key] = group
        group.calls += stats.calls
        group.total_us += stats.total_us
        group.slow_calls += stats.slow_calls
        group.max_us = max(group.max_us, stats.max_us)
        group.rows += stats.rows
    groups_by_time = sorted(groups.values(), key=lambda item: item.total_us, reverse=True)

    lines: list[str] = []
    lines.append("# KBO Perf Overhead Study")
    lines.append("")
    lines.append(f"- perf: `{summary.path}`")
    if bench_path is not None:
        lines.append(f"- bench: `{bench_path}`")
    lines.append(f"- zone csv: `{zone_csv_path}`")
    lines.append(f"- coverage csv: `{coverage_csv_path}`")
    if summary.first_ts and summary.last_ts:
        lines.append(f"- window: {summary.first_ts} -> {summary.last_ts} ({format_seconds(summary.duration_seconds)})")
    lines.append(f"- profiler rows: {summary.rows:,}")
    lines.append(f"- zones: {len(summary.zones):,}")
    lines.append(f"- inclusive profiled calls: {summary.total_calls:,}")
    lines.append(f"- inclusive profiled time: {format_seconds(summary.total_us / 1_000_000.0)}")
    if summary.duration_seconds > 0:
        pct = (summary.total_us / 10000.0) / summary.duration_seconds
        lines.append(f"- inclusive profiled time / wall window: {pct:.1f}% (nested zones can exceed 100%)")

    lines.append("")
    lines.append("## Offline Bench Coverage")
    lines.append("")
    coverage_rows = [
        ["mapped zones", f"{len(mapped_zones):,} / {len(summary.zones):,}", f"{mapped_zone_pct:.1f}%"],
        ["mapped calls", f"{mapped_calls:,} / {summary.total_calls:,}", f"{mapped_call_pct:.1f}%"],
        ["mapped inclusive time", f"{mapped_us / 1000.0:,.1f} / {summary.total_us / 1000.0:,.1f} ms", f"{mapped_time_pct:.1f}%"],
    ]
    if mapping:
        coverage_rows.append(["mapping rules used", f"{len(mapped_rules):,} / {len(mapping):,}", ""])
    if bench:
        coverage_rows.append(["bench cases used by profiler zones", f"{len(mapped_cases):,} / {len(bench):,}", ""])
    lines.extend(markdown_table(["metric", "value", "coverage"], coverage_rows))

    lines.append("")
    lines.append("## Mechanical 100% Accounting")
    lines.append("")
    accounting_summary_rows = [
        ["accounted zones", f"{len(accounting_rows):,} / {len(summary.zones):,}", f"{accounted_zone_pct:.1f}%"],
        ["accounted calls", f"{accounted_calls:,} / {summary.total_calls:,}", f"{accounted_call_pct:.1f}%"],
        [
            "accounted profiler time",
            f"{accounted_profile_us / 1000.0:,.1f} / {summary.total_us / 1000.0:,.1f} ms",
            f"{accounted_time_pct:.1f}%",
        ],
        [
            "bench-substituted profiler time",
            f"{bench_accounted_profile_us / 1000.0:,.1f} ms -> {bench_accounted_us / 1000.0:,.3f} ms",
            f"{bench_accounted_time_pct:.1f}%",
        ],
        [
            "profile fallback time",
            f"{profile_accounted_us / 1000.0:,.1f} ms",
            f"{profile_accounted_time_pct:.1f}%",
        ],
        ["profile fallback zones", f"{len(profile_accounted):,} / {len(summary.zones):,}", ""],
    ]
    lines.extend(markdown_table(["metric", "value", "coverage"], accounting_summary_rows))

    lines.append("")
    lines.append("## One-Year Whole-Code Time")
    lines.append("")
    whole_rows = [
        [
            "OOTP profiler inclusive total",
            f"{summary.total_us / 1000.0:,.1f} ms",
            format_seconds(summary.total_us / 1_000_000.0),
            "all instrumented zones",
        ],
        [
            "Bench-covered profiler portion",
            f"{mapped_us / 1000.0:,.1f} ms",
            format_seconds(mapped_us / 1_000_000.0),
            f"{mapped_time_pct:.1f}% of profiler total",
        ],
    ]
    if mapped_rows:
        whole_rows.extend([
            [
                "Bench replay for covered calls",
                f"{mapped_bench_us / 1000.0:,.3f} ms",
                format_seconds(mapped_bench_us / 1_000_000.0),
                "pure offline cost for mapped calls",
            ],
            [
                "Profiler-only remainder",
                f"{unmapped_us / 1000.0:,.1f} ms",
                format_seconds(unmapped_us / 1_000_000.0),
                "zones not replaced by bench",
            ],
            [
                "Profile-guarded whole-code estimate",
                f"{guarded_total_us / 1000.0:,.1f} ms",
                format_seconds(guarded_total_us / 1_000_000.0),
                "uses profiler when bench/profile is not aligned",
            ],
            [
                "Mapped profile fallback",
                f"{mapped_profile_fallback_us / 1000.0:,.1f} ms",
                format_seconds(mapped_profile_fallback_us / 1_000_000.0),
                f"{len(mapped_profile_fallback):,} mapped zones kept on profiler",
            ],
            [
                "Unmapped profile fallback",
                f"{unmapped_profile_fallback_us / 1000.0:,.1f} ms",
                format_seconds(unmapped_profile_fallback_us / 1_000_000.0),
                f"{len(unmapped_profile_fallback):,} zones have no bench mapping",
            ],
            [
                "Raw bench replacement",
                f"{raw_bench_replacement_us / 1000.0:,.1f} ms",
                format_seconds(raw_bench_replacement_us / 1_000_000.0),
                "untrusted if divergence table is non-empty",
            ],
        ])
    lines.extend(markdown_table(["metric", "ms", "time", "meaning"], whole_rows))

    if args.baseline_seconds is not None and args.mod_seconds is not None:
        overhead = args.mod_seconds - args.baseline_seconds
        profiled_seconds = summary.total_us / 1_000_000.0
        lines.append("")
        lines.append("## A/B Wall Time")
        lines.append("")
        lines.append(f"- baseline wall: {format_seconds(args.baseline_seconds)}")
        lines.append(f"- mod wall: {format_seconds(args.mod_seconds)}")
        lines.append(f"- measured overhead: {format_seconds(overhead)}")
        lines.append(f"- inclusive profiled sum: {format_seconds(profiled_seconds)}")
        lines.append(f"- residual overhead estimate: {format_seconds(overhead - profiled_seconds)}")

    group_rows = []
    for stats in groups_by_time[: args.top]:
        wall_pct = (stats.total_us / 10000.0 / summary.duration_seconds) if summary.duration_seconds > 0 else 0.0
        group_rows.append([
            stats.zone,
            f"{stats.calls:,}",
            f"{stats.total_ms:,.1f}",
            f"{stats.avg_us:,.1f}",
            f"{wall_pct:.1f}%",
        ])
    lines.append("")
    lines.append("## Top Groups")
    lines.append("")
    lines.extend(markdown_table(["group", "calls", "total_ms", "avg_us", "wall%"], group_rows))

    zone_rows = []
    for stats in zones_by_time[: args.top]:
        wall_pct = (stats.total_us / 10000.0 / summary.duration_seconds) if summary.duration_seconds > 0 else 0.0
        zone_rows.append([
            stats.zone,
            f"{stats.calls:,}",
            f"{stats.total_ms:,.1f}",
            f"{stats.avg_us:,.1f}",
            f"{stats.max_us:,.0f}",
            f"{wall_pct:.1f}%",
        ])
    lines.append("")
    lines.append("## Top Zones")
    lines.append("")
    lines.extend(markdown_table(["zone", "calls", "total_ms", "avg_us", "max_us", "wall%"], zone_rows))

    unmapped_rows = []
    for row in [item for item in coverage if item.status != "mapped"][: args.unmapped_top]:
        stats = row.stats
        wall_pct = (stats.total_us / 10000.0 / summary.duration_seconds) if summary.duration_seconds > 0 else 0.0
        unmapped_rows.append([
            stats.zone,
            row.status,
            f"{stats.calls:,}",
            f"{stats.total_ms:,.1f}",
            f"{stats.avg_us:,.1f}",
            f"{wall_pct:.1f}%",
        ])
    if unmapped_rows:
        lines.append("")
        lines.append("## Top Unmapped Zones")
        lines.append("")
        lines.extend(markdown_table(["zone", "status", "calls", "total_ms", "avg_us", "wall%"], unmapped_rows))

    fallback_rows = []
    for row, source, accounted, read in sorted(profile_accounted, key=lambda item: item[0].stats.total_us, reverse=True)[: args.unmapped_top]:
        stats = row.stats
        wall_pct = (stats.total_us / 10000.0 / summary.duration_seconds) if summary.duration_seconds > 0 else 0.0
        fallback_rows.append([
            stats.zone,
            source,
            read,
            f"{stats.calls:,}",
            f"{accounted / 1000.0:,.1f}",
            f"{wall_pct:.1f}%",
        ])
    if fallback_rows:
        lines.append("")
        lines.append("## Top Profile Fallback Zones")
        lines.append("")
        lines.extend(markdown_table(["zone", "source", "read", "calls", "accounted_ms", "wall%"], fallback_rows))

    if bench:
        bench_total_ms = sum(values.get("total_ms", 0.0) for values in bench.values())
        bench_total_iterations = sum(values.get("iterations", 0.0) for values in bench.values())
        lines.append("")
        lines.append("## Offline Bench Total")
        lines.append("")
        lines.append(f"- measured benchmark iterations: {bench_total_iterations:,.0f}")
        lines.append(f"- measured benchmark time: {bench_total_ms:,.3f} ms ({format_seconds(bench_total_ms / 1000.0)})")

        bench_rows = []
        for case, values in sorted(bench.items(), key=lambda item: item[1].get("avg_us", 0.0), reverse=True):
            bench_rows.append([
                case,
                f"{values.get('iterations', 0):,.0f}",
                f"{values.get('avg_us', 0):,.3f}",
                f"{values.get('p50_us', 0):,.3f}",
                f"{values.get('p95_us', 0):,.3f}",
                f"{values.get('max_us', 0):,.3f}",
            ])
        lines.append("")
        lines.append("## Offline Bench Cases")
        lines.append("")
        lines.extend(markdown_table(["case", "iterations", "avg_us", "p50_us", "p95_us", "max_us"], bench_rows))

    estimate_rows = []
    divergent_rows = []
    if mapping and bench:
        for rule in mapping:
            bench_case = resolve_bench_case(bench, rule)
            if bench_case not in bench:
                continue
            cost_field = rule.get("cost", "avg_us")
            cost_us = bench[bench_case].get(cost_field, 0.0)
            matched = [stats for stats in zones_by_time if zone_matches(stats.zone, rule)]
            if not matched:
                continue
            calls = sum(item.calls for item in matched)
            profiled_us = sum(item.total_us for item in matched)
            estimated_us = calls * cost_us
            label = rule.get("zone") or rule.get("zone_prefix") or ""
            ratio_label, interpretation = estimate_ratio_label(estimated_us, profiled_us)
            fidelity = rule.get("fidelity", "")
            estimate_rows.append([
                label,
                bench_case,
                fidelity,
                cost_field,
                f"{calls:,}",
                f"{estimated_us / 1000.0:,.3f}",
                f"{profiled_us / 1000.0:,.3f}",
                ratio_label,
                interpretation,
            ])
            if interpretation == "diverges; use profiler":
                divergent_rows.append([
                    label,
                    bench_case,
                    f"{estimated_us / 1000.0:,.3f}",
                    f"{profiled_us / 1000.0:,.3f}",
                    ratio_label,
                ])
    if estimate_rows:
        lines.append("")
        lines.append("## Offline Call Count Estimates")
        lines.append("")
        lines.extend(markdown_table(
            ["zone/prefix", "bench_case", "fidelity", "cost", "calls", "estimated_ms", "profiled_ms", "bench/profile", "read"],
            estimate_rows,
        ))
    if divergent_rows:
        lines.append("")
        lines.append("## Bench Divergence")
        lines.append("")
        lines.extend(markdown_table(
            ["zone/prefix", "bench_case", "estimated_ms", "profiled_ms", "bench/profile"],
            divergent_rows,
        ))

    lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append("- Profiler zone times are inclusive and can be nested; do not treat the full sum as exclusive CPU time.")
    lines.append("- For one-season A/B, use the same save and sim conditions for baseline and mod-on runs.")
    lines.append("- Offline benchmark estimates are only as good as the zone-to-bench mapping and snapshot similarity.")
    lines.append("- Mechanical accounting is intentionally 100% by falling back to profiler time for unmapped or non-aligned zones.")
    lines.append("- When bench/profile is not aligned, use the OOTP profiler time for wall-clock judgment and the bench time only for pure-function regression checks.")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Analyze KBO profiler overhead.")
    parser.add_argument("--perf", type=Path, default=None, help="Profiler CSV path. Defaults to latest LOCALAPPDATA/OOTP-KBO/perf file.")
    parser.add_argument("--bench", type=Path, default=None, help="Offline benchmark CSV path. Defaults to latest native/bench/results file.")
    parser.add_argument("--map", type=Path, default=None, help="Optional JSON mapping from profiler zones to bench cases.")
    parser.add_argument("--output", type=Path, default=None, help="Markdown report output path.")
    parser.add_argument("--write-replay-plan", type=Path, default=None, help="Write a bench replay plan from mapped profiler call counts.")
    parser.add_argument("--plan-only", action="store_true", help="Only write the replay plan; do not emit a Markdown report.")
    parser.add_argument("--baseline-seconds", type=float, default=None, help="Baseline OOTP-only one-season wall time.")
    parser.add_argument("--mod-seconds", type=float, default=None, help="Mod-on one-season wall time.")
    parser.add_argument("--start", type=str, default=None, help="Filter start timestamp: YYYY-MM-DD HH:MM:SS.mmm")
    parser.add_argument("--end", type=str, default=None, help="Filter end timestamp: YYYY-MM-DD HH:MM:SS.mmm")
    parser.add_argument("--last-minutes", type=float, default=None, help="Analyze only the last N minutes in the perf CSV.")
    parser.add_argument("--top", type=int, default=25, help="Number of top rows to show.")
    parser.add_argument("--unmapped-top", type=int, default=25, help="Number of unmapped zones to show.")
    parser.add_argument("--group-depth", type=int, default=1, help="Number of dot-separated zone components for group table.")
    args = parser.parse_args()

    perf_path = args.perf or default_perf_path()
    if perf_path is None or not perf_path.exists():
        raise SystemExit("No perf CSV found. Pass --perf or enable profiler and run OOTP.")

    bench_path = args.bench or default_bench_path(repo_root)
    start = parse_timestamp(args.start) if args.start else None
    end = parse_timestamp(args.end) if args.end else None
    summary = read_perf(perf_path, start, end, args.last_minutes)
    bench = read_bench(bench_path)
    mapping = load_mapping(args.map)
    zones_by_time = sorted(summary.zones.values(), key=lambda item: item.total_us, reverse=True)

    out_dir = repo_root / "artifacts" / "perf_studies"
    out_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")

    if args.write_replay_plan is not None:
        plan_count = write_replay_plan(args.write_replay_plan, zones_by_time, mapping)
        print(f"Wrote replay plan {args.write_replay_plan} ({plan_count} entries)")
        if args.plan_only:
            return 0

    output = args.output or (out_dir / f"perf_overhead_{stamp}.md")
    zone_csv_path = output.with_suffix(".zones.csv")
    coverage_csv_path = output.with_suffix(".coverage.csv")
    write_zone_csv(zone_csv_path, zones_by_time)
    write_coverage_csv(coverage_csv_path, build_zone_coverage(zones_by_time, bench, mapping))

    report = build_report(summary, bench_path, bench, mapping, args, zone_csv_path, coverage_csv_path)
    output.write_text(report, encoding="utf-8")
    print(report)
    print(f"\nWrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
