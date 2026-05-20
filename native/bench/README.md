# Native Offline Benchmarks

This folder measures KBO native functions without launching OOTP.

Run:

```powershell
pwsh -ExecutionPolicy Bypass -File native\bench\run_benchmarks.ps1
```

Useful options:

```powershell
pwsh -ExecutionPolicy Bypass -File native\bench\run_benchmarks.ps1 -Samples 400 -Players 20000
pwsh -ExecutionPolicy Bypass -File native\bench\run_benchmarks.ps1 -SnapshotPath latest
pwsh -ExecutionPolicy Bypass -File native\bench\run_benchmarks.ps1 -SnapshotPath C:\path\to\perf_snapshot_players.bin
pwsh -ExecutionPolicy Bypass -File native\bench\run_benchmarks.ps1 -SnapshotPath latest -AnalyzePerf
pwsh -ExecutionPolicy Bypass -File native\bench\run_benchmarks.ps1 -SnapshotPath latest -ReplayPerf
pwsh -ExecutionPolicy Bypass -File native\bench\run_benchmarks.ps1 -SnapshotPath latest -ReplayPerf -Readability virtual
pwsh -ExecutionPolicy Bypass -File native\bench\run_benchmarks.ps1 -BuildOnly
```

By default the benchmark uses `-Readability fast`, which treats the owned
snapshot buffers as known-readable memory and measures the native logic without
Windows `VirtualQuery` noise. Use `-Readability virtual` when you specifically
want to stress the runtime-style readability checks.

To create a real-save snapshot, open the in-game KBO hub, go to
`모드 정보 > 설정`, then press `성능 스냅샷 / 덤프`. The DLL writes
`perf_snapshot_players.bin` and `perf_snapshot_meta.json` under `cache\` inside
the current save-scoped data directory in `%LOCALAPPDATA%\OOTP-KBO\saves\...`.

The benchmark executable builds a synthetic OOTP-like player vector, wires the
same lightweight stubs used by native tests, then records per-function timings
as CSV:

```text
case,iterations,batch,total_ms,avg_us,p50_us,p95_us,max_us
```

Current hot-path cases include:

- `foreign.org_count.cache_hit.*`: top-level foreign org count cache hit.
- `foreign.org_count.snapshot_hit_plus_invalidate.*`: warmed org snapshot reuse
  after invalidating only the per-team count cache.
- `foreign.org_count.snapshot_rebuild.*`: roster-mutation path that rebuilds the
  all-team foreign-count snapshot.
- `foreign.org_count.fresh_scan.*`: raw player-vector scan with caches bypassed.
- `fa.protection_build.cache_hit.*`: FA protected-list materialization from the
  team candidate cache.
- `fa.protection_build.cold.*`: FA protected-list scan/score/sort path with a
  changing league id to avoid cache reuse.

Results are written to `native/bench/results/kbo_bench_*.csv`.

To compare the latest in-game profiler output against the latest offline bench
run:

```powershell
pwsh -ExecutionPolicy Bypass -File tools\analyze-perf-overhead.ps1 -MapPath tools\perf_overhead_map.sample.json -Top 25 -UnmappedTop 25 -GroupDepth 2
```

The report writes three artifacts under `artifacts/perf_studies/`:

- `perf_overhead_*.md`: summary, mapped offline cost estimates, and the most
  expensive profiler zones that still lack an offline bench mapping.
- `perf_overhead_*.zones.csv`: every profiler zone aggregated by call count and
  inclusive time.
- `perf_overhead_*.coverage.csv`: every profiler zone with `mapped` or
  `unmapped` status and the resolved bench case when one exists.

For a one-season replay, pass `-ReplayPerf` or `-ReplayPerfPath C:\path\to\kbo_perf_*.csv`.
The wrapper writes `native/bench/results/kbo_bench_replay_plan_*.csv`,
runs only the mapped benchmark cases, sets each case's `iterations` to the
actual profiler call count, and prints the measured total replay time. The
result CSV is named `kbo_bench_replay_*.csv`.

These benchmarks are for our function cost only. They intentionally exclude OOTP
hook dispatch, UI thread contention, and real save-file timing, so the final
speed check should still be done with the in-game profiler.

The analyzer prints a `bench/profile` ratio for mapped zones. Ratios near `1x`
mean the offline estimate and OOTP profiler agree; large divergences mean the
bench case is useful as a pure-function regression check, but wall-clock
judgment should stay anchored to the OOTP profiler row.

The report also includes mechanical 100% accounting. Every profiler zone is
accounted either by an aligned bench substitution or by profiler fallback, so
the whole-code total remains complete even while benchmark coverage is still
partial.

Coverage is profiler-zone based. A raw C helper that has no profiler zone cannot
be discovered from an OOTP run by this analyzer; it becomes measurable once the
runtime records a zone for the call path or the function is added as an offline
bench case.
