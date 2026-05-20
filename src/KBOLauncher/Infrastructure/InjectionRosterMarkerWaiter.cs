using static KboRosterMarkerGuard;
using static LauncherGuardStatus;
using static LauncherLog;

internal static class InjectionRosterMarkerWaiter
{
    public static RosterMarkerInfo WaitForMarkedCurrentSave(
        int pid,
        TimeSpan timeout,
        TimeSpan pollInterval,
        string logPath,
        DateTimeOffset? minSaveCompletedAt = null,
        DateTimeOffset? fallbackMinSaveCompletedAt = null,
        bool allowIncompleteMarkedSave = false)
    {
        var deadline = DateTimeOffset.Now + timeout;
        RosterMarkerInfo? lastInfo = null;
        var attempt = 0;

        Console.WriteLine(
            $"Waiting for marked and fully saved OOTP {OotpProduct.SaveGameExtension} save before KBOFix injection, timeout={FormatTimeout(timeout)}.");
        Log(logPath, $"roster_marker_wait started pid={pid} timeout_seconds={(int)timeout.TotalSeconds} min_save_completed_at=\"{(minSaveCompletedAt is null ? "" : minSaveCompletedAt.Value.ToString("O"))}\"");

        while (DateTimeOffset.Now <= deadline)
        {
            attempt++;
            lastInfo = ProbeRosterMarkerForInjectionTarget(
                pid,
                logPath,
                minSaveCompletedAt,
                fallbackMinSaveCompletedAt,
                allowIncompleteMarkedSave);
            Log(logPath, $"roster_marker_wait attempt={attempt} {FormatLogStatus(lastInfo)}");

            if (lastInfo.Ok)
            {
                Console.WriteLine(FormatConsoleStatus(lastInfo));
                Log(logPath, $"roster_marker_wait ready pid={pid} attempts={attempt}");
                return lastInfo;
            }

            if (IsTerminalRosterMarkerFailure(lastInfo))
            {
                Console.WriteLine(FormatConsoleStatus(lastInfo));
                Log(logPath, $"roster_marker_wait blocked pid={pid} attempts={attempt}");
                return lastInfo;
            }

            if (attempt == 1 || attempt % 15 == 0)
            {
                Console.WriteLine($"Waiting for marked save... latest status={lastInfo.Status}");
            }

            SleepUntilNextPoll(deadline, pollInterval);
        }

        var timeoutInfo = RosterMarkerInfo.Fail(
            "marked_save_wait_timeout",
            lastInfo?.SavePath,
            lastInfo?.DescriptionPath,
            lastInfo?.Error ?? "timed out waiting for a marked current save");
        WriteRosterMarkerGuardStatus(timeoutInfo);
        Log(logPath, $"roster_marker_wait timeout pid={pid} attempts={attempt} {FormatLogStatus(timeoutInfo)}");
        return timeoutInfo;
    }

    internal static bool IsTerminalRosterMarkerFailure(RosterMarkerInfo info)
    {
        return info.Status is "current_save_not_lg" or "marker_missing" or "process_unavailable";
    }

    private static void SleepUntilNextPoll(DateTimeOffset deadline, TimeSpan pollInterval)
    {
        var remaining = deadline - DateTimeOffset.Now;
        if (remaining <= TimeSpan.Zero)
        {
            return;
        }

        Thread.Sleep(remaining < pollInterval ? remaining : pollInterval);
    }

    private static string FormatTimeout(TimeSpan timeout)
    {
        if (timeout.TotalMinutes >= 1)
        {
            return $"{timeout.TotalMinutes:0.#}m";
        }

        return $"{timeout.TotalSeconds:0}s";
    }
}
