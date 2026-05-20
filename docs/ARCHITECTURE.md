# OOTP27 KBO Launcher Architecture

This document describes the current structure of the repository. The governing
principles live in `docs/CONSTITUTION.md`; when this file and the constitution
disagree, the constitution wins.

## Runtime Layers

The project has two runtime layers.

- `src/KBOLauncher/Program.cs` is the managed entry point. It delegates to the
  `LauncherApp` application layer, locates OOTP, prepares runtime data, checks
  the target process, and injects the native DLL only when the guards allow it.
- `native/KBOFix.c` is the native patch shell. It includes public module
  headers and assembles native startup order, but gameplay behavior lives in
  named modules under `native/src/`.

Most KBO rule emulation lives in the native layer because the project patches a
closed game process. The architecture therefore favors explicit modules and
fail-closed guards over generic abstractions.

## Current Native Shape

The native layer is now a multi-translation-unit build.

`native/build.ps1` compiles `native/KBOFix.c` and every `*.c` file under
`native/src/`, emits dependency files, and links the resulting objects into
`native/bin/KBOFix.dll`. External WebView2 headers are passed as system headers
so third-party warning noise does not dominate incremental native builds.

The native tree intentionally favors focused translation units over source
fragments. Mechanically named files such as `_part1.c` are not considered a
stable structure; split files should be named for the responsibility they own.
Current source-shape checkpoints: no native `.c/.h` source under
`native/src/` should exceed 400 lines, and folders that need more than four
direct `.c/.h` files should introduce a responsibility-named child folder.

Run `tools/check-native-architecture.ps1` before native refactors or release
packaging to verify these source-shape checkpoints and the broader native
ownership rules. Use `-WarnOnly` when reviewing existing debt without failing
the command. The checker also supports `-Json`, `-Rule <rule-id>`,
`-WriteBaselinePath <path>`, and `-BaselinePath <path>` for CI and debt ratchets.
It also validates that local quoted includes still resolve under owned native
roots (`native/src/` or vendored `native/third_party/`), that one translation
unit does not define the same top-level function twice, and that code does not
continue after an unconditional function-level return. Cross-module includes of
`*_internal.h` are reported as architecture warnings so private contracts do not
silently become public API.

`native/KBOFix.c` should remain a thin shell. Its responsibilities are:

- process-wide preprocessor setup needed by Win32/WebView2 headers
- OOTP ABI and build-verification includes
- public module header includes
- startup and patch-installation ordering

It should not own domain policy, persistence, scanners, UI rendering, or large
helper bodies.

## Native Source Layout

The important native folders are:

```text
native/
  KBOFix.c                         native shell and startup include surface
  build.ps1                        native build/link script
  tests/                           native helper tests
  src/
    bootstrap/                     ABI offsets, typedefs, startup declarations
    build_verify/                  native supported-build verification
    core/                          logging, paths, dates, flags, SQL helpers
    runtime_memory/                process-memory safety helpers
    team/                          team lookup, names, roster-array mutation
    foreign/                       foreign-player policy subsystem
    military_service/              military-service loan subsystem
    custom_events/                 event scheduling, scanning, dispatch
    allstar/                       single-division All-Star support
    fa_*                           FA rules, filing, market, compensation
    amateur_player_quality/        amateur reputation assignment
    season_phase_monitor/          read-only phase monitor
    patch_helpers/                 low-level patch helpers
    hook_stubs/                    generated/executable hook stubs
    patch_installers/              byte patches and detour installers
    hotkey_window/                 in-game F2 hub UI
    entrypoint/                    native startup and runtime install flow
```

New native code should be added as a focused `.c/.h` module under the owning
subsystem. Headers expose the narrow contract; implementation details stay
`static` in the `.c` file.

Tiny compatibility shims that only include an internal header are not useful
modules. Keep public headers for stable entry points, but do not keep empty
`.c` landing files just to preserve an old filename.

## Managed Launcher

`Program.cs` calls `LauncherApp.Run(args)`. `LauncherApp` is a partial
application shell split by flow:

