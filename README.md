# OOTP27 KBO Launcher

OOTP27은 KBO를 지원하지 않는다. 이 프로젝트가 그 빈자리를 채운다.

C# 런처가 게임 프로세스를 시작하고, C로 작성된 네이티브 패치 DLL을 주입해 KBO 고유 규칙들을 실시간으로 적용한다. OOTP의 소스코드 없이, 실행 중인 바이너리에 직접 훅을 걸어 동작한다.

English documentation: [`README.en.md`](README.en.md)

공식 Discord: [https://discord.gg/sp5Bh6jP](https://discord.gg/sp5Bh6jP)

---

## 디스클레이머

이 프로젝트는 Out of the Park Baseball 27의 비공식 팬 제작 모드/런처다. OOTP Developments, Com2uS, KBO, 각 구단, 선수협 또는 Steam과 제휴하거나 승인받은 프로젝트가 아니다.

이 런처는 실행 중인 OOTP27 프로세스에 네이티브 DLL을 주입하고 게임 메모리와 런타임 동작을 패치한다. 그 특성상 OOTP 업데이트, 실행 환경, 세이브 상태, 백신/보안 정책에 따라 오작동, 충돌, 세이브 손상, 데이터 유실이 발생할 수 있다.

사용자는 반드시 중요한 세이브를 백업한 뒤 사용해야 한다. 사용에 따른 책임은 사용자 본인에게 있다.

상업적 이용, 재배포, 타 프로젝트 포함, 공개 릴리즈 배포 전에는 포함된 폰트, 이미지, 시드 데이터, OOTP 관련 파일 및 기타 자산의 라이선스를 직접 확인해야 한다.

---

## 요구사항

- **OOTP27** (Steam 기준 `ootp27.exe`)
- **.NET 8** — 런처 실행
- **PowerShell 7 이상 (`pwsh`)** — 네이티브 빌드 스크립트 실행
- **GCC (MinGW-w64)** — 네이티브 DLL 빌드
- **WebView2 NuGet 패키지** — F2 허브 UI

---

## 빌드

```powershell
# 네이티브 DLL
pwsh -NoProfile -ExecutionPolicy Bypass -File native\build.ps1

# C# 런처
dotnet build .\OOTP27-KBO-Launcher.sln
```

---

## 실행

```bat
KBOLauncher.exe
```

런처가 OOTP27을 시작하고 KBOFix.dll을 자동 주입한다. 자세한 실행 옵션과 런타임 플래그 설정은 위키를 참고한다.

---

## 아키텍처 및 문서

- 구현 규칙, 설정, 런타임 플래그: **위키**
- 네이티브 레이어 구조와 설계 원칙: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

---

## 라이선스

소스 코드는 **Mozilla Public License 2.0 (MPL-2.0)** 으로 배포된다. 자세한 내용은 [`LICENSE`](LICENSE)를 확인한다.

번들 폰트, 이미지, 시드 데이터, OOTP/KBO 관련 명칭 및 기타 제3자 자산은 각자의 라이선스를 따른다.
