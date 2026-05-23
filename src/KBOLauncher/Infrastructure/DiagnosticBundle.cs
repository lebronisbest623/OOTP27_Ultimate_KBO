using System.Diagnostics;
using System.IO.Compression;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;

internal static class DiagnosticBundle
{
    private const long MaxIncludedFileBytes = 12L * 1024L * 1024L;
    private const int MaxRecentProblemLines = 240;

    private static readonly string[] LocalDiagnosticFilePatterns =
    [
        OotpProduct.LauncherLogFileName,
        "runtime.ndjson*",
        "rule_audit.ndjson*",
        "foreign_*.ndjson*",
        "launcher_*_status.txt",
        "current_save_path_*.txt",
    ];

    private static readonly string[] ProblemLineNeedles =
    [
        "fatal",
        "failed",
        "fail",
        "blocked",
        "unsupported",
        "unreadable",
        "exception",
        "crash",
        "crashed_on_startup",
        "inject_failed",
        "inject_blocked",
        "already_loaded_stale",
        "marker_missing",
        "save_not_completed",
        "current_save_unavailable",
        "process_module_unreadable",
        "WebView2 process failed",
        "WebView2 environment create failed",
        "WebView2 environment start failed",
        "WebView2Loader.dll load failed",
        "KBO FRONT OFFICE HTML UI FAILED",
        "StackOverflowException",
    ];

    private sealed record CheckLine(string Status, string Name, string Detail);

    public static string Create(LauncherOptions options, string? reason = null)
    {
        var outputDirectory = Path.Combine(OotpProduct.LocalDataDirectory, "diagnostics");
        return Create(options, OotpProduct.LocalDataDirectory, outputDirectory, reason);
    }

    public static string CreateForFatal(Exception ex)
    {
        var options = new LauncherOptions(
            OotpPath: null,
            DllPath: null,
            AttachPid: null,
            AttachExisting: false,
            EnableForeignWaiverAi: null,
            EnableSingleDivisionAllstarEvents: null,
            DryRun: false,
            AllowSecondInstance: false,
            Diagnostics: true,
            ShowHelp: false,
            OotpArgs: []);

        return Create(options, $"fatal {ex.GetType().Name}: {ex.Message}");
    }

    internal static string Create(
        LauncherOptions options,
        string localDataDirectory,
        string outputDirectory,
        string? reason = null)
    {
        Directory.CreateDirectory(localDataDirectory);
        Directory.CreateDirectory(outputDirectory);

        var stamp = DateTimeOffset.Now.ToString("yyyyMMdd_HHmmss");
        var zipPath = Path.Combine(outputDirectory, $"Ultimate_KBO_diagnostics_{stamp}.zip");
        if (File.Exists(zipPath))
        {
            zipPath = Path.Combine(outputDirectory, $"Ultimate_KBO_diagnostics_{stamp}_{Guid.NewGuid():N}.zip");
        }

        var addedEntryNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        using var archive = ZipFile.Open(zipPath, ZipArchiveMode.Create);

        var report = BuildReport(options, localDataDirectory, reason);
        AddText(archive, addedEntryNames, "README_FIRST.txt", BuildReadme(report.Checks));
        AddText(archive, addedEntryNames, "ACTION_REQUIRED.txt", report.ActionText);
        AddText(archive, addedEntryNames, "diagnostics_summary.json", report.SummaryJson);
        AddText(archive, addedEntryNames, "TROUBLESHOOTING_KO.txt", BuildTroubleshootingGuide());
        AddText(archive, addedEntryNames, "self_check.txt", FormatChecks(report.Checks));
        AddText(archive, addedEntryNames, "recent_problem_lines.txt", report.RecentProblemText);
        AddText(archive, addedEntryNames, "local_data_inventory.txt", report.LocalDataInventoryText);
        AddText(archive, addedEntryNames, "ootp_user_data_report.txt", report.OotpUserDataText);
        AddText(archive, addedEntryNames, "support_request_template.txt", report.SupportTemplateText);
        AddText(archive, addedEntryNames, "privacy_notice.txt", BuildPrivacyNotice());
        AddText(archive, addedEntryNames, "environment.txt", report.EnvironmentText);
        AddText(archive, addedEntryNames, "ootp_path_candidates.txt", report.PathCandidatesText);
        AddText(archive, addedEntryNames, "processes.txt", report.ProcessesText);
        AddText(archive, addedEntryNames, "current_save_paths.txt", BuildCurrentSavePathReport(localDataDirectory));

        AddKnownLocalFiles(archive, addedEntryNames, localDataDirectory);
        AddRecentPerfFiles(archive, addedEntryNames, localDataDirectory);

        return zipPath;
    }

    private sealed record DiagnosticReport(
        List<CheckLine> Checks,
        string EnvironmentText,
        string PathCandidatesText,
        string ProcessesText,
        string ActionText,
        string SummaryJson,
        string RecentProblemText,
        string LocalDataInventoryText,
        string OotpUserDataText,
        string SupportTemplateText);