- `LauncherApp.cs` parses options, builds run context, applies guards, and
  delegates to attach or launch flow.
- `LauncherAttachFlow.cs` handles explicit attach paths.
- `LauncherLaunchFlow.cs` handles launch and injection paths.
- `LauncherRunSetup.cs` prepares runtime files and default flag state.
- Infrastructure modules under `src/KBOLauncher/Infrastructure/` own build
  guard reads, process discovery, injection, paths, seed copying, and flag JSON.

The managed layer must not implement KBO gameplay rules. It prepares data and
controls whether the native runtime is allowed to load.

## Supported Build Manifest

Supported OOTP builds are sourced from:

```text
config/ootp-supported-builds.json
```

`tools/generate-supported-builds.ps1` generates:

- `src/KBOLauncher/Infrastructure/OotpSupportedBuilds.Generated.cs`
- `native/src/build_verify/supported_builds.generated.h`
- `native/src/build_verify/supported_builds.generated.c`

`KBOLauncher.Tests/OotpSupportedBuildManifestTests.cs` verifies that the JSON,
managed generated source, and native generated source match.

The remaining build-related debt is the build-specific RVA mapping in
`native/src/build_verify/build_verify.c`. The supported-build list is generated,
but the per-build RVA map is still hand-maintained C code. Future OOTP build
updates should move those mappings into generated data as well.

## Safety Guards

KBOFix injection is fail-closed.

- The launcher reads the target `ootp27.exe` PE header and checks timestamp plus
  image size before injection.
- Explicit injection requests fail visibly on unsupported or unreadable builds.
- Default no-argument runs do not inject into unsupported targets.
- The native DLL repeats build verification if loaded by another path.
- Injection requires the current `.lg` save to contain the Ultimate KBO roster
  marker in `description.txt`.

The required marker is:

```text
https://github.com/lebronisbest623/OOTP27_Ultimate_KBO
```

The native save-scoped path helper follows the same fail-closed rule: it uses
the current save path reported by OOTP memory. It must not guess the newest save
folder as an output destination.

## Runtime Flags

Runtime booleans live in:

```text
%LOCALAPPDATA%\OOTP-KBO\kbo_flags.json
```

Native callers read canonical JSON keys from `kbo_flags.json`; callers may pass
old `.txt`-shaped names only as a local normalization convenience.
Status files, command files, seeds, CSVs, and save-scoped persistence remain
separate files. New save-scoped gameplay state should move toward the
mod-owned `kbo_state.sqlite3` described in `docs/SAVE_SYSTEM.md`.

The F2 hub intentionally exposes recovery flags so users can disable risky
runtime paths without editing JSON by hand. Runtime flag metadata is sourced
from:

```text
config/kbo-runtime-flags.json
```

`tools/generate-runtime-flags.ps1` generates:

- `src/KBOLauncher/Infrastructure/KboFlags.RuntimeDefinitions.cs`
- `native/src/hotkey_window/views/mod/runtime_flags/runtime_flags.generated.h`
- `native/src/hotkey_window/views/mod/runtime_flags/runtime_flags.generated.c`

`KBOLauncher.Tests/KboRuntimeFlagManifestTests.cs` verifies that the manifest,
managed defaults, and native F2 UI flag table match.

## Native Subsystems

### Core

`native/src/core/` owns shared infrastructure only: logging, atomic writes,
date/text helpers, SQL escaping, save-scoped paths, runtime flags, league
context, league-event helpers, and news/history helper APIs.

Core must not accumulate feature policy. If a helper knows KBO domain rules, it
belongs in the owning subsystem.

Core uses the following internal grouping:

- `dates/`: current-date extraction and text/date conversion helpers
- `events/`: shared league-event lookup helpers
- `files/`: atomic writes, message-body temp files, and save-scoped paths.
  `save_paths/` is split into `platform/` Win32 UTF-8 path helpers,
  `current/` current-save discovery, and `scope/` save-scoped data paths.
- `sql/save_state/`: the mod-owned save-state SQLite opener for
  `%LOCALAPPDATA%\OOTP-KBO\saves\<save_id>\kbo_state.sqlite3`.
