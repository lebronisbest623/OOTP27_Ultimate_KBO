# KBO Save State

The KBO runtime stores mod-owned state beside, not inside, the OOTP `.lg`
directory. The save-scoped root remains:

```text
%LOCALAPPDATA%\OOTP-KBO\saves\<save_id>\
```

New save-scoped gameplay state should use:

```text
kbo_state.sqlite3
```

`save_id` is still derived from the active OOTP save path and the save folder's
filesystem identity by `native/src/core/files/save_paths/scope/save_scoped_data.c`.
The runtime must not guess the newest `.lg` folder as a write destination.

## Layout

For new saves, the target shape is:

```text
<save_id>\
  kbo_state.sqlite3      canonical KBO gameplay state
  config\                save-specific editable seeds and rules
  logs\                  large runtime/audit logs
  cache\                 regenerable analysis caches and SQLite copies
  work\                  external solver request/result files
```

The root should not collect feature CSV/TXT/JSONL files. Domain modules should
write durable state through a save-state SQLite table. Large append-only logs,
debug snapshots, OR-Tools exchange files, and regenerable caches can remain as
files under `logs\`, `cache\`, or `work\`. User-editable save overrides should
live under `config\`.
`kbo_get_save_scoped_data_file` creates intermediate directories for nested
save-scoped paths, so modules can request paths such as `logs\foo.csv` directly.

## Initial Tables

The first migrated subsystems are:

- `custom_event_markers`: processed event idempotency markers.
- `custom_event_runs`: event execution ledger rows.
- `custom_event_state`: small keyed runtime state, currently the calendar
  cursor.
- `foreign_reserve_rights`: exercised KBO foreign-player reserve rights,
  formerly `foreign_waiver_rights.csv`.
- `foreign_waiver_decisions`: retain/skip decision ledger rows, formerly
  `foreign_waiver_decisions.csv`.
- `foreign_waiver_window`: current foreign reserve-right decision window,
  formerly `foreign_waiver_negotiation_window.txt`.
- `foreign_waiver_announcements`: result-news idempotency and body ledger,
  formerly `foreign_waiver_announcements.txt`.
- `amateur_reputation_history`: high-school/college team reputation replay
  history, formerly `amateur_reputation_history.csv`.
- `season_calendar`: observed KBO league opening-day cache, formerly
  `season_calendar.csv`.
- `custom_news_runs`: shared custom-news completion ledger, formerly
  `custom_news_runs.jsonl`.
- `independent_acquisition_window`: independent Futures acquisition open-date
  state, formerly `independent_acquisition_window.txt`.
- `independent_acquisition_ai_cursor`: independent Futures acquisition AI
  processed-date cursor, formerly `independent_acquisition_ai_cursor.txt`.
- `independent_acquisition_requests`: independent Futures acquisition buyer
  request ledger, formerly `independent_acquisition_requests.jsonl`.
- `independent_acquisition_decisions`: independent Futures acquisition seller
  decision ledger, formerly `independent_acquisition_decisions.jsonl`.
- `foreign_roster_daily_audit_state`: daily foreign-roster audit cursor,
  formerly `kbo_daily_audit_state.json`.

The shared SQLite opener lives in:

```text
native/src/core/sql/save_state/
```

It opens `%LOCALAPPDATA%\OOTP-KBO\saves\<save_id>\kbo_state.sqlite3` through
`winsqlite3.dll`, initializes `kbo_schema`, and serializes DB operations inside
the native process.

## Rules

- Memory and hook boundaries remain the authority for live gameplay decisions.
  `kbo_state.sqlite3` is mod-owned persistence, not evidence from OOTP SQL.
- Core owns the SQLite file opener and schema bookkeeping only.
- Domain modules own their own tables and migration decisions.
- Prefer compact relational tables for durable state.
- Keep large logs and temporary solver artifacts out of `kbo_state.sqlite3`.

## Migration Triage

Use this classification when moving existing save-scoped files out of the root.

### Move Into `kbo_state.sqlite3`

These files are durable mod state. They should become domain-owned tables, with
the old CSV/TXT/JSONL names retired for new saves:

- Foreign reserve and waiver state: `foreign_waiver_rights.csv` is retired for
  new saves and now lives in
  `foreign_reserve_rights`; `foreign_waiver_decisions.csv` now lives in
  `foreign_waiver_decisions`; `foreign_waiver_negotiation_window.txt` now lives
  in `foreign_waiver_window`; `foreign_waiver_announcements.txt` now lives in
  `foreign_waiver_announcements`.
- FA state: `fa_declarations.csv`, `fa_filing.csv`, `fa_compensation.csv`,
  `fa_compensation_decisions.csv`, `fa_compensation_cash_transfers.csv`,
  `fa_salary_opening_day_snapshot_YYYY.csv`.
- CBT state: `cbt_records.csv`, `cbt_exception_players.csv`,
  `cbt_cash_charges.csv`, `cbt_opening_days.csv`.
- National-team and service state: `asian_games_roster.csv`,
  `asian_games_roster_history.csv`, `asian_games_tournament_history.csv`,
  `military_service_resolved.csv`, `military_selection_results.csv`.
- Other durable gameplay state: `captains_YYYY.csv`,
  `foreign_injury_replacements.csv`.
  `amateur_reputation_history.csv` is retired for new saves and now lives in
  `amateur_reputation_history`; `season_calendar.csv` now lives in
  `season_calendar`.
- Idempotency and cursors: `custom_news_runs.jsonl` and the news marker files
  now live in `custom_news_runs`; `independent_acquisition_window.txt` now
  lives in `independent_acquisition_window`; `independent_acquisition_ai_cursor.txt`
  now lives in `independent_acquisition_ai_cursor`;
  `independent_acquisition_requests.jsonl` now lives in
  `independent_acquisition_requests`; `independent_acquisition_decisions.jsonl`
  now lives in `independent_acquisition_decisions`;
  `kbo_daily_audit_state.json` now lives in
  `foreign_roster_daily_audit_state`; `*_state.txt` and other small state JSON
  files still need table owners.

### Move Under `logs\`

These are observation, audit, or investigation outputs. They can be large and
should stay outside the main state DB:

- `rule_audit.ndjson`
- `foreign_roster_audit.csv` now writes to `logs\foreign_roster_audit.csv`.
- `domestic_fa_market_investigation.csv` now writes to
  `logs\domestic_fa_market_investigation.csv`.
- `foreign_waiver_candidates.csv` now writes to
  `logs\foreign_waiver_candidates.csv`.
- `fa_compensation_protection_debug.csv` now writes to
  `logs\fa_compensation_protection_debug.csv`.
- `amateur_assignment_debug.csv` now writes to
  `logs\amateur_assignment_debug.csv`.
- `intl_established_fa_postscan.csv` now writes to
  `logs\intl_established_fa_postscan.csv` unless a later implementation proves it is
  required as durable decision state.

### Move Under `cache\`

These files are derived or copied from OOTP data and can be regenerated:

- `fa_market_text_data_<pid>.sqlite3`, `-wal`, and `-shm` now write under
  `cache\`.
- `fa_market_classification.csv` now writes to
  `cache\fa_market_classification.csv`.
- `foreign_roster_snapshot.csv` now writes to
  `cache\foreign_roster_snapshot.csv`.
- `foreign_replacement_players_resolved.csv` now writes to
  `cache\foreign_replacement_players_resolved.csv`.

### Move Under `work\`

These are external solver exchange files. They should not become save-state
tables unless the solver protocol itself changes:

- `*_ortools_request.csv`, `*_ortools_result.csv`,
  `*_ortools_batch_request.csv`, and `*_ortools_batch_result.csv` now write
  under `work\`.

### Move Under `config\`

These are save-specific user-editable inputs or policy overrides. They should
remain file-based unless the UI becomes their editor:

- `*_seed.csv` save overrides now resolve under `config\`, including
  `captain_seed.csv`, `asian_games_schedule_seed.csv`,
  `asian_games_projected_hosts.csv`, `fa_market_cases_seed.csv`,
  `military_service_seed.csv`, `foreign_replacement_players_seed.csv`, and
  `foreign_injury_replacements_seed.csv`.
- `*_policy.json` save overrides now resolve under `config\`, including the
  shared policy reader, `award_schedule_policy.json`, and
  `asian_games_projected_policy.json`.
- `*_rules.json`, `fa_rules.json`, and `cbt_rules.json` save overrides now
  resolve under `config\`.
- News/text template overrides now resolve under `config\news_templates\...`.
- `allstar_teams.csv` first resolves under `config\allstar_teams.csv` for
  save-specific team-split rules.