    private static DiagnosticReport BuildReport(LauncherOptions options, string localDataDirectory, string? reason)
    {
        var checks = new List<CheckLine>();
        var environment = new List<string>
        {
            $"generated_at={DateTimeOffset.Now:O}",
            $"reason={reason ?? ""}",
            $"launcher_version={Assembly.GetExecutingAssembly().GetName().Version}",
            $"launcher_process_path={Environment.ProcessPath ?? ""}",
            $"launcher_base_dir={AppContext.BaseDirectory}",
            $"current_directory={Environment.CurrentDirectory}",
            $"os={RuntimeInformation.OSDescription}",
            $"framework={RuntimeInformation.FrameworkDescription}",
            $"process_arch={RuntimeInformation.ProcessArchitecture}",
            $"os_arch={RuntimeInformation.OSArchitecture}",
            $"is_64bit_process={Environment.Is64BitProcess}",
            $"user_interactive={Environment.UserInteractive}",
            $"local_data_dir={localDataDirectory}",
            $"explicit_ootp={options.OotpPath ?? ""}",
            $"explicit_dll={options.DllPath ?? ""}",
            $"attach_pid={options.AttachPid?.ToString() ?? ""}",
            $"attach_existing={options.AttachExisting}",
            $"dry_run={options.DryRun}",
            $"allow_second_instance={options.AllowSecondInstance}",
        };

        checks.Add(CheckLocalDataWritable(localDataDirectory));
        checks.Add(CheckFileExists(Path.Combine(localDataDirectory, OotpProduct.LauncherLogFileName), "launcher.log"));
        checks.Add(CheckFileExists(Path.Combine(localDataDirectory, "runtime.ndjson"), "runtime.ndjson"));
        checks.Add(CheckFileExists(Path.Combine(localDataDirectory, OotpProduct.FlagsFileName), OotpProduct.FlagsFileName));

        var resolvedOotp = LauncherPaths.ResolveOotpPath(options.OotpPath);
        if (resolvedOotp is null)
        {
            checks.Add(new CheckLine("WARN", "OOTP executable", $"Not found. Use --ootp or set {OotpProduct.OotpEnvironmentVariables[1]}."));
            environment.Add("resolved_ootp=");
        }
        else
        {
            environment.Add($"resolved_ootp={resolvedOotp}");
            var buildInfo = OotpBuildGuard.Read(resolvedOotp);
            var knownBuild = OotpBuildGuard.FindKnownBuild(buildInfo);
            environment.Add(OotpBuildGuard.FormatLogStatus(buildInfo, knownBuild));
            checks.Add(CheckOotpBuild(buildInfo, knownBuild));
        }

        var defaultDll = options.DllPath ?? LauncherPaths.ResolveDefaultKboFixDllPath();
        checks.Add(defaultDll is not null && File.Exists(defaultDll)
            ? new CheckLine("PASS", "KBOFix.dll", defaultDll)
            : new CheckLine("WARN", "KBOFix.dll", "Default DLL not found next to launcher. Build or reinstall the mod."));
        checks.Add(CheckWebView2Loader(defaultDll, localDataDirectory));

        var processesText = BuildProcessReport(resolvedOotp);
        checks.Add(CheckRecentLauncherSignals(localDataDirectory));
        checks.Add(CheckRecentRuntimeSignals(localDataDirectory));
        checks.Add(CheckCurrentSavePath(localDataDirectory));
        checks.Add(CheckRosterMarkerGuardStatus(localDataDirectory));
        checks.Add(CheckOotpCrashFlagFiles());
        checks.Add(CheckCrashStartupSignal(localDataDirectory));

        var recentProblemText = BuildRecentProblemReport(localDataDirectory);
        var localDataInventoryText = BuildLocalDataInventory(localDataDirectory);
        var ootpUserDataText = BuildOotpUserDataReport();

        return new DiagnosticReport(
            checks,
            string.Join(Environment.NewLine, environment) + Environment.NewLine,
            BuildPathCandidatesReport(options.OotpPath),
            processesText,
            BuildActionRequired(checks, recentProblemText),
            BuildSummaryJson(checks, localDataDirectory, reason),
            recentProblemText,
            localDataInventoryText,
            ootpUserDataText,
            BuildSupportTemplate(checks, localDataDirectory, reason));
    }

    private static CheckLine CheckLocalDataWritable(string localDataDirectory)
    {
        try
        {
            Directory.CreateDirectory(localDataDirectory);
            var probe = Path.Combine(localDataDirectory, $".diagnostic_write_probe_{Guid.NewGuid():N}.tmp");
            File.WriteAllText(probe, "ok");
            File.Delete(probe);
            return new CheckLine("PASS", "Local data directory writable", localDataDirectory);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return new CheckLine("FAIL", "Local data directory writable", $"{localDataDirectory} ({ex.GetType().Name}: {ex.Message})");
        }
    }

    private static CheckLine CheckFileExists(string path, string name)
    {
        return File.Exists(path)
            ? new CheckLine("PASS", name, path)
            : new CheckLine("WARN", name, $"Missing: {path}");
    }

    private static CheckLine CheckOotpBuild(OotpBuildInfo buildInfo, OotpSupportedBuild? knownBuild)
    {
        if (!buildInfo.Ok)
        {
            return new CheckLine("FAIL", "OOTP build readable", buildInfo.Error ?? "unknown error");
        }

        if (knownBuild is null)
        {
            return new CheckLine(
                "FAIL",
                "OOTP build supported",
                $"Unsupported timestamp=0x{buildInfo.Timestamp:X8} size_of_image=0x{buildInfo.SizeOfImage:X8}");
        }

        return knownBuild.NativePatchesSupported
            ? new CheckLine("PASS", "OOTP build supported", knownBuild.Label)
            : new CheckLine("WARN", "OOTP build supported", $"{knownBuild.Label} is metadata-only; native injection may be blocked.");
    }

    private static CheckLine CheckRecentLauncherSignals(string localDataDirectory)
    {
        var logPath = Path.Combine(localDataDirectory, OotpProduct.LauncherLogFileName);
        var recent = ReadRecentText(logPath, 800);
        if (recent.Length == 0)
        {
            return new CheckLine("WARN", "Launcher injection signal", "No launcher.log lines available.");
        }

        if (recent.Contains("inject_failed", StringComparison.OrdinalIgnoreCase))
        {
            return new CheckLine("FAIL", "Launcher injection signal", "Recent launcher.log contains inject_failed.");
        }

        if (recent.Contains("inject_blocked", StringComparison.OrdinalIgnoreCase))
        {
            return new CheckLine("WARN", "Launcher injection signal", "Recent launcher.log contains inject_blocked. Check roster marker/build status.");
        }

        if (recent.Contains("inject_complete", StringComparison.OrdinalIgnoreCase)
                || recent.Contains("already_loaded", StringComparison.OrdinalIgnoreCase))
        {
            return new CheckLine("PASS", "Launcher injection signal", "Recent launcher.log shows injection completed or KBOFix already loaded.");
        }

        return new CheckLine("WARN", "Launcher injection signal", "No recent inject_complete/already_loaded signal found.");
    }

    private static CheckLine CheckRecentRuntimeSignals(string localDataDirectory)
    {
        var recent = ReadRecentRuntimeText(localDataDirectory, 1200);
        if (recent.Length == 0)
        {
            return new CheckLine("WARN", "Runtime signal", "No runtime.ndjson lines available. KBOFix may not have loaded.");
        }

        if (recent.Contains("WebView2 process failed", StringComparison.OrdinalIgnoreCase)
                || recent.Contains("KBO FRONT OFFICE HTML UI FAILED", StringComparison.OrdinalIgnoreCase))
        {
            return new CheckLine("FAIL", "F2 hub signal", "Runtime log contains WebView2/F2 failure.");
        }

        if (recent.Contains("WebView2 F2 rights UI ready", StringComparison.OrdinalIgnoreCase)
                || recent.Contains("KBO F2 hub ready", StringComparison.OrdinalIgnoreCase))
        {
            return new CheckLine("PASS", "F2 hub signal", "Runtime log shows F2 hub ready.");
        }

        return new CheckLine("WARN", "F2 hub signal", "No F2 hub ready line found in recent runtime logs.");
    }

