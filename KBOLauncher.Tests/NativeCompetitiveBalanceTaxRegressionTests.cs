namespace KBOLauncher.Tests;

using FluentAssertions;
using Xunit;

public sealed class NativeCompetitiveBalanceTaxRegressionTests
{
    [Fact]
    public void CbtAnnouncementProcess_DoesNotRunDraftSchemaProbe()
    {
        var source = File.ReadAllText(RepoPath(
            "native",
            "src",
            "competitive_balance_tax",
            "process",
            "cbt_process.c"));

        source.Should().NotContain(
            "kbo_cbt_draft_probe_schema(",
            "the CBT announcement path runs inside OOTP and must not enumerate sqlite_master as a production side effect");
    }

    private static string RepoPath(params string[] parts)
    {
        return Path.Combine(new[] { RepoRoot() }.Concat(parts).ToArray());
    }

    private static string RepoRoot()
    {
        var dir = AppContext.BaseDirectory;
        while (dir is not null)
        {
            if (File.Exists(Path.Combine(dir, "CONSTITUTION.md")))
            {
                return dir;
            }

            dir = Directory.GetParent(dir)?.FullName;
        }

        throw new FileNotFoundException("Could not find repository root.", "CONSTITUTION.md");
    }
}
