# OOTP27 KBO Launcher

OOTP27 does not natively support the Korean Baseball Organization ruleset. This project fills that gap.

The launcher starts or attaches to an OOTP27 process, prepares KBO data files, and injects a native patch DLL written in C. The native layer hooks the running game binary and applies KBO-specific rules at runtime without access to OOTP source code.

Korean documentation: [`README.md`](README.md)

Official Discord: [https://discord.gg/sp5Bh6jP](https://discord.gg/sp5Bh6jP)

---

## Disclaimer

This is an unofficial fan-made launcher/mod for Out of the Park Baseball 27. It is not affiliated with, endorsed by, sponsored by, or approved by OOTP Developments, Com2uS, KBO, any KBO club, any players' association, or Steam.

This project injects a native DLL into a running OOTP27 process and patches game memory and runtime behavior. OOTP updates, unsupported builds, local machine differences, antivirus/security policy, or unexpected save state may cause crashes, broken behavior, save corruption, or data loss.

Back up important saves before using this project. Use at your own risk.

Before commercial use, redistribution, inclusion in another project, or public release packaging, verify the licenses for bundled fonts, images, seed data, OOTP-related files, and any other included assets.

---

## Requirements

- **OOTP27** (`ootp27.exe`, Steam layout supported)
- **.NET 8** for the launcher
- **PowerShell 7 or newer (`pwsh`)** for native build scripts
- **GCC / MinGW-w64** for the native DLL
- **Microsoft WebView2 NuGet package** for the in-game F2 hub UI

---

## Build

```powershell
# Native DLL
pwsh -NoProfile -ExecutionPolicy Bypass -File native\build.ps1

# C# launcher
dotnet build .\OOTP27-KBO-Launcher.sln
```

---

## Run

```bat
KBOLauncher.exe
```

The launcher starts OOTP27 and injects KBOFix.dll automatically. For detailed run options and runtime flag configuration, see the wiki.

---

## Architecture and Docs

- Implemented rules, configuration, and runtime flags: **wiki**
- Native layer structure and design principles: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

---

## License

Source code is distributed under the **Mozilla Public License 2.0 (MPL-2.0)**. See [`LICENSE`](LICENSE) for the full license text.

Bundled fonts, images, seed data, OOTP/KBO names, and other third-party assets remain subject to their own licenses.
