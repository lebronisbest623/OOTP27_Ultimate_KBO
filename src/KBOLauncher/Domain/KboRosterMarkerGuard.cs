internal static class KboRosterMarkerGuard
{
    public const string RequiredMarkerUrl = OotpProduct.RosterMarkerUrl;
    private const string SaveCompletedFileName = OotpProduct.SaveCompletedFileName;
    private const string SaveCompletedSentinel = OotpProduct.SaveCompletedSentinel;

    public static RosterMarkerInfo CheckCurrentSave(int pid, Action<string>? log = null)
    {
        return CheckCurrentSave(pid, null, log);
    }

    public static RosterMarkerInfo CheckCurrentSave(int pid, DateTimeOffset? minSaveCompletedAt, Action<string>? log = null)
    {
        return CheckCurrentSave(pid, minSaveCompletedAt, allowIncompleteMarkedSave: false, log);
    }

    public static RosterMarkerInfo CheckCurrentSave(
        int pid,
        DateTimeOffset? minSaveCompletedAt,
        bool allowIncompleteMarkedSave,
        Action<string>? log = null)
    {
        var savePathInfo = OotpCurrentSavePathReader.TryRead(pid, log);
        if (!savePathInfo.Ok)
        {
            return RosterMarkerInfo.Fail(savePathInfo.Status, savePathInfo.SavePath, null, savePathInfo.Error ?? "current save path unavailable");
        }

        return CheckSavePath(savePathInfo.SavePath!, minSaveCompletedAt, allowIncompleteMarkedSave);
    }

    internal static RosterMarkerInfo CheckSavePath(string savePath)
    {
        return CheckSavePath(savePath, null);
    }

    internal static RosterMarkerInfo CheckSavePath(string savePath, DateTimeOffset? minSaveCompletedAt)
    {
        return CheckSavePath(savePath, minSaveCompletedAt, allowIncompleteMarkedSave: false);
    }

    internal static RosterMarkerInfo CheckSavePath(
        string savePath,
        DateTimeOffset? minSaveCompletedAt,
        bool allowIncompleteMarkedSave)
    {
        var normalizedSavePath = Path.GetFullPath(savePath);
        if (!Directory.Exists(normalizedSavePath))
        {
            return RosterMarkerInfo.Fail("current_save_missing", normalizedSavePath, null, "current save directory was not found");
        }
        if (!normalizedSavePath.EndsWith(OotpProduct.SaveGameExtension, StringComparison.OrdinalIgnoreCase))
        {
            return RosterMarkerInfo.Fail("current_save_not_lg", normalizedSavePath, null, "current save path is not an .lg directory");
        }

        var descriptionPath = Path.Combine(normalizedSavePath, "description.txt");
        if (!File.Exists(descriptionPath))
        {
            return RosterMarkerInfo.Fail("description_missing", normalizedSavePath, descriptionPath, "description.txt not found");
        }

        string text;
        try
        {
            text = File.ReadAllText(descriptionPath);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return RosterMarkerInfo.Fail("description_unreadable", normalizedSavePath, descriptionPath, $"{ex.GetType().Name}: {ex.Message}");
        }

        if (text.IndexOf(RequiredMarkerUrl, StringComparison.OrdinalIgnoreCase) < 0)
        {
            return RosterMarkerInfo.Fail("marker_missing", normalizedSavePath, descriptionPath, "required GitHub marker was not found");
        }

        var saveCompletedPath = Path.Combine(normalizedSavePath, SaveCompletedFileName);
        if (!File.Exists(saveCompletedPath))
        {
            if (allowIncompleteMarkedSave)
            {
                return new RosterMarkerInfo(true, "marked_save_in_progress", normalizedSavePath, descriptionPath, saveCompletedPath, null, null);
            }
            return RosterMarkerInfo.Fail("save_not_completed", normalizedSavePath, descriptionPath, saveCompletedPath, $"{SaveCompletedFileName} not found");
        }

        string completedText;
        DateTimeOffset completedAt;
        try
        {
            completedText = File.ReadAllText(saveCompletedPath);
            completedAt = File.GetLastWriteTimeUtc(saveCompletedPath);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return RosterMarkerInfo.Fail("save_completion_unreadable", normalizedSavePath, descriptionPath, saveCompletedPath, $"{ex.GetType().Name}: {ex.Message}");
        }

        if (completedText.IndexOf(SaveCompletedSentinel, StringComparison.OrdinalIgnoreCase) < 0)
        {
            if (allowIncompleteMarkedSave)
            {
                return new RosterMarkerInfo(true, "marked_save_in_progress", normalizedSavePath, descriptionPath, saveCompletedPath, completedAt, null);
            }
            return RosterMarkerInfo.Fail("save_not_completed", normalizedSavePath, descriptionPath, saveCompletedPath, $"{SaveCompletedFileName} does not contain the completed save sentinel");
        }

        if (minSaveCompletedAt is not null && completedAt < minSaveCompletedAt.Value.ToUniversalTime())
        {
            if (allowIncompleteMarkedSave)
            {
                return new RosterMarkerInfo(true, "marked_save_in_progress", normalizedSavePath, descriptionPath, saveCompletedPath, completedAt, null);
            }
            return RosterMarkerInfo.Fail(
                "save_completion_stale",
                normalizedSavePath,
                descriptionPath,
                saveCompletedPath,
                $"{SaveCompletedFileName} was last written before this launcher run");
        }

        return new RosterMarkerInfo(true, "marked_save_completed", normalizedSavePath, descriptionPath, saveCompletedPath, completedAt, null);
    }

    internal static RosterMarkerInfo? FindLatestMarkedCompletedSave(DateTimeOffset? minSaveCompletedAt = null)
    {
        return FindLatestMarkedCompletedSaveInDirectory(OotpProduct.DefaultSavedGamesDirectory, minSaveCompletedAt);
    }

    internal static RosterMarkerInfo? FindLatestMarkedSaveForInjection(DateTimeOffset? minTouchedAt = null)
    {
        return FindLatestMarkedSaveForInjectionInDirectory(OotpProduct.DefaultSavedGamesDirectory, minTouchedAt);
    }

    internal static RosterMarkerInfo? FindLatestMarkedCompletedSaveInDirectory(
        string savedGamesPath,
        DateTimeOffset? minSaveCompletedAt = null)
    {
        if (!Directory.Exists(savedGamesPath))
        {
            return null;
        }

        return Directory.EnumerateDirectories(savedGamesPath, OotpProduct.SaveGameSearchPattern, SearchOption.TopDirectoryOnly)
            .Select(path =>
            {
                try
                {
                    var info = CheckSavePath(path, minSaveCompletedAt);
                    var completedAt = info.SaveCompletedAt ?? DateTimeOffset.MinValue;
                    return (info, completedAt);
                }
                catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
                {
                    return (RosterMarkerInfo.Fail("save_scan_failed", path, null, ex.Message), DateTimeOffset.MinValue);
                }
            })
            .Where(candidate => candidate.Item1.Ok)
            .OrderByDescending(candidate => candidate.Item2)
            .Select(candidate => candidate.Item1)
            .FirstOrDefault();
    }

    internal static RosterMarkerInfo? FindLatestMarkedSaveForInjectionInDirectory(
        string savedGamesPath,
        DateTimeOffset? minTouchedAt = null)
    {
        if (!Directory.Exists(savedGamesPath))
        {
            return null;
        }

        var minUtc = minTouchedAt?.ToUniversalTime();
        return Directory.EnumerateDirectories(savedGamesPath, OotpProduct.SaveGameSearchPattern, SearchOption.TopDirectoryOnly)
            .Select(path =>
            {
                try
                {
                    var touchedAt = Directory.GetLastWriteTimeUtc(path);
                    if (minUtc is not null && touchedAt < minUtc.Value)
                    {
                        return (info: (RosterMarkerInfo?)null, touchedAt);
                    }

                    var info = CheckSavePath(path, minSaveCompletedAt: null, allowIncompleteMarkedSave: true);
                    return (info: info.Ok ? info : null, touchedAt);
                }
                catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
                {
                    return (info: (RosterMarkerInfo?)null, touchedAt: DateTimeOffset.MinValue);
                }
            })
            .Where(candidate => candidate.info is not null)
            .OrderByDescending(candidate => candidate.touchedAt)
            .Select(candidate => candidate.info!)
            .FirstOrDefault();
    }

    public static string FormatConsoleStatus(RosterMarkerInfo info)
    {
        if (info.Ok)
        {
            return $"KBO roster marker: current save marked and saved in {info.DescriptionPath}";
        }

        return $"KBO roster marker: blocked ({info.Status})";
    }

    public static string FormatLogStatus(RosterMarkerInfo info)
    {
        var save = string.IsNullOrWhiteSpace(info.SavePath) ? "" : info.SavePath;
        var description = string.IsNullOrWhiteSpace(info.DescriptionPath) ? "" : info.DescriptionPath;
        var saveCompleted = string.IsNullOrWhiteSpace(info.SaveCompletedPath) ? "" : info.SaveCompletedPath;
        var completedAt = info.SaveCompletedAt is null ? "" : info.SaveCompletedAt.Value.ToString("O");
        var error = string.IsNullOrWhiteSpace(info.Error) ? "" : info.Error;
        return $"roster_marker status={info.Status} ok={(info.Ok ? "1" : "0")} save=\"{save}\" description=\"{description}\" save_completed=\"{saveCompleted}\" save_completed_at=\"{completedAt}\" error=\"{error}\"";
    }
}

