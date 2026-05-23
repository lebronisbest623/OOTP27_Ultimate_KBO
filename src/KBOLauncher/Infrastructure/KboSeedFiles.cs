using System.Text.Json;
using System.Text.Encodings.Web;
using Spectre.Console;
using static LauncherPaths;

internal static partial class KboSeedFiles
{
    private const string SeedManifestFileName = "seed_manifest.json";

    private static readonly JsonSerializerOptions SeedManifestJsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };

    private static readonly JsonSerializerOptions BundleJsonOptions = new()
    {
        WriteIndented = false,
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    public static void EnsureKboLeagueIdConfig()
    {
        var localDir = OotpProduct.LocalDataDirectory;
        EnsureKboLeagueIdConfig(localDir,
        [
            Path.Combine(AppContext.BaseDirectory, OotpProduct.LeagueIdFileName),
            Path.Combine(Environment.CurrentDirectory, OotpProduct.LeagueIdFileName),
            Path.Combine(AppContext.BaseDirectory, "native", OotpProduct.LeagueIdFileName)
        ]);
    }

    internal static void EnsureKboLeagueIdConfig(string localDir, IReadOnlyList<string> candidates)
    {
        foreach (var candidate in candidates)
        {
            if (!File.Exists(candidate))
            {
                continue;
            }
    
            try
            {
                UpsertBundledKboData(localDir, OotpProduct.LeagueIdFileName, File.ReadAllText(candidate));
                RemoveLegacyBundledKboDataFileIfUnchanged(localDir, OotpProduct.LeagueIdFileName, candidate);
                AnsiConsole.MarkupLineInterpolated($"[green]KBO league id: bundled[/] {GetBundlePath(localDir)}");
                return;
            }
            catch (Exception ex)
            {
                AnsiConsole.MarkupLineInterpolated($"[yellow]Failed to seed {OotpProduct.LeagueIdFileName} from {candidate}: {ex.Message}[/]");
                return;
            }
        }

        AnsiConsole.MarkupLineInterpolated($"[yellow]{OotpProduct.LeagueIdFileName} not found in launcher directory. Set it manually at:[/]");
        AnsiConsole.WriteLine(GetBundlePath(localDir));
    }
    
    public static void EnsureBundledKboDataManifest()
    {
        var localDir = OotpProduct.LocalDataDirectory;
        var manifestPath = ResolveBundledKboDataFileCandidates(SeedManifestFileName).FirstOrDefault(File.Exists);
        if (manifestPath is null)
        {
            Console.WriteLine($"{SeedManifestFileName}: bundled seed manifest not found");
            return;
        }

        EnsureBundledKboDataManifest(localDir, manifestPath, Path.GetDirectoryName(manifestPath)!);
    }

    internal static void EnsureBundledKboDataManifest(string localDir, string manifestPath, string dataRoot)
    {
        Directory.CreateDirectory(localDir);

        KboSeedManifest? manifest;
        try
        {
            manifest = JsonSerializer.Deserialize<KboSeedManifest>(
                File.ReadAllText(manifestPath),
                SeedManifestJsonOptions);
        }
        catch (Exception ex)
        {
            AnsiConsole.MarkupLineInterpolated($"[yellow]Failed to read {SeedManifestFileName}: {ex.Message}[/]");
            return;
        }

        if (manifest?.Groups is null)
        {
            AnsiConsole.MarkupLineInterpolated($"[yellow]{SeedManifestFileName}: no seed groups found[/]");
            return;
        }

        var seeded = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var bundle = ReadBundle(localDir);
        foreach (var file in manifest.Groups.SelectMany(group => group.Files ?? []))
        {
            if (string.IsNullOrWhiteSpace(file.Path))
            {
                continue;
            }

            string targetRelativePath;
            try
            {
                targetRelativePath = NormalizeSeedManifestRelativePath(file.Path);
            }
            catch (Exception ex)
            {
                AnsiConsole.MarkupLineInterpolated($"[yellow]{SeedManifestFileName}: ignored invalid seed path '{file.Path}': {ex.Message}[/]");
                continue;
            }

            var source = string.IsNullOrWhiteSpace(file.Source) ? file.Path : file.Source;
            string sourceRelativePath;
            try
            {
                sourceRelativePath = NormalizeSeedManifestRelativePath(source);
            }
            catch (Exception ex)
            {
                AnsiConsole.MarkupLineInterpolated($"[yellow]{SeedManifestFileName}: ignored invalid seed source '{source}': {ex.Message}[/]");
                continue;
            }

            if (!seeded.Add(targetRelativePath))
            {
                continue;
            }

            var candidates = new List<string> { Path.Combine(dataRoot, sourceRelativePath) };
            candidates.AddRange(ResolveBundledKboDataFileCandidates(sourceRelativePath));
            if (!string.Equals(sourceRelativePath, targetRelativePath, StringComparison.OrdinalIgnoreCase))
            {
                candidates.Add(Path.Combine(dataRoot, targetRelativePath));
                candidates.AddRange(ResolveBundledKboDataFileCandidates(targetRelativePath));
            }

            var candidate = candidates.FirstOrDefault(File.Exists);
            if (candidate is null)
            {
                AnsiConsole.MarkupLineInterpolated($"[yellow]{ManifestLabel(file)}: bundled seed not found for {targetRelativePath}[/]");
                continue;
            }

            try
            {
                bundle.Files[BundleKey(targetRelativePath)] = File.ReadAllText(candidate);
            }
            catch (Exception ex)
            {
                AnsiConsole.MarkupLineInterpolated($"[yellow]Failed to bundle {targetRelativePath} from {candidate}: {ex.Message}[/]");
            }
        }

        WriteBundle(localDir, bundle);
        AnsiConsole.MarkupLineInterpolated($"[green]KBO data bundle: updated[/] {GetBundlePath(localDir)}");

        foreach (var retiredFile in manifest.RetiredFiles ?? [])
        {
            if (string.IsNullOrWhiteSpace(retiredFile.Path))
            {
                continue;
            }

            string relativePath;
            try
            {
                relativePath = NormalizeSeedManifestRelativePath(retiredFile.Path);
            }
            catch
            {
                continue;
            }

            RemoveRetiredBundledKboDataFileIfUnchanged(localDir, relativePath, ManifestLabel(retiredFile), retiredFile);
            RemoveRetiredLegacyBundledKboDataFileIfUnchanged(localDir, relativePath, ManifestLabel(retiredFile), retiredFile);
        }

        RemoveLegacyBundledDataDirectoryIfSafe(localDir, bundle);
    }

    public static void EnsureBundledKboDataFile(string fileName, string label)
    {
        var localDir = OotpProduct.LocalDataDirectory;
        EnsureBundledKboDataFile(localDir, fileName, label, ResolveBundledKboDataFileCandidates(fileName));
    }

    public static void EnsureBundledKboDataDirectory(string directoryName, string label)
    {
        var localDir = OotpProduct.LocalDataDirectory;
        EnsureBundledKboDataDirectory(localDir, directoryName, label,
        [
            Path.Combine(AppContext.BaseDirectory, "data", "seeds", directoryName),
            Path.Combine(Environment.CurrentDirectory, "data", "seeds", directoryName),
            Path.Combine(AppContext.BaseDirectory, directoryName),
            Path.Combine(Environment.CurrentDirectory, directoryName)
        ]);
    }

    private static void RemoveRetiredBundledKboDataFileIfUnchanged(
        string localDir,
        string fileName,
        string label,
        KboSeedManifestFile? retiredFile)
    {
        var localPath = GetBundledKboDataPath(localDir, fileName);
        RemoveRetiredBundledKboDataFileAtPathIfUnchanged(localPath, fileName, label, retiredFile);
    }

    private static void RemoveRetiredLegacyBundledKboDataFileIfUnchanged(
        string localDir,
        string fileName,
        string label,
        KboSeedManifestFile? retiredFile)
    {
        var localPath = Path.Combine(localDir, fileName);
        RemoveRetiredBundledKboDataFileAtPathIfUnchanged(localPath, fileName, label, retiredFile);
    }

    private static void RemoveRetiredBundledKboDataFileAtPathIfUnchanged(
        string localPath,
        string fileName,
        string label,
        KboSeedManifestFile? retiredFile)
    {
        if (!File.Exists(localPath))
        {
            return;
        }

        try
        {
            var meaningfulLines = File.ReadAllLines(localPath)
                .Select(line => line.Trim())
                .Where(line => line.Length > 0 && !line.StartsWith("#", StringComparison.Ordinal) && !line.StartsWith(";", StringComparison.Ordinal))
                .ToArray();

            if (!IsRetiredBundledKboDataFileSafeToRemove(retiredFile, meaningfulLines))
            {
                return;
            }

            File.Delete(localPath);
            AnsiConsole.MarkupLineInterpolated($"{label}: removed retired bundled seed {localPath}");
        }
        catch (Exception ex)
        {
            AnsiConsole.MarkupLineInterpolated($"[yellow]Failed to remove retired {fileName}: {ex.Message}[/]");
        }
    }

    private static bool IsRetiredBundledKboDataFileSafeToRemove(KboSeedManifestFile? retiredFile, string[] meaningfulLines)
    {
        if (meaningfulLines.Length == 0)
        {
            return false;
        }

        if (retiredFile?.SafeRemoveAllLinesStartWithAny is { Count: > 0 } prefixes
                && meaningfulLines.All(line => prefixes.Any(prefix => line.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))))
        {
            return true;
        }

        if (retiredFile?.SafeRemoveWhenContainsAny is { Count: > 0 } markers
                && meaningfulLines.Any(line => markers.Any(marker => line.Contains(marker, StringComparison.OrdinalIgnoreCase))))
        {
            return true;
        }

        return false;
    }

    internal static void EnsureBundledKboDataFile(
        string localDir,
        string fileName,
        string label,
        IReadOnlyList<string> candidates)
    {
        var localPath = GetBundledKboDataPath(localDir, fileName);

        Directory.CreateDirectory(Path.GetDirectoryName(localPath)!);

        foreach (var candidate in candidates)
        {
            if (!File.Exists(candidate))
            {
                continue;
            }
    
            try
            {
                var shouldCopy = ShouldCopyBundledKboDataFile(candidate, localPath);
                if (shouldCopy)
                {
                    File.Copy(candidate, localPath, overwrite: true);
                    AnsiConsole.MarkupLineInterpolated($"[green]{label}: seeded[/] {localPath}");
                }
                RemoveLegacyBundledKboDataFileIfUnchanged(localDir, fileName, localPath);
                return;
            }
            catch (Exception ex)
            {
                AnsiConsole.MarkupLineInterpolated($"[yellow]Failed to seed {fileName} from {candidate}: {ex.Message}[/]");
                return;
            }
        }

        AnsiConsole.MarkupLineInterpolated($"[yellow]{label}: bundled seed not found for {fileName}[/]");
    }

    private static bool ShouldCopyBundledKboDataFile(string candidate, string localPath)
    {
        if (!File.Exists(localPath))
        {
            return true;
        }

        var candidateInfo = new FileInfo(candidate);
        var localInfo = new FileInfo(localPath);
        if (candidateInfo.Length != localInfo.Length)
        {
            return true;
        }

        return !File.ReadAllBytes(candidate).AsSpan().SequenceEqual(File.ReadAllBytes(localPath));
    }

    private static string GetBundledKboDataDirectory(string localDir)
    {
        return Path.Combine(localDir, OotpProduct.BundledDataDirectoryName);
    }

    private static string GetBundledKboDataPath(string localDir, string relativePath)
    {
        return Path.Combine(GetBundledKboDataDirectory(localDir), relativePath);
    }

    private static string GetBundlePath(string localDir)
    {
        return Path.Combine(localDir, OotpProduct.BundledDataFileName);
    }

    private static string BundleKey(string relativePath)
    {
        return NormalizeSeedManifestRelativePath(relativePath).Replace(Path.DirectorySeparatorChar, '/');
    }

    private static void UpsertBundledKboData(string localDir, string relativePath, string content)
    {
        var bundle = ReadBundle(localDir);
        bundle.Files[BundleKey(relativePath)] = content;
        WriteBundle(localDir, bundle);
    }

    private static KboDataBundle ReadBundle(string localDir)
    {
        var path = GetBundlePath(localDir);
        if (!File.Exists(path))
        {
            return new KboDataBundle();
        }

        try
        {
            return JsonSerializer.Deserialize<KboDataBundle>(
                File.ReadAllText(path),
                SeedManifestJsonOptions) ?? new KboDataBundle();
        }
        catch
        {
            return new KboDataBundle();
        }
    }

    private static void WriteBundle(string localDir, KboDataBundle bundle)
    {
        Directory.CreateDirectory(localDir);
        bundle.Version = 1;
        bundle.Files = bundle.Files
            .OrderBy(pair => pair.Key, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(pair => pair.Key, pair => pair.Value, StringComparer.OrdinalIgnoreCase);
        File.WriteAllText(GetBundlePath(localDir), JsonSerializer.Serialize(bundle, BundleJsonOptions));
    }

    private static void RemoveLegacyBundledKboDataFileIfUnchanged(string localDir, string fileName, string canonicalPath)
    {
        var legacyPath = Path.Combine(localDir, fileName);
        if (!File.Exists(legacyPath)
                || !File.Exists(canonicalPath)
                || string.Equals(
                    Path.GetFullPath(legacyPath),
                    Path.GetFullPath(canonicalPath),
                    StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        try
        {
            if (File.ReadAllBytes(legacyPath).AsSpan().SequenceEqual(File.ReadAllBytes(canonicalPath)))
            {
                File.Delete(legacyPath);
            }
        }
        catch
        {
        }
    }

    private static IReadOnlyList<string> ResolveBundledKboDataFileCandidates(string relativePath)
    {
        return
        [
            Path.Combine(AppContext.BaseDirectory, "data", "seeds", relativePath),
            Path.Combine(Environment.CurrentDirectory, "data", "seeds", relativePath),
            Path.Combine(AppContext.BaseDirectory, relativePath),
            Path.Combine(Environment.CurrentDirectory, relativePath),
            Path.Combine(AppContext.BaseDirectory, "native", relativePath)
        ];
    }

    private static string NormalizeSeedManifestRelativePath(string path)
    {
        var normalized = path.Trim()
            .Replace('/', Path.DirectorySeparatorChar)
            .Replace('\\', Path.DirectorySeparatorChar);

        if (Path.IsPathFullyQualified(normalized))
        {
            throw new InvalidOperationException("absolute paths are not allowed");
        }

        var segments = normalized.Split(Path.DirectorySeparatorChar, StringSplitOptions.RemoveEmptyEntries);
        if (segments.Length == 0 || segments.Any(segment => segment == ".."))
        {
            throw new InvalidOperationException("empty paths and parent traversal are not allowed");
        }

        return Path.Combine(segments);
    }

    private static string ManifestLabel(KboSeedManifestFile file)
    {
        return string.IsNullOrWhiteSpace(file.Label) ? file.Path : file.Label;
    }

    private sealed class KboSeedManifest
    {
        public List<KboSeedManifestGroup>? Groups { get; set; }

        public List<KboSeedManifestFile>? RetiredFiles { get; set; }
    }

    private sealed class KboSeedManifestGroup
    {
        public List<KboSeedManifestFile>? Files { get; set; }
    }

    private sealed class KboSeedManifestFile
    {
        public string Path { get; set; } = "";

        public string Source { get; set; } = "";

        public string Label { get; set; } = "";

        public List<string>? SafeRemoveAllLinesStartWithAny { get; set; }

        public List<string>? SafeRemoveWhenContainsAny { get; set; }
    }

    internal static void EnsureBundledKboDataDirectory(
        string localDir,
        string directoryName,
        string label,
        IReadOnlyList<string> candidates)
    {
        var localPath = GetBundledKboDataPath(localDir, directoryName);

        Directory.CreateDirectory(localDir);

        foreach (var candidate in candidates)
        {
            if (!Directory.Exists(candidate))
            {
                continue;
            }

            try
            {
                var copied = 0;
                foreach (var sourcePath in Directory.EnumerateFiles(candidate, "*", SearchOption.AllDirectories))
                {
                    var relative = Path.GetRelativePath(candidate, sourcePath);
                    var targetPath = Path.Combine(localPath, relative);
                    Directory.CreateDirectory(Path.GetDirectoryName(targetPath)!);

                    var shouldCopy = !File.Exists(targetPath)
                        || File.GetLastWriteTimeUtc(sourcePath) > File.GetLastWriteTimeUtc(targetPath)
                        || new FileInfo(sourcePath).Length != new FileInfo(targetPath).Length;
                    if (!shouldCopy)
                    {
                        continue;
                    }

                    File.Copy(sourcePath, targetPath, overwrite: true);
                    copied++;
                }

                if (copied > 0)
                {
                    AnsiConsole.MarkupLineInterpolated($"[green]{label}: seeded {copied} file(s) under[/] {localPath}");
                }
                RemoveLegacyBundledKboDataDirectoryIfUnchanged(localDir, directoryName, localPath);
                return;
            }
            catch (Exception ex)
            {
                AnsiConsole.MarkupLineInterpolated($"[yellow]Failed to seed {directoryName} from {candidate}: {ex.Message}[/]");
                return;
            }
        }

        AnsiConsole.MarkupLineInterpolated($"[yellow]{label}: bundled seed directory not found for {directoryName}[/]");
    }

    private static void RemoveLegacyBundledKboDataDirectoryIfUnchanged(string localDir, string directoryName, string canonicalPath)
    {
        var legacyPath = Path.Combine(localDir, directoryName);
        if (!Directory.Exists(legacyPath)
                || !Directory.Exists(canonicalPath)
                || string.Equals(
                    Path.GetFullPath(legacyPath),
                    Path.GetFullPath(canonicalPath),
                    StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        try
        {
            var legacyFiles = Directory.EnumerateFiles(legacyPath, "*", SearchOption.AllDirectories).ToArray();
            foreach (var legacyFile in legacyFiles)
            {
                var relative = Path.GetRelativePath(legacyPath, legacyFile);
                var canonicalFile = Path.Combine(canonicalPath, relative);
                if (!File.Exists(canonicalFile)
                        || !File.ReadAllBytes(legacyFile).AsSpan().SequenceEqual(File.ReadAllBytes(canonicalFile)))
                {
                    return;
                }
            }

            Directory.Delete(legacyPath, recursive: true);
        }
        catch
        {
        }
    }

    private static void RemoveLegacyBundledDataDirectoryIfSafe(string localDir, KboDataBundle bundle)
    {
        var legacyRoot = GetBundledKboDataDirectory(localDir);
        if (!Directory.Exists(legacyRoot))
        {
            return;
        }

        try
        {
            foreach (var legacyFile in Directory.EnumerateFiles(legacyRoot, "*", SearchOption.AllDirectories))
            {
                var relative = Path.GetRelativePath(legacyRoot, legacyFile).Replace(Path.DirectorySeparatorChar, '/');
                if (!bundle.Files.TryGetValue(relative, out var bundledContent)
                        || File.ReadAllText(legacyFile) != bundledContent)
                {
                    return;
                }
            }

            Directory.Delete(legacyRoot, recursive: true);
        }
        catch
        {
        }
    }

    private sealed class KboDataBundle
    {
        public int Version { get; set; } = 1;

        public Dictionary<string, string> Files { get; set; } = new(StringComparer.OrdinalIgnoreCase);
    }

}
