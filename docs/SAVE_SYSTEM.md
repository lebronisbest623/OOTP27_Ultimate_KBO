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
  logs\                  large runtime/audit logs
  cache\                 regenerable analysis caches and SQLite copies
  work\                  external solver request/result files
```

The root should not collect feature CSV/TXT/JSONL files. Domain modules should
write durable state through a save-state SQLite table. Large append-only logs,
debug snapshots, OR-Tools exchange files, and regenerable caches can remain as
files under `logs\`, `cache\`, or `work\`.

## Initial Tables

The first migrated subsystem is custom events:

- `custom_event_markers`: processed event idempotency markers.
- `custom_event_runs`: event execution ledger rows.
- `custom_event_state`: small keyed runtime state, currently the calendar
  cursor.

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
