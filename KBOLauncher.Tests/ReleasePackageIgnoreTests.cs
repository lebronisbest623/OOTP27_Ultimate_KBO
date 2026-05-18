namespace KBOLauncher.Tests;

using System.Text.Json;
using FluentAssertions;
using Xunit;

public sealed class ReleasePackageIgnoreTests
{
    [Theory]
    [InlineData("dist/")]
    [InlineData("release_artifacts/")]
    [InlineData("native/bin/")]
    [InlineData("*.dll")]
    [InlineData("*.exe")]
    [InlineData("*.pdb")]
    [InlineData("TestResults/")]
    public void GitIgnore_ExcludesReleaseNativeAndTestOutputs(string requiredPattern)
    {
        var gitignore = File.ReadAllLines(Path.Combine(FindRepoRoot(), ".gitignore"));

        gitignore.Should().Contain(requiredPattern);
    }

    [Theory]
    [InlineData("\"KBOLauncher.exe\"")]
    [InlineData("\"KBOFix.dll\"")]
    [InlineData("\"WebView2Loader.dll\"")]
    [InlineData("\"kbo_league_id.txt\"")]
    [InlineData("\"assets\\fonts\\JejuGothic-Regular.ttf\"")]
    [InlineData("\"assets\\fonts\\JejuGothic-OFL.txt\"")]
    [InlineData("\"assets\\icons\\github-mark.png\"")]
    [InlineData("\"tools\\kbo_optimizer.exe\"")]
    [InlineData("\"tools\\kbo_optimizer.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\__init__.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\amateur_assignment.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\amateur_batch.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\amateur_common.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\amateur_metrics.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\amateur_role_capacities.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\amateur_roles.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\amateur_targets.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\asian_games_roster.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\cli.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\constants.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\csv_io.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\fa_compensation.py\"")]
    [InlineData("\"tools\\kbo_optimizer_lib\\military_selection.py\"")]
    public void ReleaseScript_ValidatesRequiredNonSeedPayloadFiles(string requiredFileLiteral)
    {
        var script = File.ReadAllText(Path.Combine(FindRepoRoot(), "scripts", "release.ps1"));

        script.Should().Contain(requiredFileLiteral);
    }

    [Fact]
    public void ReleaseValidation_UsesSeedManifest()
    {
        var root = FindRepoRoot();
        var releaseScript = File.ReadAllText(Path.Combine(root, "scripts", "release.ps1"));
        var validationScript = File.ReadAllText(Path.Combine(root, "tests", "release", "verify-release-artifact.ps1"));

        releaseScript.Should().Contain("seed_manifest.json");
        validationScript.Should().Contain("seed_manifest.json");
        releaseScript.Should().Contain("Get-SeedManifestPayloadFiles");
        validationScript.Should().Contain("Get-SeedManifestPayloadFiles");
    }

    [Fact]
    public void SeedManifest_ListsEveryJsonAndCsvSourceFile()
    {
        var root = FindRepoRoot();
        var seedRoot = Path.Combine(root, "data", "seeds");
        var manifestPath = Path.Combine(seedRoot, "seed_manifest.json");

        using var document = JsonDocument.Parse(File.ReadAllText(manifestPath));
        var manifestFiles = document.RootElement
            .GetProperty("groups")
            .EnumerateArray()
            .SelectMany(group => group.GetProperty("files").EnumerateArray())
            .ToArray();
        var listedSources = manifestFiles
            .Select(file => GetManifestRelativePath(file, "source", fallbackPropertyName: "path"))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        var listedTargets = manifestFiles
            .Select(file => GetManifestRelativePath(file, "path"))
            .ToArray();

        var actual = Directory.EnumerateFiles(seedRoot, "*", SearchOption.AllDirectories)
            .Where(path => path.EndsWith(".json", StringComparison.OrdinalIgnoreCase)
                || path.EndsWith(".csv", StringComparison.OrdinalIgnoreCase))
            .Select(path => Path.GetRelativePath(seedRoot, path))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        actual.Except(listedSources, StringComparer.OrdinalIgnoreCase).Should().BeEmpty();
        listedSources.Except(actual, StringComparer.OrdinalIgnoreCase).Should().BeEmpty();
        listedTargets.Distinct(StringComparer.OrdinalIgnoreCase).Count().Should().Be(listedTargets.Length);
    }

    private static string GetManifestRelativePath(
        JsonElement file,
        string propertyName,
        string? fallbackPropertyName = null)
    {
        if (!file.TryGetProperty(propertyName, out var property)
            || property.ValueKind is JsonValueKind.Null
            || string.IsNullOrWhiteSpace(property.GetString()))
        {
            if (fallbackPropertyName is null)
            {
                throw new InvalidOperationException($"Missing manifest file property: {propertyName}");
            }

            property = file.GetProperty(fallbackPropertyName);
        }

        return property.GetString()!.Replace('/', Path.DirectorySeparatorChar);
    }

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "OOTP27-KBO-Launcher.sln")))
            {
                return dir.FullName;
            }

            dir = dir.Parent;
        }

        throw new InvalidOperationException("Could not find repository root.");
    }
}
