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

    [Fact]
    public void ForeignFaDemandBaseline_CoversAiOfferTermsAndTeamFinancialsWithoutFinalSalaryFloor()
    {
        var repoRoot = RepoRoot();
        var offsetsText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "bootstrap", "abi", "ootp_offsets.h"));
        var rvasText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "bootstrap", "abi", "ootp_rvas.generated.h"));
        var prepareText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "foreign", "signability", "submit_offer_probe", "baseline", "submit_offer_probe_foreign_fa_baseline_prepare.c"));
        var submitProbeHeaderText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "foreign", "signability", "submit_offer_probe", "submit_offer_probe.h"));
        var offerProbeText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "foreign", "signability", "foreign_policy", "wrappers", "offer_attach", "foreign_signability_foreign_ai_offer_attach_probe.c"));
        var hookStubsText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "hook_stubs", "foreign", "ai_status", "hook_stubs_foreign_ai_status.c"));
        var patchInstallerText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "patch_installers", "foreign", "ai_fa", "patch_installers_foreign_ai_fa_status.c"));
        var demandRemapText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "foreign", "signability", "submit_offer_probe", "demand", "submit_offer_probe_foreign_fa_demand_remap.c"));
        var teamAddText = File.ReadAllText(Path.Combine(repoRoot, "native", "src", "team", "add_player_guard", "team_add_player_guard.c"));

        offsetsText.Should().Contain("#define OOTP27_KBO_LEAGUE_FINANCIALS_REDIRECT_LEAGUE_ID_OFFSET 0x44e8u");
        rvasText.Should().Contain("#define OOTP27_AI_FA_OFFER_TERMS_BUILD_FUNC_RVA 0x00857ED0u");
        rvasText.Should().Contain("#define OOTP27_AI_FA_OFFER_TERMS_BUILD_PREP_RVA 0x00AAA2F0u");
        prepareText.Should().Contain("kbo_prepare_foreign_fa_offer_demand_baseline_for_team_key");
        prepareText.Should().Contain("kbo_resolve_team_key_league_financials");
        prepareText.Should().Contain("foreign_ai_offer_terms");
        prepareText.Should().Contain("foreign_ai_offer_build");
        prepareText.Should().Contain("foreign_ai_offer_attach");
        submitProbeHeaderText.Should().Contain("kbo_resolve_team_key_league_financials");
        offerProbeText.Should().Contain("ootp_kbo_foreign_ai_offer_terms_build_probe_wrapper");
        offerProbeText.Should().Contain("kbo_prepare_foreign_fa_offer_demand_baseline_for_team_key");
        hookStubsText.Should().Contain("build_kbo_foreign_ai_offer_terms_build_probe_stub");
        patchInstallerText.Should().Contain("kbo_install_foreign_ai_offer_terms_build_probe_patch");
        demandRemapText.Should().Contain("demand_floor <= 0 || holder_team_id == 0u");
        demandRemapText.Should().NotContain("kbo_apply_foreign_contract_salary_floor");
        teamAddText.Should().NotContain("team_add_post_original");
        teamAddText.Should().NotContain("kbo_apply_foreign_contract_salary_floor");
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