    private static CheckLine CheckWebView2Loader(string? dllPath, string localDataDirectory)
    {
        var candidates = new List<string>();
        if (!string.IsNullOrWhiteSpace(dllPath))
        {
            try
            {
                var dllDirectory = Path.GetDirectoryName(Path.GetFullPath(dllPath));
                if (!string.IsNullOrWhiteSpace(dllDirectory))
                {
                    candidates.Add(Path.Combine(dllDirectory, "WebView2Loader.dll"));
                }
            }
            catch (Exception ex) when (ex is ArgumentException or NotSupportedException or PathTooLongException)
            {
            }
        }

        candidates.Add(Path.Combine(localDataDirectory, "run_dlls", "WebView2Loader.dll"));

        var existing = candidates.FirstOrDefault(File.Exists);
        return existing is not null
            ? new CheckLine("PASS", "WebView2Loader.dll", existing)
            : new CheckLine("WARN", "WebView2Loader.dll", "Not found next to KBOFix.dll or in staged run_dlls. F2 hub may fail to open.");
    }

    private static CheckLine CheckCurrentSavePath(string localDataDirectory)
    {
        var files = Directory.Exists(localDataDirectory)
            ? Directory.EnumerateFiles(localDataDirectory, "current_save_path_*.txt")
                .Select(path => new FileInfo(path))
                .Where(file => file.Exists)
                .OrderByDescending(file => file.LastWriteTimeUtc)
                .ToArray()
            : [];
        if (files.Length == 0)
        {
            return new CheckLine("WARN", "Current save cache", "No current_save_path file found. Open a save and run the launcher again.");
        }

        var latest = files[0];
        var savePath = SafeReadAllText(latest.FullName).Trim();
        if (string.IsNullOrWhiteSpace(savePath))
        {
            return new CheckLine("WARN", "Current save cache", $"{latest.Name} is empty.");
        }

        RosterMarkerInfo info;
        try
        {
            info = KboRosterMarkerGuard.CheckSavePath(savePath, minSaveCompletedAt: null, allowIncompleteMarkedSave: true);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
        {
            return new CheckLine("FAIL", "Current save cache", $"{latest.Name} could not be checked: {ex.GetType().Name}: {ex.Message}");
        }

        if (info.Ok)
        {
            return new CheckLine("PASS", "Current save cache", $"{files.Length} current_save_path file(s) found. Latest save is {info.Status}: {info.SavePath}");
        }

        return new CheckLine("FAIL", "Current save cache", $"{latest.Name} points to an invalid KBO save ({info.Status}): {info.Error}");
    }

    private static CheckLine CheckRosterMarkerGuardStatus(string localDataDirectory)
    {
        var path = Path.Combine(localDataDirectory, OotpProduct.RosterMarkerGuardStatusFileName);
        var recent = ReadRecentText(path, 120);
        if (recent.Length == 0)
        {
            return new CheckLine("WARN", "Roster marker guard", $"Missing: {path}");
        }

        if (recent.Contains("kbo_injection=disabled", StringComparison.OrdinalIgnoreCase)
                || recent.Contains("status=marker_missing", StringComparison.OrdinalIgnoreCase)
                || recent.Contains("status=description_missing", StringComparison.OrdinalIgnoreCase)
                || recent.Contains("status=save_not_completed", StringComparison.OrdinalIgnoreCase)
                || recent.Contains("status=current_save_unavailable", StringComparison.OrdinalIgnoreCase))
        {
            return new CheckLine("FAIL", "Roster marker guard", "Recent guard status blocked injection. Check launcher_roster_marker_guard_status.txt.");
        }

        if (recent.Contains("kbo_injection=allowed", StringComparison.OrdinalIgnoreCase)
                || recent.Contains("status=marked_save_completed", StringComparison.OrdinalIgnoreCase)
                || recent.Contains("status=marked_save_in_progress", StringComparison.OrdinalIgnoreCase))
        {
            return new CheckLine("PASS", "Roster marker guard", "Recent guard status allows KBOFix injection.");
        }

        return new CheckLine("WARN", "Roster marker guard", "Guard status exists but did not contain a clear allowed/blocked result.");
    }

    private static CheckLine CheckOotpCrashFlagFiles()
    {
        var existing = EnumerateOotpCrashFlagCandidates().FirstOrDefault(File.Exists);
        return existing is not null
            ? new CheckLine("FAIL", "OOTP crashed_on_startup flag file", $"Crash startup flag candidate exists: {existing}")
            : new CheckLine("INFO", "OOTP crashed_on_startup flag file", "No crashed_on_startup flag candidate found in known OOTP user-data locations.");
    }

    private static CheckLine CheckCrashStartupSignal(string localDataDirectory)
    {
        if (!Directory.Exists(localDataDirectory))
        {
            return new CheckLine("INFO", "OOTP crash flag signal", "Local data directory is missing.");
        }

        foreach (var file in EnumerateRecentLocalFiles(localDataDirectory).Take(20))
        {
            var recent = ReadRecentText(file.FullName, 800);
            if (recent.Contains("crashed_on_startup", StringComparison.OrdinalIgnoreCase))
            {
                return new CheckLine("FAIL", "OOTP crash flag signal", $"Recent log mentions crashed_on_startup in {RelativeTo(localDataDirectory, file.FullName)}.");
            }
        }

        return new CheckLine("INFO", "OOTP crash flag signal", "No crashed_on_startup line found in recent launcher/runtime logs.");
    }

    private static string BuildReadme(IEnumerable<CheckLine> checks)
    {
        var lines = new List<string>
        {
            "Ultimate KBO diagnostics",
            "",
            "1. Open ACTION_REQUIRED.txt first.",
            "2. Open self_check.txt for the raw check result.",
            "3. Open recent_problem_lines.txt to see the suspicious log lines.",
            "4. If you are reporting a bug, send this whole ZIP.",
            "5. This ZIP does not include your save folder. It only includes logs, local mod settings, and environment reports.",
            "",
            "진단 ZIP입니다.",
            "- ACTION_REQUIRED.txt: 먼저 볼 요약/조치",
            "- diagnostics_summary.json: 개발자/자동분석용 구조화 요약",
            "- TROUBLESHOOTING_KO.txt: 흔한 실패별 설명",
            "- self_check.txt: 원본 체크 결과",
            "- recent_problem_lines.txt: 실패 가능성이 큰 로그 줄",
            "- ootp_user_data_report.txt: OOTP 사용자 데이터/크래시 플래그 후보",
            "- support_request_template.txt: 그대로 붙여서 보낼 보고 양식",
            "",
            "Quick status:",
        };
        lines.AddRange(checks.Select(c => $"{c.Status,-4} {c.Name}: {c.Detail}"));
        lines.Add("");
        lines.Add("Common next steps:");
        lines.Add("- OOTP executable WARN/FAIL: pass --ootp \"C:\\path\\to\\ootp27.exe\" or set OOTP27_DIR.");
        lines.Add("- OOTP build unsupported: update OOTP/mod or report the build timestamp and size.");
        lines.Add("- Injection blocked: confirm the opened save is the Ultimate KBO quickstart and has completed saving once.");
        lines.Add("- F2 hub missing: include this ZIP in the report.");
        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static string BuildActionRequired(IEnumerable<CheckLine> checks, string recentProblemText)
    {
        var checkList = checks.ToList();
        var failures = checkList.Where(c => c.Status.Equals("FAIL", StringComparison.OrdinalIgnoreCase)).ToList();
        var warnings = checkList.Where(c => c.Status.Equals("WARN", StringComparison.OrdinalIgnoreCase)).ToList();
        var lines = new List<string>
        {
            "Ultimate KBO 자가진단 결과",
            "",
            "이 파일부터 보면 됩니다. ZIP 자체는 세이브 폴더를 포함하지 않고, 런처/모드 로그와 환경 정보만 담습니다.",
            "",
        };

        if (failures.Count > 0)
        {
            lines.Add("[판정]");
            lines.Add("- 이 ZIP에서는 FAIL 항목이 보입니다. 아래 항목이 실제 실패 원인일 가능성이 큽니다.");
            lines.Add("");
            lines.Add("[즉시 확인]");
            lines.AddRange(failures.Select(c => $"- {c.Name}: {DescribeLikelyCause(c)} / {c.Detail}"));
        }
        else if (warnings.Count > 0)
        {
            lines.Add("[판정]");
            lines.Add("- 확정 실패는 아니지만 WARN 항목이 있습니다. 이 항목부터 확인하세요.");
            lines.Add("");
            lines.Add("[주의]");
            lines.AddRange(warnings.Select(c => $"- {c.Name}: {DescribeLikelyCause(c)} / {c.Detail}"));
        }
        else
        {
            lines.Add("[상태]");
            lines.Add("- 큰 문제 신호는 self-check에서 바로 보이지 않습니다.");
        }

        lines.Add("");
        lines.Add("[바로 해볼 것]");
        var actions = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var check in failures.Concat(warnings))
        {
            foreach (var action in DescribeActions(check))
            {
                if (actions.Add(action))
                {
                    lines.Add($"- {action}");
                }
            }
        }

        if (actions.Count == 0)
        {
            lines.Add("- 문제가 계속되면 support_request_template.txt 내용을 채우고 이 ZIP 전체를 보내 주세요.");
        }

        lines.Add("");
        lines.Add("[의심 로그]");
        lines.Add(recentProblemText.Contains("No known problem lines", StringComparison.OrdinalIgnoreCase)
            ? "- 최근 로그에서 정해진 실패 키워드는 잡히지 않았습니다."
            : "- recent_problem_lines.txt에 실패/차단/크래시 의심 줄이 모였습니다.");
        lines.Add("");
        lines.Add("[더 자세히]");
        lines.Add("- TROUBLESHOOTING_KO.txt에는 실패 유형별 설명이 있습니다.");
        lines.Add("- diagnostics_summary.json은 개발자에게 보낼 때 자동 분류용으로 쓰입니다.");
        lines.Add("");
        lines.Add("그래도 해결이 안 되면 support_request_template.txt 내용을 붙여서 보내고, 이 ZIP 전체를 첨부해 주세요.");
        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static string BuildSummaryJson(IEnumerable<CheckLine> checks, string localDataDirectory, string? reason)
    {
        var checkList = checks.ToList();
        var actionable = checkList
            .Where(c => c.Status.Equals("FAIL", StringComparison.OrdinalIgnoreCase)
                    || c.Status.Equals("WARN", StringComparison.OrdinalIgnoreCase))
            .Select(c => new
            {
                severity = c.Status,
                component = c.Name,
                likely_cause = DescribeLikelyCause(c),
                detail = c.Detail,
                next_actions = DescribeActions(c).ToArray(),
            })
            .ToArray();

        var payload = new
        {
            generated_at = DateTimeOffset.Now,
            reason = reason ?? "",
            worst_status = WorstStatus(checkList),
            local_data_dir = localDataDirectory,
            actionable_count = actionable.Length,
            checks = checkList.Select(c => new { status = c.Status, name = c.Name, detail = c.Detail }).ToArray(),
            likely_causes = actionable,
            important_files = new[]
            {
                "ACTION_REQUIRED.txt",
                "TROUBLESHOOTING_KO.txt",
                "self_check.txt",
                "recent_problem_lines.txt",
                "ootp_user_data_report.txt",
                "environment.txt",
                "processes.txt",
                "current_save_paths.txt",
            },
        };

        return JsonSerializer.Serialize(payload, new JsonSerializerOptions { WriteIndented = true }) + Environment.NewLine;
    }

    private static IEnumerable<string> DescribeActions(CheckLine check)
    {
        if (check.Name.Contains("OOTP executable", StringComparison.OrdinalIgnoreCase))
        {
            yield return "OOTP 실행 파일을 못 찾으면 런처를 OOTP 설치 폴더에서 실행하거나 --ootp \"C:\\path\\to\\ootp27.exe\"로 명시하세요.";
        }

        if (check.Name.Contains("OOTP build", StringComparison.OrdinalIgnoreCase))
        {
            yield return "OOTP 빌드가 지원 목록과 다르면 OOTP/모드를 같은 릴리즈 조합으로 맞추고, self_check.txt의 timestamp/size_of_image 값을 함께 보내세요.";
        }

        if (check.Name.Contains("KBOFix.dll", StringComparison.OrdinalIgnoreCase)
                || check.Name.Contains("WebView2Loader", StringComparison.OrdinalIgnoreCase))
        {
            yield return "KBOFix.dll 또는 WebView2Loader.dll이 없으면 릴리즈 ZIP을 다시 풀고, 실행 위치가 bin/Debug 같은 임시 빌드 폴더인지 확인하세요.";
        }

        if (check.Name.Contains("Launcher injection", StringComparison.OrdinalIgnoreCase))
        {
            yield return "inject_failed/inject_blocked가 보이면 OOTP를 완전히 끄고, Ultimate KBO 퀵스타트 세이브를 연 뒤 저장이 끝난 다음 런처를 다시 실행하세요.";
        }

        if (check.Name.Contains("Roster marker", StringComparison.OrdinalIgnoreCase))
        {
            yield return "로스터 마커가 막혔으면 현재 열린 세이브가 Ultimate KBO 퀵스타트인지, description.txt에 required_marker가 있는지 확인하세요.";
        }

        if (check.Name.Contains("Current save", StringComparison.OrdinalIgnoreCase))
        {
            yield return "current_save_path가 없으면 OOTP에서 세이브를 완전히 로드하고 한 번 저장한 뒤 런처를 다시 실행하세요.";
        }

        if (check.Name.Contains("F2 hub", StringComparison.OrdinalIgnoreCase))
        {
            yield return "F2 허브가 실패하면 recent_problem_lines.txt와 runtime.ndjson를 같이 보내세요. WebView2 관련 실패면 WebView2 Runtime 설치 상태도 확인해야 합니다.";
        }

        if (check.Name.Contains("crash flag", StringComparison.OrdinalIgnoreCase))
        {
            yield return "crashed_on_startup 문구가 있으면 OOTP를 모드 없이 한 번 정상 실행/종료해서 플래그를 지운 뒤 다시 런처로 실행해 보세요.";
        }

        if (check.Name.Contains("crashed_on_startup", StringComparison.OrdinalIgnoreCase))
        {
            yield return "crashed_on_startup 플래그 파일이 남아 있으면 OOTP가 시작 직후 한 번 닫힐 수 있습니다. OOTP를 모드 없이 정상 실행/종료한 뒤 다시 시도하세요.";
        }

        if (check.Name.Contains("Local data", StringComparison.OrdinalIgnoreCase))
        {
            yield return "%LOCALAPPDATA%\\OOTP-KBO에 쓰기 권한이 없으면 백신/권한/동기화 폴더 차단을 확인하세요.";
        }
    }

    private static string DescribeLikelyCause(CheckLine check)
    {
        if (check.Name.Contains("OOTP executable", StringComparison.OrdinalIgnoreCase))
        {
            return "런처가 OOTP 실행 파일 위치를 찾지 못했습니다.";
        }

        if (check.Name.Contains("OOTP build", StringComparison.OrdinalIgnoreCase))
        {
            return "현재 OOTP 실행 파일이 이 모드가 검증한 빌드와 다릅니다.";
        }

        if (check.Name.Contains("KBOFix.dll", StringComparison.OrdinalIgnoreCase))
        {
            return "네이티브 패치 DLL이 릴리즈/빌드 산출물에 없습니다.";
        }

        if (check.Name.Contains("WebView2Loader", StringComparison.OrdinalIgnoreCase))
        {
            return "F2 허브를 띄우는 WebView2 로더 파일이 없습니다.";
        }

        if (check.Name.Contains("Launcher injection", StringComparison.OrdinalIgnoreCase))
        {
            return "런처가 OOTP 프로세스에 KBOFix를 주입하지 못했거나 안전장치가 차단했습니다.";
        }

        if (check.Name.Contains("Runtime signal", StringComparison.OrdinalIgnoreCase))
        {
            return "KBOFix 런타임 로그가 없어서 DLL 로드 여부가 불명확합니다.";
        }

        if (check.Name.Contains("F2 hub", StringComparison.OrdinalIgnoreCase))
        {
            return "KBOFix는 로드됐지만 F2 허브/WebView2 쪽에서 실패했을 가능성이 큽니다.";
        }

        if (check.Name.Contains("Current save", StringComparison.OrdinalIgnoreCase))
        {
            return "런처가 현재 열린 세이브를 Ultimate KBO 세이브로 확인하지 못했습니다.";
        }

        if (check.Name.Contains("Roster marker", StringComparison.OrdinalIgnoreCase))
        {
            return "현재 세이브가 Ultimate KBO 퀵스타트로 표시되어 있지 않거나 저장 완료 전입니다.";
        }

        if (check.Name.Contains("crashed_on_startup", StringComparison.OrdinalIgnoreCase)
                || check.Name.Contains("crash flag", StringComparison.OrdinalIgnoreCase))
        {
            return "OOTP의 이전 크래시 플래그가 남아 런처 흐름을 끊고 있을 수 있습니다.";
        }

        if (check.Name.Contains("Local data", StringComparison.OrdinalIgnoreCase))
        {
            return "런처가 로컬 데이터 폴더에 로그/설정을 쓸 수 없습니다.";
        }

        return "추가 확인이 필요한 진단 항목입니다.";
    }

    private static string BuildSupportTemplate(IEnumerable<CheckLine> checks, string localDataDirectory, string? reason)
    {
        var checkList = checks.ToList();
        var lines = new List<string>
        {
            "아래 내용을 그대로 붙여서 보내세요.",
            "",
            "[증상]",
            "- F2 안 뜸 / 런처가 꺼짐 / OOTP crash flag / 기타:",
            "",
            "[언제 발생했는지]",
            "- OOTP 실행 직후 / 세이브 로드 후 / F2 누른 뒤 / 시즌 진행 중:",
            "",
            "[자가진단 요약]",
            $"reason={reason ?? ""}",
            $"worst_status={WorstStatus(checkList)}",
            $"launcher_base_dir={AppContext.BaseDirectory}",
            $"local_data_dir={localDataDirectory}",
        };

        lines.AddRange(checkList
            .Where(c => !c.Status.Equals("PASS", StringComparison.OrdinalIgnoreCase)
                    && !c.Status.Equals("INFO", StringComparison.OrdinalIgnoreCase))
            .Take(12)
            .Select(c => $"- {c.Status} {c.Name}: {c.Detail}"));

        lines.Add("");
        lines.Add("[첨부]");
        lines.Add("- Ultimate_KBO_diagnostics_*.zip 전체");
        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static string BuildTroubleshootingGuide()
    {
        var lines = new List<string>
        {
            "Ultimate KBO 문제 해결 가이드",
            "",
            "1. ACTION_REQUIRED.txt에서 FAIL이 있으면 그 항목부터 처리하세요.",
            "2. FAIL이 없고 WARN만 있으면 WARN 중 OOTP build, KBOFix.dll, Roster marker, F2 hub 순서로 보세요.",
            "3. 계속 안 되면 support_request_template.txt를 채우고 ZIP 전체를 보내세요.",
            "",
            "[OOTP executable]",
            "- 런처가 ootp27.exe를 못 찾은 상태입니다.",
            "- OOTP 설치 폴더에서 실행하거나 --ootp \"C:\\path\\to\\ootp27.exe\"를 사용하세요.",
            "",
            "[OOTP build supported]",
            "- 현재 OOTP 빌드가 모드가 아는 빌드가 아닙니다.",
            "- OOTP 업데이트 직후라면 모드 업데이트가 필요할 수 있습니다.",
            "",
            "[KBOFix.dll / WebView2Loader.dll]",
            "- 릴리즈 ZIP을 일부만 풀었거나 실행 위치가 잘못됐을 가능성이 큽니다.",
            "- 특히 F2가 안 뜨면 WebView2Loader.dll 존재 여부가 중요합니다.",
            "",
            "[Current save cache / Roster marker guard]",
            "- 현재 열린 세이브가 Ultimate KBO 퀵스타트인지 확인하는 안전장치입니다.",
            "- 세이브를 열고 저장이 완전히 끝난 뒤 런처를 다시 실행하세요.",
            "",
            "[Launcher injection signal]",
            "- DLL 주입 단계에서 실패했거나 안전장치가 차단한 상태입니다.",
            "- OOTP를 완전히 종료한 뒤 다시 시도하고, 계속되면 launcher.log를 포함한 ZIP을 보내세요.",
            "",
            "[F2 hub signal]",
            "- DLL은 로드됐지만 F2 허브/WebView2 UI가 실패한 상태입니다.",
            "- runtime.ndjson와 recent_problem_lines.txt를 보면 WebView2 실패 원인이 남아 있을 수 있습니다.",
            "",
            "[OOTP crashed_on_startup]",
            "- OOTP가 이전 크래시 플래그 때문에 시작 직후 한 번 닫히는 상태일 수 있습니다.",
            "- OOTP를 모드 없이 한 번 정상 실행하고 종료한 뒤 다시 런처로 실행하세요.",
        };

        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static string BuildPrivacyNotice()
    {
        var lines = new List<string>
        {
            "Ultimate KBO diagnostics privacy note",
            "",
            "This ZIP intentionally does not include the save folder itself.",
            "It may include:",
            "- Launcher and KBOFix logs from %LOCALAPPDATA%\\OOTP-KBO",
            "- Mod config files such as kbo_flags.json and kbo_settings.json",
            "- Environment/path/process reports",
            "- Current save path text files, which can reveal local Windows folder names",
            "",
            "It does not intentionally include:",
            "- .lg save directory contents",
            "- OOTP account credentials",
            "- Browser cookies",
            "",
            "If you do not want to share local folder names, remove or redact paths before sending.",
        };

        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static string WorstStatus(IEnumerable<CheckLine> checks)
    {
        if (checks.Any(c => c.Status.Equals("FAIL", StringComparison.OrdinalIgnoreCase)))
        {
            return "FAIL";
        }

        if (checks.Any(c => c.Status.Equals("WARN", StringComparison.OrdinalIgnoreCase)))
        {
            return "WARN";
        }

        return "PASS";
    }

    private static string FormatChecks(IEnumerable<CheckLine> checks)
    {
        return string.Join(Environment.NewLine, checks.Select(c => $"{c.Status,-4} {c.Name}: {c.Detail}")) + Environment.NewLine;
    }

    private static string BuildPathCandidatesReport(string? explicitPath)
    {
        var lines = new List<string>
        {
            $"generated_at={DateTimeOffset.Now:O}",
            $"explicit_path={explicitPath ?? ""}",
            "",
        };

        foreach (var candidate in LauncherPaths.GetOotpPathCandidates(explicitPath).Distinct(StringComparer.OrdinalIgnoreCase))
        {
            string fullPath;
            try
            {
                fullPath = Path.GetFullPath(candidate);
            }
            catch
            {
                fullPath = candidate;
            }

            lines.Add($"exists={(File.Exists(candidate) ? "1" : "0")} path=\"{fullPath}\"");
        }

        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static string BuildProcessReport(string? resolvedOotpPath)
    {
        var lines = new List<string>
        {
            $"generated_at={DateTimeOffset.Now:O}",
            $"resolved_ootp={resolvedOotpPath ?? ""}",
            "",
        };

        foreach (var process in Process.GetProcesses().Select(ProcessDiscovery.TryDescribe).Where(p => p is not null).Cast<ProcessInfo>())
        {
            if (!process.Name.Contains("ootp", StringComparison.OrdinalIgnoreCase)
                    && !process.Path.Contains("ootp", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            lines.Add($"pid={process.Id} name=\"{process.Name}\" path=\"{process.Path}\"");
        }

        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static string BuildCurrentSavePathReport(string localDataDirectory)
    {
        var lines = new List<string> { $"generated_at={DateTimeOffset.Now:O}", "" };
        if (!Directory.Exists(localDataDirectory))
        {
            lines.Add($"local data directory missing: {localDataDirectory}");
            return string.Join(Environment.NewLine, lines) + Environment.NewLine;
        }

        foreach (var pathFile in Directory.EnumerateFiles(localDataDirectory, "current_save_path_*.txt").OrderBy(Path.GetFileName))
        {
            var savePath = SafeReadAllText(pathFile).Trim();
            lines.Add($"cache={Path.GetFileName(pathFile)} value=\"{savePath}\" exists={(Directory.Exists(savePath) ? "1" : "0")}");
            if (Directory.Exists(savePath))
            {
                var descriptionPath = Path.Combine(savePath, OotpProduct.DescriptionFileName);
                lines.Add($"  description_exists={(File.Exists(descriptionPath) ? "1" : "0")} description=\"{descriptionPath}\"");
                var statePath = Path.Combine(savePath, OotpProduct.SaveStateSqliteFileName);
                lines.Add($"  state_db_exists={(File.Exists(statePath) ? "1" : "0")} state_db=\"{statePath}\"");
            }
        }

        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static string BuildRecentProblemReport(string localDataDirectory)
    {
        var lines = new List<string>
        {
            $"generated_at={DateTimeOffset.Now:O}",
            "This file extracts recent lines that contain known failure/block/crash keywords.",
            "",
        };

        if (!Directory.Exists(localDataDirectory))
        {
            lines.Add($"local data directory missing: {localDataDirectory}");
            return string.Join(Environment.NewLine, lines) + Environment.NewLine;
        }

        var count = 0;
        foreach (var file in EnumerateRecentLocalFiles(localDataDirectory).OrderByDescending(file => file.LastWriteTimeUtc).Take(40))
        {
            foreach (var line in ReadRecentLines(file.FullName, 1600))
            {
                if (!ContainsAny(line, ProblemLineNeedles))
                {
                    continue;
                }

                lines.Add($"{RelativeTo(localDataDirectory, file.FullName)}: {TrimForReport(line, 1600)}");
                count++;
                if (count >= MaxRecentProblemLines)
                {
                    lines.Add($"[truncated after {MaxRecentProblemLines} matched lines]");
                    return string.Join(Environment.NewLine, lines) + Environment.NewLine;
                }
            }
        }

        if (count == 0)
        {
            lines.Add("No known problem lines matched in recent local diagnostics.");
        }

        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static string BuildLocalDataInventory(string localDataDirectory)
    {
        var lines = new List<string>
        {
            $"generated_at={DateTimeOffset.Now:O}",
            $"local_data_dir={localDataDirectory}",
            "",
        };

        if (!Directory.Exists(localDataDirectory))
        {
            lines.Add("local data directory missing");
            return string.Join(Environment.NewLine, lines) + Environment.NewLine;
        }

        foreach (var file in EnumerateRecentLocalFiles(localDataDirectory).OrderBy(file => RelativeTo(localDataDirectory, file.FullName)))
        {
            lines.Add($"{RelativeTo(localDataDirectory, file.FullName)}\tsize={file.Length}\tmodified_utc={file.LastWriteTimeUtc:O}");
        }

        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static string BuildOotpUserDataReport()
    {
        var lines = new List<string>
        {
            $"generated_at={DateTimeOffset.Now:O}",
            "This report only lists likely OOTP user-data paths and crash-flag candidates. It does not copy save folders.",
            "",
            "[candidate roots]",
        };

        foreach (var root in EnumerateOotpUserDataRoots().Distinct(StringComparer.OrdinalIgnoreCase))
        {
            lines.Add($"exists={(Directory.Exists(root) ? "1" : "0")} path=\"{root}\"");
        }

        lines.Add("");
        lines.Add("[crashed_on_startup candidates]");
        var crashCandidates = EnumerateOotpCrashFlagCandidates().Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        if (crashCandidates.Length == 0)
        {
            lines.Add("No candidate roots were available.");
        }
        else
        {
            foreach (var candidate in crashCandidates)
            {
                lines.Add($"exists={(File.Exists(candidate) ? "1" : "0")} path=\"{candidate}\"");
            }
        }

        lines.Add("");
        lines.Add("[recent crash/log-like files]");
        var diagnosticFiles = EnumerateOotpUserDiagnosticFiles()
            .OrderByDescending(file => file.LastWriteTimeUtc)
            .Take(40)
            .ToArray();
        if (diagnosticFiles.Length == 0)
        {
            lines.Add("No recent OOTP crash/log-like files found in known user-data folders.");
        }
        else
        {
            foreach (var file in diagnosticFiles)
            {
                lines.Add($"{file.FullName}\tsize={file.Length}\tmodified_utc={file.LastWriteTimeUtc:O}");
            }
        }

        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    private static void AddKnownLocalFiles(ZipArchive archive, HashSet<string> addedEntryNames, string localDataDirectory)
    {
        if (!Directory.Exists(localDataDirectory))
        {
            return;
        }

        AddFilesByPattern(archive, addedEntryNames, localDataDirectory, "logs", "launcher.log", 1);
        AddFilesByPattern(archive, addedEntryNames, localDataDirectory, "logs", "runtime.ndjson*", 6);
        AddFilesByPattern(archive, addedEntryNames, localDataDirectory, "logs", "rule_audit.ndjson*", 6);
        AddFilesByPattern(archive, addedEntryNames, localDataDirectory, "logs", "foreign_*.ndjson*", 6);
        AddFilesByPattern(archive, addedEntryNames, localDataDirectory, "config", "kbo_flags.json", 1);
        AddFilesByPattern(archive, addedEntryNames, localDataDirectory, "config", "kbo_settings.json", 1);
        AddFilesByPattern(archive, addedEntryNames, localDataDirectory, "status", "launcher_*_status.txt", 8);
        AddFilesByPattern(archive, addedEntryNames, localDataDirectory, "status", "current_save_path_*.txt", 16);
        AddFilesByPattern(archive, addedEntryNames, Path.Combine(localDataDirectory, "run_dlls"), "run_dlls", "WebView2Loader.dll", 1);
    }

    private static void AddRecentPerfFiles(ZipArchive archive, HashSet<string> addedEntryNames, string localDataDirectory)
    {
        var perfDir = Path.Combine(localDataDirectory, "perf");
        if (!Directory.Exists(perfDir))
        {
            return;
        }

        AddFilesByPattern(archive, addedEntryNames, perfDir, "perf", "*.csv", 4);
    }

    private static void AddFilesByPattern(
        ZipArchive archive,
        HashSet<string> addedEntryNames,
        string directory,
        string entryDirectory,
        string pattern,
        int maxFiles)
    {
        if (!Directory.Exists(directory))
        {
            return;
        }

        foreach (var file in Directory.EnumerateFiles(directory, pattern)
                     .Select(path => new FileInfo(path))
                     .Where(file => file.Exists)
                     .OrderByDescending(file => file.LastWriteTimeUtc)
                     .Take(maxFiles))
        {
            AddFile(archive, addedEntryNames, file.FullName, $"{entryDirectory}/{file.Name}");
        }
    }

    private static void AddText(ZipArchive archive, HashSet<string> addedEntryNames, string entryName, string content)
    {
        var entry = archive.CreateEntry(UniqueEntryName(addedEntryNames, entryName), CompressionLevel.Optimal);
        using var writer = new StreamWriter(entry.Open());
        writer.Write(content);
    }

    private static void AddFile(ZipArchive archive, HashSet<string> addedEntryNames, string filePath, string entryName)
    {
        try
        {
            var info = new FileInfo(filePath);
            if (!info.Exists)
            {
                return;
            }

            if (info.Length > MaxIncludedFileBytes)
            {
                AddLargeFileTail(archive, addedEntryNames, filePath, entryName, info.Length);
                return;
            }

            var entry = archive.CreateEntry(UniqueEntryName(addedEntryNames, entryName), CompressionLevel.Optimal);
            using var input = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            using var output = entry.Open();
            input.CopyTo(output);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            AddText(
                archive,
                addedEntryNames,
                $"{Path.GetDirectoryName(entryName)?.Replace('\\', '/')}/{Path.GetFileName(entryName)}.copy_failed.txt".TrimStart('/'),
                $"{filePath}{Environment.NewLine}{ex.GetType().Name}: {ex.Message}{Environment.NewLine}");
        }
    }

    private static void AddLargeFileTail(
        ZipArchive archive,
        HashSet<string> addedEntryNames,
        string filePath,
        string entryName,
        long originalBytes)
    {
        var tailBytes = (int)MaxIncludedFileBytes;
        byte[] buffer;
        using (var input = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
        {
            input.Seek(-tailBytes, SeekOrigin.End);
            buffer = new byte[tailBytes];
            var read = input.Read(buffer, 0, tailBytes);
            if (read < tailBytes)
            {
                Array.Resize(ref buffer, read);
            }
        }

        var tailName = $"{entryName}.tail.txt";
        var entry = archive.CreateEntry(UniqueEntryName(addedEntryNames, tailName), CompressionLevel.Optimal);
        using var output = entry.Open();
        var header = System.Text.Encoding.UTF8.GetBytes($"[truncated original_bytes={originalBytes} tail_bytes={buffer.Length} source={filePath}]{Environment.NewLine}");
        output.Write(header, 0, header.Length);
        output.Write(buffer, 0, buffer.Length);
    }

    private static string UniqueEntryName(HashSet<string> addedEntryNames, string entryName)
    {
        entryName = entryName.Replace('\\', '/').TrimStart('/');
        if (addedEntryNames.Add(entryName))
        {
            return entryName;
        }

        var directory = Path.GetDirectoryName(entryName)?.Replace('\\', '/');
        var name = Path.GetFileNameWithoutExtension(entryName);
        var extension = Path.GetExtension(entryName);
        for (var i = 2; ; i++)
        {
            var candidate = string.IsNullOrWhiteSpace(directory)
                ? $"{name}_{i}{extension}"
                : $"{directory}/{name}_{i}{extension}";
            if (addedEntryNames.Add(candidate))
            {
                return candidate;
            }
        }
    }

    private static string ReadRecentRuntimeText(string localDataDirectory, int maxLines)
    {
        if (!Directory.Exists(localDataDirectory))
        {
            return "";
        }

        var files = Directory.EnumerateFiles(localDataDirectory, "runtime.ndjson*")
            .Select(path => new FileInfo(path))
            .Where(file => file.Exists)
            .OrderByDescending(file => file.LastWriteTimeUtc)
            .Take(3);
        return string.Join(Environment.NewLine, files.Select(file => ReadRecentText(file.FullName, maxLines)));
    }

    private static string ReadRecentText(string path, int maxLines)
    {
        try
        {
            if (!File.Exists(path))
            {
                return "";
            }

            return string.Join(Environment.NewLine, File.ReadLines(path).TakeLast(maxLines));
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return $"{ex.GetType().Name}: {ex.Message}";
        }
    }

    private static IEnumerable<string> ReadRecentLines(string path, int maxLines)
    {
        try
        {
            if (!File.Exists(path))
            {
                return [];
            }

            return File.ReadLines(path).TakeLast(maxLines).ToArray();
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return [$"{ex.GetType().Name}: {ex.Message}"];
        }
    }

    private static IEnumerable<FileInfo> EnumerateRecentLocalFiles(string localDataDirectory)
    {
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var file in EnumerateFiles(localDataDirectory, LocalDiagnosticFilePatterns)
                     .Concat(EnumerateFiles(Path.Combine(localDataDirectory, "perf"), ["*.csv"]))
                     .Concat(EnumerateFiles(Path.Combine(localDataDirectory, "run_dlls"), ["WebView2Loader.dll", "KBOFix-*.dll"])))
        {
            if (seen.Add(file.FullName))
            {
                yield return file;
            }
        }
    }

    private static IEnumerable<string> EnumerateOotpCrashFlagCandidates()
    {
        var fileNames = new[]
        {
            "crashed_on_startup",
            "crashed_on_startup.txt",
            "crashed_on_startup.flag",
            "crashed_on_startup.dat",
        };

        foreach (var root in EnumerateOotpUserDataRoots())
        {
            foreach (var directory in EnumerateOotpUserDiagnosticDirectories(root))
            {
                foreach (var fileName in fileNames)
                {
                    yield return Path.Combine(directory, fileName);
                }

                if (!Directory.Exists(directory))
                {
                    continue;
                }

                foreach (var file in EnumerateFiles(directory, ["*crashed_on_startup*"]))
                {
                    yield return file.FullName;
                }
            }
        }
    }

    private static IEnumerable<FileInfo> EnumerateOotpUserDiagnosticFiles()
    {
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var root in EnumerateOotpUserDataRoots())
        {
            foreach (var directory in EnumerateOotpUserDiagnosticDirectories(root))
            {
                foreach (var file in EnumerateFiles(directory, ["*crash*", "*.log", "debug*.txt", "trace*.txt", "ootp*.txt"]))
                {
                    if (seen.Add(file.FullName))
                    {
                        yield return file;
                    }
                }
            }
        }
    }

    private static IEnumerable<string> EnumerateOotpUserDiagnosticDirectories(string root)
    {
        yield return root;
        yield return Path.Combine(root, "config");
        yield return Path.Combine(root, "debug");
        yield return Path.Combine(root, "logs");
        yield return Path.Combine(root, "settings");
    }

    private static IEnumerable<string> EnumerateOotpUserDataRoots()
    {
        foreach (var baseRoot in new[]
        {
            Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        })
        {
            if (string.IsNullOrWhiteSpace(baseRoot))
            {
                continue;
            }

            foreach (var productFolder in OotpProduct.ProductFolderNames.Concat([OotpProduct.ProductShortFolderName]).Distinct(StringComparer.OrdinalIgnoreCase))
            {
                yield return Path.Combine(baseRoot, OotpProduct.VendorFolderName, productFolder);
            }
        }
    }

    private static IEnumerable<FileInfo> EnumerateFiles(string directory, IEnumerable<string> patterns)
    {
        if (!Directory.Exists(directory))
        {
            yield break;
        }

        foreach (var pattern in patterns)
        {
            IEnumerable<string> paths;
            try
            {
                paths = Directory.EnumerateFiles(directory, pattern, SearchOption.TopDirectoryOnly).ToArray();
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
            {
                continue;
            }

            foreach (var path in paths)
            {
                FileInfo info;
                try
                {
                    info = new FileInfo(path);
                }
                catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
                {
                    continue;
                }

                if (info.Exists)
                {
                    yield return info;
                }
            }
        }
    }

    private static bool ContainsAny(string text, IEnumerable<string> needles)
    {
        return needles.Any(needle => text.Contains(needle, StringComparison.OrdinalIgnoreCase));
    }

    private static string RelativeTo(string root, string path)
    {
        try
        {
            return Path.GetRelativePath(root, path);
        }
        catch (Exception ex) when (ex is ArgumentException or InvalidOperationException)
        {
            return path;
        }
    }

    private static string TrimForReport(string text, int maxChars)
    {
        if (text.Length <= maxChars)
        {
            return text;
        }

        return text[..maxChars] + "...";
    }

    private static string SafeReadAllText(string path)
    {
        try
        {
            return File.Exists(path) ? File.ReadAllText(path) : "";
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return $"{ex.GetType().Name}: {ex.Message}";
        }
    }
}