- `logging/`: native log output and profiling bridge
- `news/`: live-news emission, news-object creation, and history stubs
- `sql/`: SQL escaping plus league-news/history transaction helpers
- `teams/`: shared team collection helpers
- `core_flags/`: runtime flag metadata and local-app-data flag reads.
  `api/` is split into `runtime/` thread lifecycle controls and `settings/`
  numeric/UI settings.
- `core_league_context_parts/`: league context lookup and event-manager access

### Foreign Player Policy

`native/src/foreign/` owns foreign-player reserve rights, signability and offer
blocking, Asian quota behavior, injury replacement slots, replacement-player
seed resolution, and foreign roster diagnostics.

Key subfolders:

- `common/`: CSV parsing, path/date/config helpers, waiver policy, player
  evaluation, and shared event declarations
- `rights/`: exercised reserve-right storage, loading, mutation, and query
- `signability/`: signability, offer, FA candidate, and no-minor-contract probes
- `replacement_seed/`: replacement-player seed parsing and resolution
- `injury/`: injury replacement slot lifecycle
- `quota/`: Asian quota, active count, and call-up limit policy
- `roster_audit/`: read-only roster diagnostics and snapshots
- `waiver_core/`: waiver scanner, AI candidate logic, CSV candidate writing,
  and top-candidate selection
- `waiver_decisions/`: retain/skip command handling, team resolution, and
  decision persistence
- `waiver_window/`: waiver window state, status, opening, and event-window
  checks
- `waiver_outputs/`: user-visible waiver announcements and close-window
  results
- `intl_established_fa/`: established foreign FA generation/multiplier wrappers
- `intl_established_fa_postscan/`: post-generation visibility and quality pass

Foreign-player event-window logic decides when a retain/skip decision is legal.
Rights storage owns exercised rights after a valid retain decision exists. The
two concepts must not be collapsed.

### Military Service

`native/src/military_service/` owns military-service candidate queues, seed
assignment, active service-loan records, player service-day state, annual
selection events, return logic, and hook wrappers.

Seed storage and player movement are separate responsibilities. Queueing a
candidate does not move the player; movement happens in seed assignment or the
selection-event path.

The military subsystem uses the following internal grouping:

- `runtime/`: service-day ticking, assignment orchestration, thread startup,
  shared runtime state, and hook wrappers
- `players/`: active service-loan records, player service state, team-add
  guards, and service-team policy
- `selection/`: annual selection events, draft queues, FA policy, and selection
  news
- `returns/`: player return flow and return-history persistence
- `seed/`: seed parsing, seed paths, and seed-registry storage
- `calendar/`: service-day and current-date helpers

The root folder keeps `military_service.h` as the public facade.

### Custom Events

`native/src/custom_events/` owns event-name matching, event scheduling,
processed-event markers, due-event scanning, and dispatch into owning domain
handlers.

The scanner owns timing and idempotency. Domain modules own behavior. New custom
events should add a title matcher and a dispatch target rather than expanding
scanner logic inline.

The custom-event subsystem uses the following internal grouping:

- `runtime/`: catalog loading, event names, lookup, markers, scanner,
  dispatcher, monitor, and shared custom-event declarations
- `asian_games/`: Asian Games state, schedule creation, roster store, and
  player evaluation
- `asian_games_lifecycle/`: selection-result application, departure,
  replacement, and final-return flows
- `asian_games_selection/`: roster selection and wildcard/missing-org helpers
- `asian_games_schedule_seed/`: Asian Games schedule seed persistence, parsing,
  import, and query helpers
- `asian_games_news/`: Asian Games news body, link, emit, and handler helpers
- `schedules/`: foreign-priority and offseason-transition event scheduling
- `diagnostics/`: custom-event-adjacent read-only diagnostics

The root folder should not accumulate scanner, dispatcher, or domain behavior.
Runtime event title keys and schedule offsets live in
`data/seeds/custom_events/custom_event_catalog.json`; native code should read
that catalog instead of adding new event constants inline.

### FA and Amateur Modules

