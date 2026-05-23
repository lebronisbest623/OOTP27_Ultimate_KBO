namespace KBOLauncher.Tests;

using FluentAssertions;
using Xunit;

public sealed class RosterMarkerGuardTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-roster-marker-tests", Guid.NewGuid().ToString("N"));
    private const string SaveCompletedText = "2026-05-09 23:44:08\tFinished save_database, closing flag file now";

    [Fact]
    public void CheckSavePath_FailsWhenSaveDirectoryIsMissing()
    {
        var missing = Path.Combine(tempDir, "Missing.lg");

        var info = global::KboRosterMarkerGuard.CheckSavePath(missing);

        info.Ok.Should().BeFalse();
        info.Status.Should().Be("current_save_missing");
    }

    [Fact]
    public void CheckSavePath_FailsWhenDirectoryIsNotLgSave()
    {
        var savePath = Path.Combine(tempDir, "NotALeague");
        Directory.CreateDirectory(savePath);

        var info = global::KboRosterMarkerGuard.CheckSavePath(savePath);

        info.Ok.Should().BeFalse();
        info.Status.Should().Be("current_save_not_lg");
    }

    [Fact]
    public void CheckSavePath_FailsClosedWhenDescriptionIsMissingOrUnmarked()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);

        var missingDescription = global::KboRosterMarkerGuard.CheckSavePath(savePath);
        missingDescription.Ok.Should().BeFalse();
        missingDescription.Status.Should().Be("description_missing");

        File.WriteAllText(Path.Combine(savePath, "description.txt"), "plain roster");
        var missingMarker = global::KboRosterMarkerGuard.CheckSavePath(savePath);
        missingMarker.Ok.Should().BeFalse();
        missingMarker.Status.Should().Be("marker_missing");
    }

    [Fact]
    public void CheckSavePath_FailsClosedWhenMarkedSaveWasNotCompleted()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);
        File.WriteAllText(Path.Combine(savePath, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl.ToUpperInvariant());

        var info = global::KboRosterMarkerGuard.CheckSavePath(savePath);

        info.Ok.Should().BeFalse();
        info.Status.Should().Be("save_not_completed");
        info.SaveCompletedPath.Should().EndWith("flag_save_completed.dat");
    }

    [Fact]
    public void CheckSavePath_FailsClosedWhenCompletedSaveIsOlderThanRequired()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);
        File.WriteAllText(Path.Combine(savePath, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl.ToUpperInvariant());
        var completedPath = Path.Combine(savePath, "flag_save_completed.dat");
        File.WriteAllText(completedPath, SaveCompletedText);
        File.SetLastWriteTimeUtc(completedPath, new DateTime(2026, 5, 9, 14, 44, 8, DateTimeKind.Utc));

        var info = global::KboRosterMarkerGuard.CheckSavePath(
            savePath,
            new DateTimeOffset(2026, 5, 9, 14, 45, 0, TimeSpan.Zero));

        info.Ok.Should().BeFalse();
        info.Status.Should().Be("save_completion_stale");
    }

    [Fact]
    public void CheckSavePath_AllowsIncompleteMarkedSaveForInjection()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);
        File.WriteAllText(Path.Combine(savePath, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl);

        var info = global::KboRosterMarkerGuard.CheckSavePath(
            savePath,
            minSaveCompletedAt: null,
            allowIncompleteMarkedSave: true);

        info.Ok.Should().BeTrue();
        info.Status.Should().Be("marked_save_in_progress");
        info.SavePath.Should().Be(Path.GetFullPath(savePath));
    }

    [Fact]
    public void CheckSavePath_AllowsStaleMarkedSaveForInjection()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);
        File.WriteAllText(Path.Combine(savePath, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl);
        var completedPath = Path.Combine(savePath, "flag_save_completed.dat");
        File.WriteAllText(completedPath, SaveCompletedText);
        File.SetLastWriteTimeUtc(completedPath, new DateTime(2026, 5, 9, 14, 44, 8, DateTimeKind.Utc));

        var info = global::KboRosterMarkerGuard.CheckSavePath(
            savePath,
            new DateTimeOffset(2026, 5, 9, 14, 45, 0, TimeSpan.Zero),
            allowIncompleteMarkedSave: true);

        info.Ok.Should().BeTrue();
        info.Status.Should().Be("marked_save_in_progress");
    }

    [Fact]
    public void CheckSavePath_AllowsMarkedCompletedLgSaveCaseInsensitively()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);
        File.WriteAllText(Path.Combine(savePath, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl.ToUpperInvariant());
        File.WriteAllText(Path.Combine(savePath, "flag_save_completed.dat"), SaveCompletedText);

        var info = global::KboRosterMarkerGuard.CheckSavePath(savePath);

        info.Ok.Should().BeTrue();
        info.Status.Should().Be("marked_save_completed");
        info.SavePath.Should().Be(Path.GetFullPath(savePath));
        info.DescriptionPath.Should().Be(Path.Combine(Path.GetFullPath(savePath), "description.txt"));
        info.SaveCompletedPath.Should().Be(Path.Combine(Path.GetFullPath(savePath), "flag_save_completed.dat"));
        global::KboRosterMarkerGuard.FormatLogStatus(info).Should().Contain("ok=1");
        global::KboRosterMarkerGuard.FormatConsoleStatus(info).Should().Contain("current save marked and saved");
    }

    [Fact]
    public void FindLatestMarkedCompletedSave_UsesDefaultSavedGamesDirectoryAndMinimumCompletionTime()
    {
        var savedGames = Path.Combine(tempDir, "saved_games");
        var oldSave = Path.Combine(savedGames, "Old.lg");
        var newSave = Path.Combine(savedGames, "New.lg");

        CreateMarkedSave(oldSave, new DateTime(2026, 5, 9, 14, 0, 0, DateTimeKind.Utc));
        CreateMarkedSave(newSave, new DateTime(2026, 5, 9, 15, 0, 0, DateTimeKind.Utc));

        var info = global::KboRosterMarkerGuard.FindLatestMarkedCompletedSaveInDirectory(
            savedGames,
            new DateTimeOffset(2026, 5, 9, 14, 30, 0, TimeSpan.Zero));

        info.Should().NotBeNull();
        info!.Ok.Should().BeTrue();
        info.SavePath.Should().Be(Path.GetFullPath(newSave));
    }

    [Fact]
    public void FindLatestMarkedSaveForInjection_UsesRecentIncompleteMarkedSave()
    {
        var savedGames = Path.Combine(tempDir, "saved_games");
        var oldSave = Path.Combine(savedGames, "Old.lg");
        var newSave = Path.Combine(savedGames, "New.lg");

        Directory.CreateDirectory(oldSave);
        File.WriteAllText(Path.Combine(oldSave, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl);
        Directory.SetLastWriteTimeUtc(oldSave, new DateTime(2026, 5, 9, 14, 0, 0, DateTimeKind.Utc));

        Directory.CreateDirectory(newSave);
        File.WriteAllText(Path.Combine(newSave, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl);
        Directory.SetLastWriteTimeUtc(newSave, new DateTime(2026, 5, 9, 15, 0, 0, DateTimeKind.Utc));

        var info = global::KboRosterMarkerGuard.FindLatestMarkedSaveForInjectionInDirectory(
            savedGames,
            new DateTimeOffset(2026, 5, 9, 14, 30, 0, TimeSpan.Zero));

        info.Should().NotBeNull();
        info!.Ok.Should().BeTrue();
        info.Status.Should().Be("marked_save_in_progress");
        info.SavePath.Should().Be(Path.GetFullPath(newSave));
    }

    [Fact]
    public void FormatStatuses_IncludeFailureContextForInjectionLogs()
    {
        var savePath = Path.Combine(tempDir, "Plain.lg");
        var descriptionPath = Path.Combine(savePath, "description.txt");
        var info = global::RosterMarkerInfo.Fail("marker_missing", savePath, descriptionPath, "required marker missing");

        var log = global::KboRosterMarkerGuard.FormatLogStatus(info);

        global::KboRosterMarkerGuard.FormatConsoleStatus(info).Should().Be("KBO roster marker: blocked (marker_missing)");
        log.Should().Contain("status=marker_missing");
        log.Should().Contain("ok=0");
        log.Should().Contain($"save=\"{savePath}\"");
        log.Should().Contain($"description=\"{descriptionPath}\"");
        log.Should().Contain("required marker missing");
    }

    [Fact]
    public void SelectCurrentSavePathCacheValue_OnlyCachesSuccessfulCurrentSave()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        var directOk = new global::RosterMarkerInfo(
            true,
            "marked_save_completed",
            savePath,
            Path.Combine(savePath, "description.txt"),
            Path.Combine(savePath, "flag_save_completed.dat"),
            DateTimeOffset.UtcNow,
            null);
        var directFailed = global::RosterMarkerInfo.Fail(
            "current_save_unavailable",
            null,
            null,
            "current save path unavailable");

        global::LauncherGuardStatus.SelectCurrentSavePathCacheValue(directOk).Should().Be(savePath);
        global::LauncherGuardStatus.SelectCurrentSavePathCacheValue(directFailed).Should().BeNull();
        global::LauncherGuardStatus.SelectCurrentSavePathCacheValue(null).Should().BeNull();
    }

    [Fact]
    public void WriteCurrentSavePathCache_RemovesStaleCacheWhenSavePathIsUnavailable()
    {
        var pid = int.MaxValue - Math.Abs(Guid.NewGuid().GetHashCode() % 100000);
        var logPath = Path.Combine(tempDir, "launcher.log");
        var cachePath = global::LauncherPaths.GetKboLocalDataPath($"current_save_path_{pid}.txt");

        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(cachePath)!);
            File.WriteAllText(cachePath, "stale-save");

            global::LauncherGuardStatus.WriteCurrentSavePathCache(pid, null, logPath);

            File.Exists(cachePath).Should().BeFalse();
        }
        finally
        {
            if (File.Exists(cachePath))
            {
                File.Delete(cachePath);
            }
        }
    }

    public void Dispose()
    {
        try
        {
            if (Directory.Exists(tempDir))
            {
                Directory.Delete(tempDir, recursive: true);
            }
        }
        catch (IOException)
        {
        }
    }

    private static void CreateMarkedSave(string savePath, DateTime completedUtc)
    {
        Directory.CreateDirectory(savePath);
        File.WriteAllText(Path.Combine(savePath, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl);
        var completedPath = Path.Combine(savePath, "flag_save_completed.dat");
        File.WriteAllText(completedPath, SaveCompletedText);
        File.SetLastWriteTimeUtc(completedPath, completedUtc);
    }
}
