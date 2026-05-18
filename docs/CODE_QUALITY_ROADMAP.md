# OOTP27-KBO-LAUNCHER 코드 품질 개선 로드맵

> 이 문서는 코드 품질과 구조 개선을 위한 로드맵입니다.
> 실제 작업은 한 번에 한 단계씩, 사용자가 명시적으로 시작 지시할 때 진행합니다.
> 기준 문서: [`CONSTITUTION.md`](./CONSTITUTION.md), [`ARCHITECTURE.md`](./ARCHITECTURE.md)

기준 시점: 2026-05-19 KST. 줄 수는 빈 줄 포함 물리 줄 기준이며, 이후 변경으로 달라질 수 있습니다.

진행 상태:

- 2026-05-19: 1단계 원본/산출물 경계 확인 완료. 추적 optimizer 원본은 `tools/kbo_optimizer.py` 하나입니다.
- 2026-05-19: 2단계용 `tests/optimizer/` golden fixture와 `verify-kbo-optimizer-golden.ps1` 추가.
- 2026-05-19: 3단계 1차 완료. Python 도구 상수/오프셋 섹션과 향후 분리 책임을 표시했습니다.
- 2026-05-19: 4단계 시작. optimizer 상수를 `tools/kbo_optimizer_lib/constants.py`로 분리하고 런처/릴리즈/스테이징 복사 경로를 연결했습니다.
- 2026-05-19: 4단계 진행. `kbo_optimizer.py`는 6줄 호환 shim으로 축소했고, CLI/상수/공통 CSV/아시안게임/병역/FA 보상/아마추어 단일 배정/아마추어 배치 배정을 패키지 모듈로 분리했습니다.
- 2026-05-19: 4단계 핵심 완료. `tools/kbo_optimizer_lib/*.py`의 모든 파일이 400줄 아래가 되었고, 빌드 출력본 Python fallback까지 golden 검증했습니다.

---

## 1. 현재 상태

비개발자가 AI와 협업해 만들어온 프로젝트가 관리 런처, 네이티브 패치 DLL, Python 도구까지 포함하는 규모에 도달했습니다. 코드는 동작하지만, AI에게 일을 맡길 때 위험해지는 지점이 분명합니다.

| 항목 | 현재 관찰 | 문제 |
|------|-----------|------|
| 관리 C# | `src/` 31파일, 약 4,304줄. 테스트는 19파일, 약 3,722줄 | 테스트망은 있으나 일부 파일이 정책 임계치 400줄 근처 |
| 네이티브 C | `native/src` + `native/KBOFix.c` 595파일, 약 119,237줄 | 게임 동작에 직결되므로 광범위 리팩터링 금지 |
| Python 도구 | 4파일, 약 3,190줄 | 큰 단일 파일이 많아 AI가 부분만 보고 수정하기 쉬움 |
| `tools/kbo_optimizer.py` | 1,283줄. `git ls-files '*kbo_optimizer.py'` 기준 추적 원본은 이 파일 하나 | 패키지 분리 시 런처/릴리즈/스테이징 경로를 함께 바꿔야 함 |
| 빌드 산출물 | `bin/`, `obj/`, `dist/`, `release_artifacts/`, `artifacts/`는 `.gitignore` 대상 | 산출물 사본은 직접 고치지 않고 원본 또는 빌드 스크립트를 고침 |

**목표**: 게임 동작은 그대로 두고, AI 협업의 안전성과 속도를 끌어올립니다. 헌법의 "진입점은 얇고, 책임 단위는 이름 있는 모듈로 분리" 원칙을 Python 도구와 관리 런처에도 일관되게 적용합니다.

---

## 2. 작업 원칙

1. **외부 동작 동일**: CSV 입출력, CLI 인자, 릴리즈 산출물 이름, 런처 호출 경로는 명시적으로 보존합니다.
2. **한 번에 한 책임 단위**: 모듈 분리, 파일 이동, 소유권 변경을 한 커밋에 섞지 않습니다.
3. **산출물 직접 수정 금지**: `bin/`, `dist/`, `KBOLauncher.Tests/bin/` 안의 복사본은 삭제하거나 재생성할 수는 있어도 패치 대상이 아닙니다.
4. **검증 먼저 설계**: 큰 파일을 나누기 전에 동일 입력/동일 출력 확인 방법을 먼저 만듭니다.
5. **네이티브는 마지막**: 네이티브 C는 크기만으로 건드리지 않습니다. 테스트나 아키텍처 체커로 보호되는 좁은 작업만 허용합니다.

---

## 3. 위험도 맵