The FA-adjacent modules are split by responsibility:

- `fa_rules/`: FA rule config loading and query helpers
- `fa_filing/`: FA filing persistence and lookup
- `fa_market_classification/`: market classification, cache, and CSV output
- `fa_salary_snapshot/`: opening-day salary snapshot observation
- `fa_compensation/`: compensation records, protected lists, due tasks, and news
- `amateur_player_quality/`: amateur reputation assignment and diagnostics

These modules should continue moving toward smaller files where one file owns
one lifecycle or data table.

### All-Star

`native/src/allstar/` owns single-division All-Star team setup, voting/event
hooks, schedule dates, candidate-team seeding, and related CSV parsing.

### Patch Installers and Hook Stubs

`native/src/patch_installers/` owns byte-pattern verification and patch
installation. It should not contain gameplay policy.

Patch installers are grouped by patch surface:

- `allstar/`: All-Star static/team/event/settings/candidate patch installers
- `foreign/`: foreign-player signability, counts, call-up, trade, submit-offer,
  and international-established-FA patch installers
- `military/`: military-service patch installers
- `season_phase/`: season-phase probe patch installers
- `no_minor_contracts/`: no-minor-contract and offer-demand patch installers
- `arbitration/`: arbitration offer/non-tender/no-withdraw patch installers

`no_minor_contracts/` is split by patch responsibility:

- `common/`: shared no-minor patch helpers
- `callbacks/`: offer, contract, and player-action callback detours
- `demand_floors/`: offer-demand, demand-write, foreign-FA baseline, and
  submit-salary floor patches
- `scan/`: scanner and string patches
- `experimental/`: historical folder name for official no-minor runtime flag
  wiring, offer UI hooks, and write-site patch orchestration

The root folder should remain a namespace only; new patch installers belong in
the patch-surface subfolder they modify.

`native/src/hook_stubs/` owns executable call stubs and ABI wrappers. Wrappers
should pass arguments to domain modules and translate results back into OOTP ABI
shape; policy should stay in the domain modules.

### F2 Hotkey Window

`native/src/hotkey_window/` owns the in-game F2 hub UI. It may:

- display runtime state
- select league/team/player context
- write command requests
- call narrow domain query APIs
- expose recovery flags for user safety

It must not directly mutate gameplay tables or OOTP player/team memory.

The F2 hub uses the following internal grouping:

- `runtime/`: Win32/WebView runtime. `content/` owns HTML assembly,
  selection/navigation helpers, player badges, skin bitmap use, and team-color
  helpers; `webview/` owns COM setup, command routing, and lifetime handling;
  `window/` owns window state and Win32 window procedures.
- `views/`: feature-specific HTML views split by visible hub tab:
  `asian_games/`, `fa/`, `foreign/`, `military/`, and `mod/`.
- `support/`: shared UI helpers split by support concern. `assets/` owns names,
  logos, image sources, asset paths, nations, and uniform numbers; `text/` owns
  language/date/string/text-buffer helpers; `roster/` owns roster cells, table
  CSS, and sort script; `skin/` owns scrollbar and skin metrics; `skin_assets/`
  owns bitmap drawing helpers.
- `state/`: selected league/team state lookup and display caches
- `ui_html_helpers/`: small HTML helper routines that are shared by multiple
  views

The top-level file explosion is gone and the broad `hotkey_window_runtime_internal.h`
has been replaced by narrower contracts: `hotkey_window_domain_contract.h`,
`hotkey_window_runtime_shared.h`, and per-area internals under `content/`,
`webview/`, and `window/`.

## Thread Lifecycle

Background loops should:

- be started by the module that owns the loop
- check `kbo_runtime_threads_should_continue()`
- sleep through `kbo_runtime_sleep_should_continue()`
- clear their own started flag before exit

`DllMain` requests runtime thread shutdown on process detach. The project does
not currently join every detached thread because most thread handles are closed
after creation. Any new thread owner should make shutdown behavior explicit and
avoid introducing loops that cannot observe the runtime stop flag.

## Known Architectural Debt

