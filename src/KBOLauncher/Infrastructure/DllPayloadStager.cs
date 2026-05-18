using static LauncherPaths;
using static LauncherLog;

internal static class DllPayloadStager
{
    private static readonly TimeSpan StagedDllRetention = TimeSpan.FromDays(7);

    public static string PrepareInjectableDllCopy(string dllPath, string logPath)
    {
        return PrepareInjectableDllCopy(dllPath, GetKboLocalDataPath("run_dlls"), logPath);
    }

    internal static string PrepareInjectableDllCopy(string dllPath, string runDllDir, string logPath)
    {
        var fullDllPath = Path.GetFullPath(dllPath);
        if (!File.Exists(fullDllPath))
        {
            throw new FileNotFoundException("Native DLL was not found.", fullDllPath);
        }
    
        Directory.CreateDirectory(runDllDir);
        CleanupOldStagedDlls(runDllDir, DateTimeOffset.Now - StagedDllRetention, logPath);
    
        var extension = Path.GetExtension(fullDllPath);
        var stem = Path.GetFileNameWithoutExtension(fullDllPath);
        var copyPath = Path.Combine(
            runDllDir,
            $"{stem}-{DateTimeOffset.Now:yyyyMMddHHmmssfff}-{Environment.ProcessId}{extension}");
    
        File.Copy(fullDllPath, copyPath, overwrite: false);
    
        var loaderSource = Path.Combine(Path.GetDirectoryName(fullDllPath) ?? string.Empty, "WebView2Loader.dll");
        if (File.Exists(loaderSource))
        {
            var loaderTarget = Path.Combine(runDllDir, "WebView2Loader.dll");
            File.Copy(loaderSource, loaderTarget, overwrite: true);
            Log(logPath, $"webview2_loader_copy source={loaderSource} target={loaderTarget}");
        }

        var assetSource = Path.Combine(Path.GetDirectoryName(fullDllPath) ?? string.Empty, "assets");
        if (Directory.Exists(assetSource))
        {
            var assetTarget = Path.Combine(runDllDir, "assets");
            CopyDirectory(assetSource, assetTarget);
            Log(logPath, $"assets_copy source={assetSource} target={assetTarget}");
        }

        var sourceDir = Path.GetDirectoryName(fullDllPath) ?? string.Empty;
        var toolSource = ResolveToolDirectory(sourceDir);
        if (toolSource is not null)
        {
            var toolTarget = Path.Combine(runDllDir, "tools");
            var copied = CopyToolPayloads(toolSource, toolTarget);
            if (copied > 0)
            {
                Log(logPath, $"tools_copy source={toolSource} target={toolTarget} files={copied}");
            }
        }
    
        Log(logPath, $"dll_copy source={fullDllPath} target={copyPath}");
        return copyPath;
    }

    private static string? ResolveToolDirectory(string primaryRoot)
    {
        foreach (var root in EnumerateCompanionSearchRoots(primaryRoot))
        {
            var candidate = Path.Combine(root, "tools");
            if (Directory.Exists(candidate) && ContainsToolPayload(candidate))
            {
                return candidate;
            }
        }

        return null;
    }

    private static IEnumerable<string> EnumerateCompanionSearchRoots(string primaryRoot)
    {
        var yielded = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var root in EnumerateRootAndParents(primaryRoot))
        {
            if (yielded.Add(root))
            {
                yield return root;
            }
        }
    }

    private static IEnumerable<string> EnumerateRootAndParents(string root)
    {
        if (string.IsNullOrWhiteSpace(root))
        {
            yield break;
        }

        DirectoryInfo? current;
        try
        {
            current = new DirectoryInfo(Path.GetFullPath(root));
        }
        catch (Exception ex) when (ex is ArgumentException or NotSupportedException or PathTooLongException)
        {
            yield break;
        }

        for (var depth = 0; current is not null && depth < 8; depth++, current = current.Parent)
        {
            yield return current.FullName;
        }
    }

    private static int CopyToolPayloads(string sourceDir, string targetDir)
    {
        if ((File.GetAttributes(sourceDir) & FileAttributes.ReparsePoint) != 0)
        {
            throw new IOException($"Refusing to copy reparse point tool directory: {sourceDir}");
        }

        Directory.CreateDirectory(targetDir);
        var copied = 0;
        foreach (var fileName in new[] { "kbo_optimizer.exe", "kbo_optimizer.py" })
        {
            var sourcePath = Path.Combine(sourceDir, fileName);
            if (!File.Exists(sourcePath))
            {
                continue;
            }
            if ((File.GetAttributes(sourcePath) & FileAttributes.ReparsePoint) != 0)
            {
                throw new IOException($"Refusing to copy reparse point tool file: {sourcePath}");
            }

            File.Copy(sourcePath, Path.Combine(targetDir, fileName), overwrite: true);
            copied++;
        }

        var packageSource = Path.Combine(sourceDir, "kbo_optimizer_lib");
        if (Directory.Exists(packageSource))
        {
            var packageTarget = Path.Combine(targetDir, "kbo_optimizer_lib");
            CopyDirectory(packageSource, packageTarget);
            copied += Directory.EnumerateFiles(packageSource, "*", SearchOption.AllDirectories).Count();
        }

        return copied;
    }

    private static bool ContainsToolPayload(string sourceDir)
    {
        return File.Exists(Path.Combine(sourceDir, "kbo_optimizer.exe"))
            || File.Exists(Path.Combine(sourceDir, "kbo_optimizer.py"));
    }

    private static void CopyDirectory(string sourceDir, string targetDir)
    {
        if ((File.GetAttributes(sourceDir) & FileAttributes.ReparsePoint) != 0)
        {
            throw new IOException($"Refusing to copy reparse point asset directory: {sourceDir}");
        }

        Directory.CreateDirectory(targetDir);
        foreach (var file in Directory.EnumerateFiles(sourceDir, "*", SearchOption.TopDirectoryOnly))
        {
            if ((File.GetAttributes(file) & FileAttributes.ReparsePoint) != 0)
            {
                throw new IOException($"Refusing to copy reparse point asset file: {file}");
            }

            File.Copy(file, Path.Combine(targetDir, Path.GetFileName(file)), overwrite: true);
        }

        foreach (var directory in Directory.EnumerateDirectories(sourceDir, "*", SearchOption.TopDirectoryOnly))
        {
            CopyDirectory(directory, Path.Combine(targetDir, Path.GetFileName(directory)));
        }
    }

    private static void CleanupOldStagedDlls(string runDllDir, DateTimeOffset cutoff, string logPath)
    {
        foreach (var path in Directory.EnumerateFiles(runDllDir, "KBOFix-*.dll", SearchOption.TopDirectoryOnly))
        {
            try
            {
                if (File.GetLastWriteTimeUtc(path) >= cutoff.UtcDateTime)
                {
                    continue;
                }

                File.Delete(path);
                Log(logPath, $"staged_dll_cleanup deleted={path}");
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                Log(logPath, $"staged_dll_cleanup failed={path} error=\"{ex.Message}\"");
            }
        }
    }
}