| 영역 | 위험 | 깨지면 일어나는 일 | 전략 |
|------|------|--------------------|------|
| 무시된 빌드 산출물 | 낮음 | 다음 빌드 때 덮어써짐 | 직접 편집 금지, 필요시 재생성 |
| Python 분석/최적화 도구 | 낮음-중간 | CSV 산출 또는 런처 보조 기능 실패 | 골든 출력으로 보호하며 정리 |
| `src/KBOLauncher/` C# | 중간 | 런처 실행, DLL 스테이징, 릴리즈 패키징 실패 | `dotnet test`와 기존 패턴으로 보호 |
| `native/` C | 매우 높음 | KBO 규칙 깨짐, 게임 크래시 | 기본적으로 건드리지 않음 |

진행 순서: 산출물/원본 경계 확인 -> Python 검증망 -> Python 구조 개선 -> C# 작은 분리 -> 네이티브 선택 작업.

---

## 4. 단계별 로드맵

### 1단계. 원본과 산출물 경계 고정 (0.5-1일, 위험 낮음)

**무엇**

- `tools/kbo_optimizer.py`가 유일한 추적 원본임을 확인합니다.
- `bin/Debug`, `KBOLauncher.Tests/bin`, `dist`의 `tools/kbo_optimizer.py`는 빌드/릴리즈 산출물로 취급합니다.
- `.gitignore`는 이미 `bin/`, `obj/`, `dist/`, `release_artifacts/`, `artifacts/`를 무시합니다. 이 규칙이 깨진 파일만 정리합니다.
- 릴리즈 경로는 `scripts/release.ps1`, 패키지 검증은 `tests/release/verify-release-artifact.ps1`, 런타임 스테이징은 `src/KBOLauncher/Infrastructure/DllPayloadStager.cs`가 소유합니다.

**왜 먼저**

어느 파일을 고칠지 혼란이 남아 있으면 이후 Python 분리 작업이 산출물 패치로 새어 나갑니다.

**검증**

```powershell
git ls-files '*kbo_optimizer.py'
git status --short -- tools src/KBOLauncher/KBOLauncher.csproj scripts/release.ps1 tests/release/verify-release-artifact.ps1
dotnet build .\OOTP27-KBO-Launcher.sln
```

### 2단계. Python 골든 출력과 CLI 계약 고정 (1-2일, 위험 낮음)

**무엇**

- `tools/kbo_optimizer.py`의 공개 실행 계약을 문서화하거나 테스트로 잠급니다.
  - `kbo_optimizer.py REQUEST_CSV RESULT_CSV`
  - `kbo_optimizer.py --mode MODE REQUEST_CSV RESULT_CSV`
  - `kbo_optimizer.py --server`
- `amateur_assignment`, `asian_games_roster`, `military_selection`, `fa_compensation` 모드별 최소 fixture를 준비합니다.
- 같은 입력으로 나온 결과 CSV가 리팩터링 전후 바이트 단위로 같은지 비교하는 스크립트를 둡니다.
- fixture를 만들 수 없는 모드는 최소한 CLI 인자 파싱과 실패 메시지 형태를 고정합니다.
- 현재 fixture와 golden 출력은 `tests/optimizer/fixtures/`, `tests/optimizer/golden/` 아래에 있습니다.
- `amateur_assignment.csv`는 단일 선수 경로, `amateur_assignment_batch.csv`는 배치 solver 경로를 검증합니다.

**왜**

상수 정리나 파일 분리는 "동작을 바꾸지 않는 작업"이어야 합니다. 이를 증명할 장치가 먼저 필요합니다.

