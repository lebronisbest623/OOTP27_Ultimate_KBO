namespace KBOLauncher.Tests;

using FluentAssertions;
using Xunit;

public sealed class DllPayloadStagerTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-dll-stage-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void PrepareInjectableDllCopy_StagesUniquelyNamedDllAndCompanionFiles()
    {
        var sourceDir = Path.Combine(tempDir, "source");
        var runDir = Path.Combine(tempDir, "run_dlls");
        var logPath = Path.Combine(tempDir, "launcher.log");
        var dllPath = Path.Combine(sourceDir, "KBOFix.dll");
        Directory.CreateDirectory(Path.Combine(sourceDir, "assets", "fonts"));
        Directory.CreateDirectory(Path.Combine(sourceDir, "tools"));
        Directory.CreateDirectory(Path.Combine(sourceDir, "tools", "kbo_optimizer_lib"));
        File.WriteAllBytes(dllPath, [1, 2, 3, 4]);
        File.WriteAllBytes(Path.Combine(sourceDir, "WebView2Loader.dll"), [5, 6, 7]);
        File.WriteAllText(Path.Combine(sourceDir, "assets", "fonts", "JejuGothic-Regular.ttf"), "font");
        File.WriteAllText(Path.Combine(sourceDir, "tools", "kbo_optimizer.py"), "tool");
        File.WriteAllText(Path.Combine(sourceDir, "tools", "kbo_optimizer_lib", "constants.py"), "constants");

        var staged = global::DllPayloadStager.PrepareInjectableDllCopy(dllPath, runDir, logPath);

        staged.Should().StartWithEquivalentOf(Path.Combine(runDir, "KBOFix-"));
        staged.Should().EndWithEquivalentOf(".dll");
        File.ReadAllBytes(staged).Should().Equal([1, 2, 3, 4]);
        File.ReadAllBytes(Path.Combine(runDir, "WebView2Loader.dll")).Should().Equal([5, 6, 7]);
        File.ReadAllText(Path.Combine(runDir, "assets", "fonts", "JejuGothic-Regular.ttf")).Should().Be("font");
        File.ReadAllText(Path.Combine(runDir, "tools", "kbo_optimizer.py")).Should().Be("tool");
        File.ReadAllText(Path.Combine(runDir, "tools", "kbo_optimizer_lib", "constants.py")).Should().Be("constants");
        File.ReadAllText(logPath).Should().Contain("dll_copy");
        File.ReadAllText(logPath).Should().Contain("webview2_loader_copy");
        File.ReadAllText(logPath).Should().Contain("assets_copy");
        File.ReadAllText(logPath).Should().Contain("tools_copy");
    }

    [Fact]
    public void PrepareInjectableDllCopy_StagesOptimizerToolsFromAncestorDirectory()
    {
        var repoRoot = Path.Combine(tempDir, "repo");
        var sourceDir = Path.Combine(repoRoot, "native", "bin");
        var runDir = Path.Combine(tempDir, "run_dlls");
        var logPath = Path.Combine(tempDir, "launcher.log");
        var dllPath = Path.Combine(sourceDir, "KBOFix.dll");
        Directory.CreateDirectory(sourceDir);
        Directory.CreateDirectory(Path.Combine(repoRoot, "native", "tools"));
        Directory.CreateDirectory(Path.Combine(repoRoot, "tools", "kbo_optimizer_lib"));
        File.WriteAllBytes(dllPath, [1]);
        File.WriteAllText(Path.Combine(repoRoot, "native", "tools", "research_helper.py"), "not-the-optimizer");
        File.WriteAllText(Path.Combine(repoRoot, "tools", "kbo_optimizer.py"), "ancestor-tool");
        File.WriteAllText(Path.Combine(repoRoot, "tools", "kbo_optimizer_lib", "__init__.py"), "package");

        _ = global::DllPayloadStager.PrepareInjectableDllCopy(dllPath, runDir, logPath);

        File.ReadAllText(Path.Combine(runDir, "tools", "kbo_optimizer.py")).Should().Be("ancestor-tool");
        File.ReadAllText(Path.Combine(runDir, "tools", "kbo_optimizer_lib", "__init__.py")).Should().Be("package");
        File.Exists(Path.Combine(runDir, "tools", "research_helper.py")).Should().BeFalse();
        File.ReadAllText(logPath).Should().Contain("tools_copy");
    }

    [Fact]
    public void PrepareInjectableDllCopy_CleansOldStagedDllsAndKeepsRecentOnes()
    {
        var sourceDir = Path.Combine(tempDir, "source");
        var runDir = Path.Combine(tempDir, "run_dlls");
        var logPath = Path.Combine(tempDir, "launcher.log");
        var dllPath = Path.Combine(sourceDir, "KBOFix.dll");
        var oldDll = Path.Combine(runDir, "KBOFix-old.dll");
        var recentDll = Path.Combine(runDir, "KBOFix-recent.dll");
        Directory.CreateDirectory(sourceDir);
        Directory.CreateDirectory(runDir);
        File.WriteAllBytes(dllPath, [1]);
        File.WriteAllBytes(oldDll, [2]);
        File.WriteAllBytes(recentDll, [3]);
        File.SetLastWriteTimeUtc(oldDll, DateTime.UtcNow.AddDays(-8));
        File.SetLastWriteTimeUtc(recentDll, DateTime.UtcNow);

        _ = global::DllPayloadStager.PrepareInjectableDllCopy(dllPath, runDir, logPath);

        File.Exists(oldDll).Should().BeFalse();
        File.Exists(recentDll).Should().BeTrue();
        File.ReadAllText(logPath).Should().Contain("staged_dll_cleanup");
    }

    [Fact]
    public void PrepareInjectableDllCopy_CreatesUniqueStagingPathForEachRun()
    {
        var sourceDir = Path.Combine(tempDir, "source");
        var runDir = Path.Combine(tempDir, "run_dlls");
        var logPath = Path.Combine(tempDir, "launcher.log");
        var dllPath = Path.Combine(sourceDir, "KBOFix.dll");
        Directory.CreateDirectory(sourceDir);
        File.WriteAllBytes(dllPath, [1]);

        var first = global::DllPayloadStager.PrepareInjectableDllCopy(dllPath, runDir, logPath);
        var second = global::DllPayloadStager.PrepareInjectableDllCopy(dllPath, runDir, logPath);

        first.Should().NotBe(second);
        File.Exists(first).Should().BeTrue();
        File.Exists(second).Should().BeTrue();
    }

    [Fact]
    public void PrepareInjectableDllCopy_ThrowsWhenSourceDllIsMissing()
    {
        var missingPath = Path.Combine(tempDir, "missing", "KBOFix.dll");

        var act = () => global::DllPayloadStager.PrepareInjectableDllCopy(
            missingPath,
            Path.Combine(tempDir, "run_dlls"),
            Path.Combine(tempDir, "launcher.log"));

        act.Should().Throw<FileNotFoundException>()
            .Which.FileName.Should().Be(Path.GetFullPath(missingPath));
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
