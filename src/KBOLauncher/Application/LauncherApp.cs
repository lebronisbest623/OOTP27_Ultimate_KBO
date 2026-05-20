using System.Diagnostics;
using static InjectedModuleDetector;
using static InjectionRosterMarkerWaiter;
using static InjectionTargetResolver;
using static KboFlags;
using static KboSeedFiles;
using static LauncherGuardStatus;
using static LauncherLog;
using static OotpKoreanFontFallbackInstaller;
using static LauncherPaths;
using static ProcessDiscovery;

internal static partial class LauncherApp
{
    private static readonly TimeSpan MarkedSaveWaitTimeout = TimeSpan.FromMinutes(15);
    private static readonly TimeSpan MarkedSavePollInterval = TimeSpan.FromSeconds(2);

    public static int Run(string[] args)
    {
        LauncherOptions options;
        try
        {
            options = LauncherOptions.Parse(args);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            LauncherOptions.PrintHelp();
            return 1;
        }

        if (options.ShowHelp)
        {
            LauncherOptions.PrintHelp();
            return 0;
        }

        return RunParsed(options, isDefaultRun: args.Length == 0);
    }

    private static int RunParsed(LauncherOptions options, bool isDefaultRun)
    {
        var exePath = ResolveOotpPath(options.OotpPath);
        if (exePath is null)
        {
            WriteOotpPathDiscoveryStatus(options.OotpPath);
            Console.Error.WriteLine($"Could not find {OotpProduct.ExecutableFileName}. Pass --ootp \"C:\\path\\to\\{OotpProduct.ExecutableFileName}\".");
            Console.Error.WriteLine($"Path discovery diagnostics written to: {GetKboLocalDataPath(OotpProduct.PathDiscoveryStatusFileName)}");
            Console.Error.WriteLine($"Official-site installs can also set {OotpProduct.OotpEnvironmentVariables[1]} to the folder containing {OotpProduct.ExecutableFileName}.");
            return 2;
        }

        EnsureLauncherRuntimeData(exePath);
        var existing = FindExistingOotpProcesses(exePath);
        var defaultOptions = ApplyDefaultRunOptions(options, isDefaultRun, existing.Count);
        if (defaultOptions.ExitCode is not null)
        {
            return defaultOptions.ExitCode.Value;
        }
        options = defaultOptions.Options;

        var logPath = InitializeLauncherLog(exePath, isDefaultRun, options);
        var buildGate = EvaluateOotpBuildGate(exePath, logPath, options, isDefaultRun, existing.Count);
        if (buildGate.ExitCode is not null)
        {
            return buildGate.ExitCode.Value;
        }
        options = buildGate.Options;

        LogExistingProcesses(existing, logPath);
        ApplyRuntimeFlagOptions(options);

        var launchPlan = BuildLaunchPlan(options, isDefaultRun, buildGate.SupportedBuild);
        Log(logPath, $"injection_policy mode={launchPlan.InjectionDecision.Mode} reason={launchPlan.InjectionDecision.Reason}");
        EnsureOotpKoreanFontFallback(exePath, logPath, options.DryRun);

        if (options.AttachPid is not null || options.AttachExisting)
        {
            return RunAttachFlow(options, isDefaultRun, existing, logPath);
        }

        return RunLaunchFlow(options, exePath, existing, logPath, launchPlan);
    }

    private static List<ProcessInfo> FindExistingOotpProcesses(string exePath)
    {
        return Process.GetProcesses()
            .Select(TryDescribe)
            .Where(p => p is not null)
            .Cast<ProcessInfo>()
            .Where(p => PathEquals(p.Path, exePath))
            .ToList();
    }

    private static void LogLauncherBuild(string logPath)
    {
        var exePath = Environment.ProcessPath ?? "";
        var assemblyWriteTime = File.Exists(exePath)
            ? File.GetLastWriteTime(exePath).ToString("O")
            : "";

        Log(
            logPath,
            $"launcher_build exe=\"{exePath}\" base_dir=\"{AppContext.BaseDirectory}\" write_time=\"{assemblyWriteTime}\"");
    }
}