**검증**

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\optimizer\verify-kbo-optimizer-golden.ps1
```

### 3단계. Python 상수와 오프셋 의미화 (2-3일, 위험 낮음)

**무엇**

- `tools/kbo_optimizer.py:11-55`의 가중치/제약 상수를 값 변경 없이 책임별로 묶습니다.
- `native/tools/research_fa_market_memory.py:12-60`의 Win32 상수, OOTP player 오프셋, field probe 목록을 값 변경 없이 구분합니다.
- `tools/analyze_perf_overhead.py`와 `native/tools/research_high_school_record_memory.py`도 큰 Python 도구이므로, 이 단계에서 "나중에 나눌 책임 단위"만 주석 또는 섹션으로 표시합니다.
- `dataclass`는 의미가 분명해질 때만 사용합니다. 단순 숫자 묶음을 과하게 추상화하지 않습니다.

**왜**

분리하기 전에 숫자의 소유권을 알아야 합니다. "어느 모듈에 둘 상수인지"가 먼저 보여야 안전하게 파일을 나눌 수 있습니다.

**검증**

- 2단계 fixture 결과가 바이트 단위로 동일해야 합니다.
- 상수 값 변경이 없는지 `git diff --word-diff`로 확인합니다.

### 4단계. `kbo_optimizer.py` 구조 분리 (3-5일, 위험 낮음-중간)

**무엇**

- `tools/kbo_optimizer.py`는 얇은 호환 진입점으로 남깁니다.
- 구현은 예를 들어 `tools/kbo_optimizer_lib/` 아래 책임별 모듈로 옮깁니다.
  - `cli.py`
  - `amateur_assignment.py`
  - `asian_games_roster.py`
  - `military_selection.py`
  - `fa_compensation.py`
  - `constants.py` (2026-05-19 분리 완료)
  - `csv_io.py`
- 현재 분리된 파일:
  - `tools/kbo_optimizer.py` - 호환 shim
  - `tools/kbo_optimizer_lib/cli.py`
  - `tools/kbo_optimizer_lib/constants.py`
  - `tools/kbo_optimizer_lib/csv_io.py`
  - `tools/kbo_optimizer_lib/amateur_assignment.py`
  - `tools/kbo_optimizer_lib/amateur_common.py`
  - `tools/kbo_optimizer_lib/amateur_batch.py`
  - `tools/kbo_optimizer_lib/amateur_metrics.py`
  - `tools/kbo_optimizer_lib/amateur_roles.py`
  - `tools/kbo_optimizer_lib/amateur_role_capacities.py`
  - `tools/kbo_optimizer_lib/amateur_targets.py`
  - `tools/kbo_optimizer_lib/asian_games_roster.py`
  - `tools/kbo_optimizer_lib/military_selection.py`
  - `tools/kbo_optimizer_lib/fa_compensation.py`
- `tools/kbo_optimizer/`라는 패키지명은 피합니다. 같은 폴더의 `kbo_optimizer.py`와 import 이름이 충돌하기 쉽습니다.

**중요한 동반 변경**

현재 런처와 릴리즈 코드는 `tools/kbo_optimizer.exe`, `tools/kbo_optimizer.py`만 복사합니다. Python 파일을 패키지로 분리하면 다음 파일도 함께 바꿔야 합니다.

- `src/KBOLauncher/KBOLauncher.csproj`: 패키지 `.py` 파일을 출력에 포함
- `scripts/release.ps1`: `tools/kbo_optimizer_lib/` 복사
- `tests/release/verify-release-artifact.ps1`: 필수 패키지 파일 검증
- `src/KBOLauncher/Infrastructure/DllPayloadStager.cs`: 런타임 스테이징 때 패키지 디렉터리도 복사
- `KBOLauncher.Tests/DllPayloadStagerTests.cs`: 패키지 복사 회귀 테스트

이 동반 변경 없이 `kbo_optimizer.py`만 얇게 만들면, `.exe`가 없는 환경에서 Python fallback이 깨질 수 있습니다.

**검증**

```powershell
dotnet test .\OOTP27-KBO-Launcher.sln
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\release.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\release\verify-release-artifact.ps1
```

그리고 2단계 fixture 결과가 동일해야 합니다.

### 5단계. Python 에러 처리와 타입 힌트 (2-3일, 위험 낮음)

**무엇**

- `tools/kbo_optimizer.py` 끝부분의 서버 루프처럼 `except Exception`으로 타입명만 출력하는 부분을 의미 있는 오류로 나눕니다.
- 파일 없음, CSV 파싱 실패, 알 수 없는 mode, 최적화 infeasible 같은 실패를 구분합니다.
- 공개 함수와 새 모듈 경계에 타입 힌트를 추가합니다.
- 사용자에게 노출되는 메시지는 짧고 안정적으로 유지합니다.

**왜**

파일을 나눈 뒤라야 어느 계층의 예외인지 구분할 수 있습니다. 타입 힌트는 이후 AI 수정의 실수도 줄입니다.

**검증**

- 정상 fixture 결과 동일
- 잘못된 CSV, 없는 파일, 알 수 없는 mode에서 메시지가 재현 가능
- `--server`는 한 요청 실패 후 다음 요청을 계속 처리

### 6단계. C# 큰 파일과 책임 경계 정리 (3-5일, 위험 중간)

**무엇**

현재 관리 런처의 큰 production 파일은 다음 정도입니다.

- `src/KBOLauncher/Infrastructure/KboSeedFiles.cs` - 398줄
- `src/KBOLauncher/Infrastructure/KboFlags.cs` - 396줄
- `src/KBOLauncher/Application/LauncherLaunchFlow.cs` - 343줄

우선순위는 `KboSeedFiles.cs`와 `KboFlags.cs`입니다. 이미 `KboSeedFiles.Schedule.cs`, `KboFlags.RuntimeDefinitions.cs` 같은 partial 패턴이 있으므로 그 스타일을 따릅니다.

**주의**

줄 수만 보고 나누지 않습니다. seed 복사, manifest 검증, runtime flag default, JSON 파싱처럼 책임이 실제로 갈라지는 지점만 분리합니다.

**검증**

```powershell
dotnet test .\OOTP27-KBO-Launcher.sln
dotnet build .\OOTP27-KBO-Launcher.sln
```

가능하면 실제 OOTP 실행 또는 attach 흐름도 한 번 확인합니다.

### 7단계. 네이티브는 선택 작업으로만 다룸 (선택, 매우 보수적)

**무엇**

- `native/`는 코드 크기만으로 리팩터링하지 않습니다.
- 먼저 현재 부채를 `tools/check-native-architecture.ps1 -WarnOnly`로 목록화합니다.
- 테스트 없는 정책 함수 하나를 고르고, C 코드 변경 전 native test만 추가하는 방식으로 시작합니다.
- 실제 C 구현 변경은 테스트가 있는 가장 작은 정책 함수 하나로 제한합니다.

**왜**

네이티브는 게임 메모리, 훅, 패치 설치 순서에 직접 연결됩니다. 이 영역은 "보기 좋게 정리"보다 "회귀를 잡는 장치 추가"가 먼저입니다.

**검증**

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\native\tests\run_tests.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\check-native-architecture.ps1 -WarnOnly
pwsh -NoProfile -ExecutionPolicy Bypass -File .\native\build.ps1
dotnet build .\OOTP27-KBO-Launcher.sln
```

