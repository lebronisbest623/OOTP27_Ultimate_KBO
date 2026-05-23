using Spectre.Console;
using static LauncherPaths;
using static LauncherLog;

internal static class LauncherGuardStatus
{
    public static void WriteLauncherBuildGuardStatus(OotpBuildInfo info, OotpSupportedBuild? supportedBuild, bool injectionRequested)
    {
        var path = GetKboLocalDataPath("launcher_build_guard_status.txt");
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
    
        var lines = new List<string>
        {
            $"checked_at={DateTimeOffset.Now:O}",
            $"injection_requested={(injectionRequested ? "1" : "0")}",
            $"build_read={(info.Ok ? "1" : "0")}",
        };
    
        if (info.Ok)
        {
            lines.Add($"timestamp=0x{info.Timestamp:X8}");
            lines.Add($"size_of_image=0x{info.SizeOfImage:X8}");
        }
        else
        {
            lines.Add($"error={info.Error}");
        }
    
        if (supportedBuild is null)
        {
            lines.Add("status=unsupported");
            lines.Add("kbo_injection=disabled");
        }
        else
        {
            lines.Add($"status={supportedBuild.SupportStatus}");
            lines.Add($"label={supportedBuild.Label}");
            if (supportedBuild.ExperimentalSignature)
            {
                lines.Add("support=experimental_signature");
            }
            if (!supportedBuild.NativePatchesSupported)
            {
                lines.Add("support=metadata_only");
                lines.Add("kbo_injection=disabled");
                lines.Add("disabled_reason=incomplete_native_rva_table");
            }
            else
            {
                lines.Add(injectionRequested ? "kbo_injection=allowed" : "kbo_injection=not_requested");
            }
        }
    
        lines.Add("supported_builds=" + string.Join(", ", OotpBuildGuard.SupportedBuildDescriptions()));
        File.WriteAllLines(path, lines);
    }
    
    public static void PrintUnsupportedBuildInjectionWarning(OotpBuildInfo info)
    {
        Console.Error.WriteLine("KBOFix injection disabled: this OOTP build is not verified.");
        Console.Error.WriteLine("This prevents silent memory-offset mismatches after an OOTP patch.");
        if (info.Ok)
        {
            Console.Error.WriteLine($"Detected build: timestamp=0x{info.Timestamp:X8}, size_of_image=0x{info.SizeOfImage:X8}");
            Console.Error.WriteLine("Send these two values with your install source, for example Steam or official website.");
        }
        else
        {
            Console.Error.WriteLine($"Could not read OOTP build info: {info.Error}");
        }
        Console.Error.WriteLine("Update the launcher/native supported-build list after verifying this OOTP version.");
    }
    
    public static RosterMarkerInfo CheckRosterMarkerForInjectionTarget(int pid, string logPath)
    {
        var info = ProbeRosterMarkerForInjectionTarget(pid, logPath, null);
        AnsiConsole.WriteLine(KboRosterMarkerGuard.FormatConsoleStatus(info));
        return info;
    }

    public static RosterMarkerInfo ProbeRosterMarkerForInjectionTarget(
        int pid,
        string logPath,
        DateTimeOffset? minSaveCompletedAt = null,
        DateTimeOffset? fallbackMinSaveCompletedAt = null,
        bool allowIncompleteMarkedSave = false)
    {
        RosterMarkerInfo info;
        try
        {
            info = KboRosterMarkerGuard.CheckCurrentSave(
                pid,
                minSaveCompletedAt,
                allowIncompleteMarkedSave,
                message => Log(logPath, message));
        }
        catch (Exception ex) when (ex is ArgumentException or InvalidOperationException)
        {
            info = RosterMarkerInfo.Fail("process_unavailable", null, null, $"{ex.GetType().Name}: {ex.Message}");
        }

        WriteCurrentSavePathCache(pid, SelectCurrentSavePathCacheValue(info), logPath);
        Log(logPath, KboRosterMarkerGuard.FormatLogStatus(info));
        WriteRosterMarkerGuardStatus(info);
        return info;
    }

    internal static string? SelectCurrentSavePathCacheValue(RosterMarkerInfo? info)
    {
        if (info is null || !info.Ok)
        {
            return null;
        }

        return info.SavePath;
    }
    
    public static void WriteCurrentSavePathCache(int pid, string? savePath, string logPath)
    {
        var path = GetKboLocalDataPath(OotpProduct.CurrentSavePathFileName(pid));
        try
        {
            if (string.IsNullOrWhiteSpace(savePath))
            {
                if (File.Exists(path))
                {
                    File.Delete(path);
                }
                return;
            }
    
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.WriteAllText(path, Path.GetFullPath(savePath));
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
        {
            Log(logPath, $"current_save_cache_write_failed path=\"{path}\" error=\"{ex.Message}\"");
        }
    }
    
    public static void WriteRosterMarkerGuardStatus(RosterMarkerInfo info)
    {
        var path = GetKboLocalDataPath(OotpProduct.RosterMarkerGuardStatusFileName);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
    
        var lines = new List<string>
        {
            $"checked_at={DateTimeOffset.Now:O}",
            $"status={info.Status}",
            $"kbo_injection={(info.Ok ? "allowed" : "disabled")}",
            $"required_marker={KboRosterMarkerGuard.RequiredMarkerUrl}",
        };
    
        if (!string.IsNullOrWhiteSpace(info.SavePath))
        {
            lines.Add($"save_path={info.SavePath}");
        }
        if (!string.IsNullOrWhiteSpace(info.DescriptionPath))
        {
            lines.Add($"description_path={info.DescriptionPath}");
        }
        if (!string.IsNullOrWhiteSpace(info.SaveCompletedPath))
        {
            lines.Add($"save_completed_path={info.SaveCompletedPath}");
        }
        if (info.SaveCompletedAt is not null)
        {
            lines.Add($"save_completed_at={info.SaveCompletedAt.Value:O}");
        }
        if (!string.IsNullOrWhiteSpace(info.Error))
        {
            lines.Add($"error={info.Error}");
        }
    
        File.WriteAllLines(path, lines);
    }
    
    public static void PrintMissingRosterMarkerWarning(RosterMarkerInfo info)
    {
        Console.Error.WriteLine("KBOFix injection disabled: the currently opened OOTP save is not marked for this KBO roster or is not fully saved yet.");
        Console.Error.WriteLine($"Required marker in {OotpProduct.DescriptionFileName}: {KboRosterMarkerGuard.RequiredMarkerUrl}");
        if (!string.IsNullOrWhiteSpace(info.SavePath))
        {
            Console.Error.WriteLine($"Current save checked: {info.SavePath}");
        }
        if (!string.IsNullOrWhiteSpace(info.DescriptionPath))
        {
            Console.Error.WriteLine($"Description checked: {info.DescriptionPath}");
        }
        if (!string.IsNullOrWhiteSpace(info.SaveCompletedPath))
        {
            Console.Error.WriteLine($"Save completion checked: {info.SaveCompletedPath}");
        }
        if (!string.IsNullOrWhiteSpace(info.Error))
        {
            Console.Error.WriteLine($"Reason: {info.Error}");
        }
    }
}
