namespace KBOLauncher.Tests;

using System.IO.Compression;
using FluentAssertions;
using Xunit;

public sealed class DiagnosticBundleTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-launcher-diagnostics-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void Create_IncludesSelfCheckAndRecentLocalLogs()
    {
        var localDir = Path.Combine(tempDir, "local");
        var outputDir = Path.Combine(tempDir, "diagnostics");
        var saveDir = Path.Combine(tempDir, "New_Game.lg");
        Directory.CreateDirectory(localDir);
        Directory.CreateDirectory(saveDir);
        File.WriteAllText(Path.Combine(saveDir, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl);
        File.WriteAllText(Path.Combine(saveDir, "flag_save_completed.dat"), "Finished save_database, closing flag file now");
        File.WriteAllText(Path.Combine(localDir, "launcher.log"), "inject_complete pid=123 reason=test");
        File.WriteAllText(Path.Combine(localDir, "runtime.ndjson"), "WebView2 F2 rights UI ready");
        File.WriteAllText(Path.Combine(localDir, "rule_audit.ndjson"), "{\"level\":\"error\",\"message\":\"failed sample\"}");
        File.WriteAllText(Path.Combine(localDir, "kbo_flags.json"), "{}");
        File.WriteAllText(Path.Combine(localDir, "current_save_path_123.txt"), saveDir);
        File.WriteAllText(
            Path.Combine(localDir, "launcher_roster_marker_guard_status.txt"),
            "status=marked_save_completed\nkbo_injection=allowed\n");
        var runDllDir = Path.Combine(localDir, "run_dlls");
        Directory.CreateDirectory(runDllDir);
        File.WriteAllBytes(Path.Combine(runDllDir, "WebView2Loader.dll"), [1, 2, 3]);

        var options = new global::LauncherOptions(
            OotpPath: null,
            DllPath: null,
            AttachPid: null,
            AttachExisting: false,
            EnableForeignWaiverAi: null,
            EnableSingleDivisionAllstarEvents: null,
            DryRun: false,
            AllowSecondInstance: false,
            Diagnostics: true,
            ShowHelp: false,
            OotpArgs: []);

        var zipPath = global::DiagnosticBundle.Create(options, localDir, outputDir, "test");

        File.Exists(zipPath).Should().BeTrue();
        using var archive = ZipFile.OpenRead(zipPath);
        archive.Entries.Select(e => e.FullName).Should().Contain([
            "README_FIRST.txt",
            "ACTION_REQUIRED.txt",
            "diagnostics_summary.json",
            "TROUBLESHOOTING_KO.txt",
            "self_check.txt",
            "recent_problem_lines.txt",
            "local_data_inventory.txt",
            "ootp_user_data_report.txt",
            "support_request_template.txt",
            "privacy_notice.txt",
            "environment.txt",
            "logs/launcher.log",
            "logs/runtime.ndjson",
            "logs/rule_audit.ndjson",
            "config/kbo_flags.json",
            "status/launcher_roster_marker_guard_status.txt",
            "status/current_save_path_123.txt",
            "run_dlls/WebView2Loader.dll",
        ]);

        ReadEntry(archive, "self_check.txt").Should().Contain("F2 hub signal");
        ReadEntry(archive, "self_check.txt").Should().Contain("marked_save_completed");
        ReadEntry(archive, "current_save_paths.txt").Should().Contain("description_exists=1");
        ReadEntry(archive, "ACTION_REQUIRED.txt").Should().Contain("자가진단");
        ReadEntry(archive, "diagnostics_summary.json").Should().Contain("\"worst_status\"");
        ReadEntry(archive, "TROUBLESHOOTING_KO.txt").Should().Contain("문제 해결");
        ReadEntry(archive, "recent_problem_lines.txt").Should().Contain("rule_audit.ndjson");
        ReadEntry(archive, "local_data_inventory.txt").Should().Contain("launcher.log");
        ReadEntry(archive, "ootp_user_data_report.txt").Should().Contain("crashed_on_startup candidates");
        ReadEntry(archive, "support_request_template.txt").Should().Contain("worst_status=");
        ReadEntry(archive, "privacy_notice.txt").Should().Contain("does not include the save folder");
    }

    [Fact]
    public void Create_FlagsInvalidCurrentSaveCacheClearly()
    {
        var localDir = Path.Combine(tempDir, "invalid-local");
        var outputDir = Path.Combine(tempDir, "invalid-diagnostics");
        var saveDir = Path.Combine(tempDir, "Plain_Game.lg");
        Directory.CreateDirectory(localDir);
        Directory.CreateDirectory(saveDir);
        File.WriteAllText(Path.Combine(saveDir, "description.txt"), "plain ootp save");
        File.WriteAllText(Path.Combine(localDir, "launcher.log"), "inject_blocked reason=marker_missing");
        File.WriteAllText(Path.Combine(localDir, "current_save_path_456.txt"), saveDir);

        var options = new global::LauncherOptions(
            OotpPath: null,
            DllPath: null,
            AttachPid: null,
            AttachExisting: false,
            EnableForeignWaiverAi: null,
            EnableSingleDivisionAllstarEvents: null,
            DryRun: false,
            AllowSecondInstance: false,
            Diagnostics: true,
            ShowHelp: false,
            OotpArgs: []);

        var zipPath = global::DiagnosticBundle.Create(options, localDir, outputDir, "invalid-save-test");

        using var archive = ZipFile.OpenRead(zipPath);
        ReadEntry(archive, "self_check.txt").Should().Contain("FAIL Current save cache");
        ReadEntry(archive, "self_check.txt").Should().Contain("marker_missing");
        ReadEntry(archive, "ACTION_REQUIRED.txt").Should().Contain("현재 열린 세이브");
        ReadEntry(archive, "diagnostics_summary.json").Should().Contain("\"component\": \"Current save cache\"");
    }

    private static string ReadEntry(ZipArchive archive, string name)
    {
        var entry = archive.GetEntry(name);
        entry.Should().NotBeNull();
        using var reader = new StreamReader(entry!.Open());
        return reader.ReadToEnd();
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
}
