
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
        return Path.GetFileName(path).Equals(OotpProduct.ExecutableFileName, StringComparison.OrdinalIgnoreCase);
    }

    internal static IEnumerable<string> GetOotpPathCandidates(string? explicitPath)
    {
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            yield return explicitPath;
        }

        foreach (var envVar in OotpProduct.OotpEnvironmentVariables)
        {
            var value = Environment.GetEnvironmentVariable(envVar);
            if (string.IsNullOrWhiteSpace(value))
            {
                continue;
            }

            yield return value.EndsWith(".exe", StringComparison.OrdinalIgnoreCase)
                ? value
                : Path.Combine(value, OotpProduct.ExecutableFileName);
        }

        foreach (var programFilesRoot in ResolveProgramFilesRoots())
        {
            foreach (var productFolder in OotpProduct.ProductFolderNames)
            {
                yield return Path.Combine(programFilesRoot, OotpProduct.VendorFolderName, productFolder, OotpProduct.ExecutableFileName);
                yield return Path.Combine(programFilesRoot, productFolder, OotpProduct.ExecutableFileName);
            }
        }

        foreach (var drive in DriveInfo.GetDrives())
        {
            if (drive.DriveType != DriveType.Fixed && drive.DriveType != DriveType.Removable)
            {
                continue;
            }

            foreach (var installFolder in OotpProduct.RootInstallFolderNames)
            {
                yield return Path.Combine(drive.RootDirectory.FullName, installFolder, OotpProduct.ExecutableFileName);
            }
        }

        foreach (var programFilesRoot in ResolveProgramFilesRoots())
        {
            yield return Path.Combine(
                programFilesRoot,
                OotpProduct.SteamDirectoryName,
                OotpProduct.SteamAppsDirectoryName,
                OotpProduct.SteamCommonDirectoryName,
                OotpProduct.ProductLongFolderName,
                OotpProduct.ExecutableFileName);
        }
        foreach (var candidate in ResolveSteamLibraryOotpCandidates())
        {
            yield return candidate;
        }
    }

    public static void WriteOotpPathDiscoveryStatus(string? explicitPath)
    {
        var path = GetKboLocalDataPath(OotpProduct.PathDiscoveryStatusFileName);
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
            yield return Path.Combine(
                steamRoot,
                OotpProduct.SteamAppsDirectoryName,
                OotpProduct.SteamCommonDirectoryName,
                OotpProduct.ProductLongFolderName,
                OotpProduct.ExecutableFileName);

            var libraryFolders = Path.Combine(steamRoot, OotpProduct.SteamAppsDirectoryName, "libraryfolders.vdf");
            foreach (var libraryRoot in ReadSteamLibraryFolders(libraryFolders))
            {
                yield return Path.Combine(
                    libraryRoot,
                    OotpProduct.SteamAppsDirectoryName,
                    OotpProduct.SteamCommonDirectoryName,
                    OotpProduct.ProductLongFolderName,
                    OotpProduct.ExecutableFileName);
            }
        }
    }

    private static IEnumerable<string> ResolveSteamRoots()
    {
        var candidates = new List<string?>
        {
            Environment.GetEnvironmentVariable("STEAM_DIR"),
        };

        candidates.AddRange(ResolveProgramFilesRoots().Select(root => Path.Combine(root, OotpProduct.SteamDirectoryName)));

        foreach (var drive in DriveInfo.GetDrives())
        {
            if (drive.DriveType != DriveType.Fixed && drive.DriveType != DriveType.Removable)
            {
                continue;
            }

            candidates.Add(Path.Combine(drive.RootDirectory.FullName, OotpProduct.SteamDirectoryName));
            candidates.Add(Path.Combine(drive.RootDirectory.FullName, "Program Files (x86)", OotpProduct.SteamDirectoryName));
            candidates.Add(Path.Combine(drive.RootDirectory.FullName, "Program Files", OotpProduct.SteamDirectoryName));
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
        return Path.Combine(OotpProduct.LocalDataDirectory, OotpProduct.LauncherLogFileName);
    }
    
    public static string GetKboLocalDataPath(string fileName)
    {
        return Path.Combine(OotpProduct.LocalDataDirectory, fileName);
    }

    public static string GetKboFlagConfigPath()
    {
        return GetKboLocalDataPath(OotpProduct.FlagsFileName);
    }

    public static string GetKboSettingsConfigPath()
    {
        return GetKboLocalDataPath(OotpProduct.SettingsFileName);
    }
}
