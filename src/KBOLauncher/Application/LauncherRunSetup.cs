using Spectre.Console;
using static KboFlags;
using static KboSeedFiles;
using static LauncherGuardStatus;
using static LauncherLog;
using static LauncherPaths;

internal static partial class LauncherApp
{
    private sealed record DefaultRunOptions(LauncherOptions Options, int? ExitCode);

    private sealed record BuildGateResult(LauncherOptions Options, OotpSupportedBuild? SupportedBuild, int? ExitCode);

    private sealed record LaunchPlan(bool AllstarBootstrapRequested, LauncherInjectionDecision InjectionDecision);

    private static void EnsureLauncherRuntimeData(string exePath)
    {
        EnsureKboLeagueIdConfig();
        EnsureBundledKboDataManifest();
        EnsureKboScheduleAllstarGameLines(exePath);
        EnsureDefaultKboRuntimeFlags();
        MigrateLegacyKboSettingsFromFlags();
    }

    private static DefaultRunOptions ApplyDefaultRunOptions(
        LauncherOptions options,
        bool isDefaultRun,
        int existingProcessCount)
    {
        if (!isDefaultRun)
        {
            return new(options, null);
        }

        var enableDefaultInjection = ReadKboFlagDefaultEnabled("enable_launcher_injection.txt");
        string? defaultDll = null;
        if (enableDefaultInjection)
        {
            defaultDll = ResolveDefaultKboFixDllPath();
            if (defaultDll is null)
            {
                Console.Error.WriteLine("Could not find KBOFix.dll next to the launcher.");
                return new(options, 7);
            }
        }

        return new(
            options with
            {
                DllPath = defaultDll,
                EnableSingleDivisionAllstarEvents = true,
                AttachExisting = existingProcessCount > 0,
            },
            null);
    }

    private static string InitializeLauncherLog(string exePath, bool isDefaultRun, LauncherOptions options)
    {
        var logPath = GetLogPath();
        Directory.CreateDirectory(Path.GetDirectoryName(logPath)!);

        Log(logPath, $"resolved_ootp={exePath}");
        LogLauncherBuild(logPath);
        if (isDefaultRun && options.DllPath is null)
        {
            Log(logPath, "default_injection=disabled reason=explicit_flag");
        }

        AnsiConsole.MarkupLineInterpolated($"OOTP: {exePath}");
        AnsiConsole.MarkupLineInterpolated($"WorkDir: {Path.GetDirectoryName(exePath)}");
        AnsiConsole.MarkupLineInterpolated($"Log: {logPath}");
        if (isDefaultRun && options.DllPath is null)
        {
            AnsiConsole.MarkupLine("[yellow]KBOFix injection is disabled by enable_launcher_injection=false.[/]");
            AnsiConsole.MarkupLineInterpolated($"To re-enable launcher injection, set enable_launcher_injection=true in: {GetKboFlagConfigPath()}");
        }

        return logPath;
    }

    private static BuildGateResult EvaluateOotpBuildGate(
        string exePath,
        string logPath,
        LauncherOptions options,
        bool isDefaultRun,
        int existingProcessCount)
    {
        var ootpBuildInfo = OotpBuildGuard.Read(exePath);
        var supportedOotpBuild = OotpBuildGuard.FindSupportedBuild(ootpBuildInfo);
        var knownOotpBuild = supportedOotpBuild ?? OotpBuildGuard.FindKnownBuild(ootpBuildInfo);
        AnsiConsole.WriteLine(OotpBuildGuard.FormatConsoleStatus(ootpBuildInfo, knownOotpBuild));
        Log(logPath, OotpBuildGuard.FormatLogStatus(ootpBuildInfo, knownOotpBuild));
        WriteLauncherBuildGuardStatus(ootpBuildInfo, knownOotpBuild, options.DllPath is not null);

        if (options.DllPath is null || supportedOotpBuild is not null)
        {
            return new(options, supportedOotpBuild, null);
        }

        PrintUnsupportedBuildInjectionWarning(ootpBuildInfo);
        var blockReason = knownOotpBuild is not null && !knownOotpBuild.NativePatchesSupported
            ? "metadata_only_ootp_build"
            : "unsupported_ootp_build";
        Log(logPath, $"inject_blocked reason={blockReason} {OotpBuildGuard.FormatLogStatus(ootpBuildInfo, knownOotpBuild)}");

        if (options.DryRun)
        {
            Console.WriteLine("Dry-run: KBOFix injection would be blocked for this OOTP build.");
            return new(options, supportedOotpBuild, null);
        }

        if (!isDefaultRun)
        {
            return new(options, supportedOotpBuild, 8);
        }

        var safeOptions = options with { DllPath = null, AttachExisting = false };
        if (existingProcessCount > 0)
        {
            AnsiConsole.MarkupLine("[yellow]Existing OOTP process left unmodified because this build is not supported.[/]");
            return new(safeOptions, supportedOotpBuild, 0);
        }

        return new(safeOptions, supportedOotpBuild, null);
    }

    private static void LogExistingProcesses(IReadOnlyList<ProcessInfo> existing, string logPath)
    {
        if (existing.Count == 0)
        {
            return;
        }

        AnsiConsole.MarkupLine("[yellow]Existing OOTP process:[/]");
        foreach (var process in existing)
        {
            AnsiConsole.MarkupLineInterpolated($"  pid={process.Id} name={process.Name} path={process.Path}");
            Log(logPath, $"existing pid={process.Id} name={process.Name} path={process.Path}");
        }
    }

    private static void ApplyRuntimeFlagOptions(LauncherOptions options)
    {
        if (options.EnableForeignWaiverAi is not null)
        {
            WriteKboForeignWaiverAiFlag(options.EnableForeignWaiverAi.Value);
        }
        if (options.EnableSingleDivisionAllstarEvents is not null)
        {
            WriteKboSingleDivisionAllstarEventsFlag(options.EnableSingleDivisionAllstarEvents.Value);
        }
    }

    private static LaunchPlan BuildLaunchPlan(
        LauncherOptions options,
        bool isDefaultRun,
        OotpSupportedBuild? supportedOotpBuild)
    {
        var allstarBootstrapRequested = options.DllPath is not null
            && supportedOotpBuild is not null
            && ReadKboFlag("enable_single_division_allstar_runtime_patches.txt")
            && ReadKboFlag("enable_single_division_allstar_events.txt");
        var injectionDecision = LauncherInjectionPolicy.Decide(
            isDefaultRun,
            options.DllPath,
            supportedOotpBuild is not null,
            options.AttachPid is not null || options.AttachExisting,
            allstarBootstrapRequested);

        return new(allstarBootstrapRequested, injectionDecision);
    }

}
