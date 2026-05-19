
internal static class LauncherPaths
{
    public static string? ResolveOotpPath(string? explicitPath)
    {
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            if (IsOotpExecutablePath(explicitPath) && File.Exists(explicitPath))
            {
                return explicitPath;
            }

            if (File.Exists(explicitPath) || Directory.Exists(explicitPath))
            {
                return null;
            }
        }

        return ResolveExistingNewestPath(GetOotpPathCandidates(null));
    }

    private static bool IsOotpExecutablePath(string path)
    {
        return Path.GetFileName(path).Equals("ootp27.exe", StringComparison.OrdinalIgnoreCase);
    }

    internal static IEnumerable<string> GetOotpPathCandidates(string? explicitPath)
    {
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            yield return explicitPath;
        }

        foreach (var envVar in new[] { "OOTP27_EXE", "OOTP27_DIR", "OOTP_DIR" })
        {
            var value = Environment.GetEnvironmentVariable(envVar);
            if (string.IsNullOrWhiteSpace(value))
            {
                continue;
            }

            yield return value.EndsWith(".exe", StringComparison.OrdinalIgnoreCase)
                ? value
                : Path.Combine(value, "ootp27.exe");
        }

        foreach (var programFilesRoot in ResolveProgramFilesRoots())
        {
            yield return Path.Combine(programFilesRoot, "Out of the Park Developments", "OOTP Baseball 27", "ootp27.exe");
            yield return Path.Combine(programFilesRoot, "Out of the Park Developments", "Out of the Park Baseball 27", "ootp27.exe");
            yield return Path.Combine(programFilesRoot, "OOTP Baseball 27", "ootp27.exe");
            yield return Path.Combine(programFilesRoot, "Out of the Park Baseball 27", "ootp27.exe");
        }

        foreach (var drive in DriveInfo.GetDrives())
        {
            if (drive.DriveType != DriveType.Fixed && drive.DriveType != DriveType.Removable)
            {
                continue;
            }

            yield return Path.Combine(drive.RootDirectory.FullName, "OOTP 27", "ootp27.exe");
            yield return Path.Combine(drive.RootDirectory.FullName, "OOTP Baseball 27", "ootp27.exe");
            yield return Path.Combine(drive.RootDirectory.FullName, "Out of the Park Baseball 27", "ootp27.exe");
        }

        yield return @"C:\Program Files (x86)\Steam\steamapps\common\Out of the Park Baseball 27\ootp27.exe";
        yield return @"C:\Program Files\Steam\steamapps\common\Out of the Park Baseball 27\ootp27.exe";
        foreach (var candidate in ResolveSteamLibraryOotpCandidates())
        {
            yield return candidate;
        }
    }

    public static void WriteOotpPathDiscoveryStatus(string? explicitPath)
    {
        var path = GetKboLocalDataPath("launcher_path_discovery_status.txt");
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);

        var lines = new List<string>
        {
            $"checked_at={DateTimeOffset.Now:O}",
            $"explicit_path={explicitPath ?? ""}",
            "status=not_found",
        };

        foreach (var candidate in GetOotpPathCandidates(explicitPath).Distinct(StringComparer.OrdinalIgnoreCase))
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

            lines.Add($"candidate exists={(File.Exists(candidate) ? "1" : "0")} path=\"{fullPath}\"");
        }

        File.WriteAllLines(path, lines);
    }

    internal static string? ResolveExistingNewestPath(IEnumerable<string> candidates)
    {
        return candidates
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .Where(File.Exists)
            .OrderByDescending(File.GetLastWriteTimeUtc)
            .FirstOrDefault();
    }

    private static IEnumerable<string> ResolveSteamLibraryOotpCandidates()
    {
        foreach (var steamRoot in ResolveSteamRoots())
        {
            yield return Path.Combine(steamRoot, "steamapps", "common", "Out of the Park Baseball 27", "ootp27.exe");

            var libraryFolders = Path.Combine(steamRoot, "steamapps", "libraryfolders.vdf");
            foreach (var libraryRoot in ReadSteamLibraryFolders(libraryFolders))
            {
                yield return Path.Combine(libraryRoot, "steamapps", "common", "Out of the Park Baseball 27", "ootp27.exe");
            }
        }
    }

    private static IEnumerable<string> ResolveSteamRoots()
    {
        var candidates = new List<string?>
        {
            Environment.GetEnvironmentVariable("STEAM_DIR"),
            @"C:\Program Files (x86)\Steam",
            @"C:\Program Files\Steam",
        };

        foreach (var drive in DriveInfo.GetDrives())
        {
            if (drive.DriveType != DriveType.Fixed && drive.DriveType != DriveType.Removable)
            {
                continue;
            }

            candidates.Add(Path.Combine(drive.RootDirectory.FullName, "Steam"));
            candidates.Add(Path.Combine(drive.RootDirectory.FullName, "Program Files (x86)", "Steam"));
            candidates.Add(Path.Combine(drive.RootDirectory.FullName, "Program Files", "Steam"));
        }

        foreach (var candidate in candidates)
        {
            if (!string.IsNullOrWhiteSpace(candidate) && Directory.Exists(candidate))
            {
                yield return candidate;
            }
        }
    }

    private static IEnumerable<string> ResolveProgramFilesRoots()
    {
        var candidates = new List<string?>
        {
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            @"C:\Program Files",
            @"C:\Program Files (x86)",
        };

        foreach (var drive in DriveInfo.GetDrives())
        {
            if (drive.DriveType != DriveType.Fixed && drive.DriveType != DriveType.Removable)
            {
                continue;
            }

            candidates.Add(Path.Combine(drive.RootDirectory.FullName, "Program Files"));
            candidates.Add(Path.Combine(drive.RootDirectory.FullName, "Program Files (x86)"));
        }

        foreach (var candidate in candidates)
        {
            if (!string.IsNullOrWhiteSpace(candidate))
            {
                yield return candidate;
            }
        }
    }

    internal static IEnumerable<string> ReadSteamLibraryFolders(string libraryFoldersPath)
    {
        if (!File.Exists(libraryFoldersPath))
        {
            yield break;
        }

        string[] lines;
        try
        {
            lines = File.ReadAllLines(libraryFoldersPath);
        }
        catch
        {
            yield break;
        }

        foreach (var rawLine in lines)
        {
            var line = rawLine.Trim();
            if (!line.StartsWith("\"path\"", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            var parts = line.Split('"', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            if (parts.Length < 2)
            {
                continue;
            }

            var path = parts[^1].Replace(@"\\", @"\");
            if (Directory.Exists(path))
            {
                yield return path;
            }
        }
    }
    
    public static string? ResolveDefaultKboFixDllPath()
    {
        var baseDir = AppContext.BaseDirectory;
        var candidates = new[]
        {
            Path.Combine(baseDir, "KBOFix.dll"),
            Path.GetFullPath(Path.Combine(baseDir, "..", "..", "..", "native", "bin", "KBOFix.dll")),
            Path.GetFullPath(Path.Combine(baseDir, "..", "..", "..", "..", "..", "..", "native", "bin", "KBOFix.dll")),
        };
    
        return candidates
            .Where(File.Exists)
            .Select(path => new FileInfo(path))
            .OrderByDescending(file => file.LastWriteTimeUtc)
            .Select(file => file.FullName)
            .FirstOrDefault();
    }
    
    public static string GetLogPath()
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return Path.Combine(local, "OOTP-KBO", "launcher.log");
    }
    
    public static string GetKboLocalDataPath(string fileName)
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return Path.Combine(local, "OOTP-KBO", fileName);
    }

    public static string GetKboFlagConfigPath()
    {
        return GetKboLocalDataPath("kbo_flags.json");
    }

    public static string GetKboSettingsConfigPath()
    {
        return GetKboLocalDataPath("kbo_settings.json");
    }
}
