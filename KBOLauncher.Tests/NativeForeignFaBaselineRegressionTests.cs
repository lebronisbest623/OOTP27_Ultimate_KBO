namespace KBOLauncher.Tests;

using FluentAssertions;
using Xunit;

public sealed class NativeForeignFaBaselineRegressionTests
{
    [Fact]
    public void ForeignFaDemandBaseline_PatchesAndRestoresDemandCeiling()
    {
        var repoRoot = RepoRoot();
        var offsetsText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "bootstrap", "abi", "ootp_offsets.h"));
        var snapshotText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "foreign", "signability", "submit_offer_probe", "submit_offer_probe.h"));
        var prepareText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "foreign", "signability", "submit_offer_probe", "baseline", "submit_offer_probe_foreign_fa_baseline_prepare.c"));
        var restoreText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "foreign", "signability", "submit_offer_probe", "demand", "submit_offer_probe_foreign_fa_demand_restore.c"));

        offsetsText.Should().Contain("#define OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET 0x374u");
        snapshotText.Should().Contain("patched_demand_ceiling_value");
        prepareText.Should().Contain("OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET");
        prepareText.Should().Contain("patched != 10");
        prepareText.Should().Contain("foreign_ceiling=%d");
        restoreText.Should().Contain("OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET");
        restoreText.Should().Contain("restore_complete = restored == 10");
        restoreText.Should().Contain("patched_ceiling=%d");
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