These are the current high-cost areas. They are not new precedent; they are the
next cleanup targets.

1. Runtime flag booleans are manifest-generated, but broader F2 command routes
   and numeric settings remain hand-maintained.
2. Supported-build rows are generated, but build-specific RVA maps are still
   hand-maintained.
3. `foreign/signability/` still carries internal headers across its subfolders
   (`foreign_policy/internal/`, `submit_offer_probe/internal/`). Folder-level
   ownership exists but the broad private contracts have not been narrowed.
4. `amateur_player_quality/` internal headers have been split into
   `amateur_assignment_internal.h` and `amateur_reputation_internal.h`, but
   both remain broad relative to the assignment and reputation lifecycles.
5. `fa_market_classification/` internal headers have been split into
   `fa_market_data_internal.h` and `fa_market_policy_internal.h`. Further
   narrowing can follow the same pattern as other resolved areas.
6. Source-fragment include files have been removed from native source. New
   native work should stay in owned `.c/.h` modules, including generated tables.

## Migration Plan

Do not resume broad mechanical migration just to move lines around. The next
steps should close one responsibility boundary at a time:

1. Split remaining broad domain `_internal.h` contracts into state, lifecycle,
   command, scanner, or IO-owned headers. Current targets: `foreign/signability/`.
2. Move build-specific RVA data into generated sources.
3. Continue splitting or regrouping domain modules only when the new file owns a clear
   lifecycle, table, scanner, policy, or parser.
4. Remove temporary `_internal.h` headers when their declarations can move to
   narrow owned headers without creating cycles.

Each code-change commit should close one subsystem or one responsibility unit.
Docs-only commits may describe multiple known debts.

## Verification

After native or launcher changes, run:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File native\build.ps1
dotnet build .\OOTP27-KBO-Launcher.sln
dotnet test .\OOTP27-KBO-Launcher.sln
pwsh -NoProfile -ExecutionPolicy Bypass -File native\tests\run_tests.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\check-native-architecture.ps1 -WarnOnly
```

CI runs managed build/test, native build, and native helper tests on Windows.
Season replay scenarios live under `tests/scenarios/season_replay/` and are
covered by `KBOLauncher.Tests/SeasonReplayHarnessTests.cs`. Current replay
coverage includes foreign-priority custom event scheduling, preseason captain
selection, custom-event offseason transition detection, captain maintenance
trigger decisions, and in-season captain repair.

## Naming Rules

- Use `reserve_rights` for exercised KBO foreign-player rights.
- Use `priority_events` for decision windows and league events.
- Use `waiver_ai` only for automatic retain/skip decisions.
- Use `foreign_policy` or `asian_quota` for roster-limit behavior.
- Avoid using `waiver` as a catch-all term for KBO foreign-player rules.

## Design Rules

- Gameplay state changes live in domain modules, not UI modules.
- UI modules can submit requests, but must not directly mutate gameplay tables.
- Event modules decide when a decision is legal.
- AI modules decide what action to take, not how rights are stored.
- Audit modules observe only.
- Patch installers patch bytes; they do not own policy.
- Hook wrappers stay thin and delegate to domain modules.
- CSV files are module-owned persistence.
- Thread start functions are explicit and owned by the loop's module.
- Public wrapper names are stable unless the corresponding patch installer is
  updated in the same change.

## Repository Data

Bundled seed/config data lives under `data/seeds/`. Runtime setup copies those
files into `%LOCALAPPDATA%\OOTP-KBO\`. Native modules read save-scoped data
first, then global local-app-data inputs as fallback. New durable gameplay
state should use the save-scoped `kbo_state.sqlite3`; generated/cache/output
files should be written to the save scope or the owning module's documented
path, not guessed global destinations.

## Open Questions

- Should `foreign_waiver_rights.csv` be renamed to
  `foreign_reserve_rights.csv`, or kept for compatibility?
- Should F2 user actions remain command-file based long term, or move to a
  narrower in-memory request API?
- Should numeric foreign-policy config values move into a structured manifest?
- Should WebView command routes be generated or table-driven once the F2 hub is
  split?