실제 규칙 변경이 있었다면 사람이 한 시즌 이상 게임 진행으로 확인합니다.

---

## 5. 우선순위 요약

| 우선순위 | 작업 | 이유 |
|----------|------|------|
| 1 | 원본/산출물 경계 확인 | 산출물 패치 사고 방지 |
| 2 | Python fixture와 CLI 계약 | 이후 모든 리팩터링의 안전망 |
| 3 | Python 상수 의미화 | 값 변경 없이 의도 드러내기 |
| 4 | optimizer 구조 분리 | 가장 큰 AI 협업 병목 해소 |
| 5 | Python 오류/타입 정리 | 실패 원인 추적과 후속 수정 안정화 |
| 6 | C# partial 분리 | 테스트망이 있는 관리 런처 정리 |
| 7 | 네이티브 테스트 보강 | 고위험 영역은 검증망부터 |

---

## 6. 비개발자를 위한 황금 규칙

1. **한 번에 한 단계만** 진행합니다.
2. **작업 지시에는 항상 "외부 동작은 동일해야 한다"를 넣습니다.**
3. **`bin/`, `dist/`, `obj/` 안 파일은 직접 고치지 않습니다.**
4. **각 단계 끝마다 커밋 가능한 상태인지 확인합니다.**
5. **Python 패키지 분리 시 릴리즈/스테이징 복사 경로까지 같이 확인합니다.**
6. **네이티브 C는 테스트 없는 상태로 구조 변경하지 않습니다.**

---

## 7. 주요 파일 경로

- `tools/kbo_optimizer.py`
- `tools/analyze_perf_overhead.py`
- `native/tools/research_fa_market_memory.py`
- `native/tools/research_high_school_record_memory.py`
- `src/KBOLauncher/KBOLauncher.csproj`
- `src/KBOLauncher/Infrastructure/DllPayloadStager.cs`
- `scripts/release.ps1`
- `tests/release/verify-release-artifact.ps1`
- `src/KBOLauncher/Infrastructure/KboSeedFiles.cs`
- `src/KBOLauncher/Infrastructure/KboFlags.cs`
- `src/KBOLauncher/Application/LauncherLaunchFlow.cs`
- `docs/CONSTITUTION.md`
- `docs/ARCHITECTURE.md`
- `KBOLauncher.Tests/`

---

## 8. 다음 한 걸음

이 문서는 실행 계획이 아니라 로드맵입니다. 실제로 시작할 때는 선택한 단계 하나에 대해 별도 세부 계획을 만들고, 그 단계의 검증 명령까지 먼저 확정합니다.

**추천 시작점**: 1단계와 2단계를 묶지 말고 순서대로 진행합니다. 특히 2단계 fixture가 생기기 전에는 `kbo_optimizer.py`를 분리하지 않습니다.
