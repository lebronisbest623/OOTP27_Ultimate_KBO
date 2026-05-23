namespace KBOLauncher.Tests;

using System.Text.Json;
using FluentAssertions;
using Xunit;

public sealed class KboFlagsTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-launcher-tests", Guid.NewGuid().ToString("N"));

    private string ConfigPath => Path.Combine(tempDir, "kbo_flags.json");

    [Fact]
    public void NormalizeKboFlagKey_StripsDirectoryAndTxtExtension()
    {
        global::KboFlags.NormalizeKboFlagKey(@"C:\tmp\enable_foreign_waiver_ai.txt").Should().Be("enable_foreign_waiver_ai");
        global::KboFlags.NormalizeKboFlagKey("disable_foreign_injury_replacement").Should().Be("disable_foreign_injury_replacement");
    }

    [Fact]
    public void ReadKboFlagConfig_ReadsFlatJsonAndNormalizesTxtKeys()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection.txt": "yes",
          "enable_foreign_waiver_ai": true,
          "disable_foreign_injury_replacement": 0,
          "ignored": "maybe"
        }
        """);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);

        flags["enable_launcher_injection"].Should().BeTrue();
        flags["enable_foreign_waiver_ai"].Should().BeTrue();
        flags["disable_foreign_injury_replacement"].Should().BeFalse();
        flags.ContainsKey("enable_launcher_injection.txt").Should().BeFalse();
        flags.ContainsKey("ignored").Should().BeFalse();
    }

    [Fact]
    public void ReadKboFlagConfig_ReadsNestedFlagsObject()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "flags": {
            "enable_foreign_waiver_ai": "enabled",
            "enable_single_division_allstar_events.txt": false
          }
        }
        """);

        global::KboFlags.ReadKboFlag(ConfigPath, "enable_foreign_waiver_ai.txt").Should().BeTrue();
        global::KboFlags.ReadKboFlag(ConfigPath, "enable_single_division_allstar_events.txt").Should().BeFalse();
    }

    [Fact]
    public void WriteKboFlagValue_UpdatesOneJsonFileAndPreservesExistingFlags()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true,
          "intl_established_fa_multiplier": 10
        }
        """);

        global::KboFlags.WriteKboFlagValue(ConfigPath, "enable_foreign_waiver_ai.txt", true);
        global::KboFlags.WriteKboFlagValue(ConfigPath, "disable_foreign_injury_replacement.txt", false);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        flags["enable_launcher_injection"].Should().BeTrue();
        flags["enable_foreign_waiver_ai"].Should().BeTrue();
        flags["disable_foreign_injury_replacement"].Should().BeFalse();
        global::KboFlags.ReadKboIntSetting(ConfigPath, "intl_established_fa_multiplier", 1, 1, 20).Should().Be(10);
    }

    [Fact]
    public async Task WriteKboFlagValue_ConcurrentWritesKeepJsonValidAndPreserveKeys()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true
        }
        """);

        var tasks = Enumerable.Range(0, 32)
            .Select(index => Task.Run(() =>
                global::KboFlags.WriteKboFlagValue(ConfigPath, $"enable_concurrent_flag_{index}.txt", index % 2 == 0)))
            .ToArray();

        await Task.WhenAll(tasks);

        using var doc = JsonDocument.Parse(File.ReadAllText(ConfigPath));
        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        flags["enable_launcher_injection"].Should().BeTrue();
        for (var index = 0; index < 32; index++)
        {
            flags[$"enable_concurrent_flag_{index}"].Should().Be(index % 2 == 0);
        }
    }

    [Fact]
    public void WriteKboFlagValue_MalformedJsonRecoversToSafeValidConfig()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, "{ nope");

        global::KboFlags.WriteKboFlagValue(ConfigPath, "enable_foreign_waiver_ai.txt", true);

        using var doc = JsonDocument.Parse(File.ReadAllText(ConfigPath));
        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        flags["enable_foreign_waiver_ai"].Should().BeTrue();
        global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt").Should().BeTrue();
    }

    [Fact]
    public void WriteKboIntSetting_UpdatesOneJsonFileAndPreservesBooleanFlags()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true
        }
        """);

        global::KboFlags.WriteKboIntSetting(ConfigPath, "intl_established_fa_multiplier", 99, 1, 20);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        flags["enable_launcher_injection"].Should().BeTrue();
        global::KboFlags.ReadKboIntSetting(ConfigPath, "intl_established_fa_multiplier", 1, 1, 20).Should().Be(20);
    }

    [Fact]
    public void MigrateLegacyKboSettingsFromFlags_MovesSettingsOutOfFlagConfig()
    {
        Directory.CreateDirectory(tempDir);
        var settingsPath = Path.Combine(tempDir, "kbo_settings.json");
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true,
          "enable_kbo_profiler": true,
          "foreign_fa_demand_minimum_salary": 700000,
          "foreign_fa_demand_average_salary": 1050000,
          "independent_acquisition_foreign_seller_transfer_fee": 80000,
          "intl_established_fa_multiplier": 18
        }
        """);
        File.WriteAllText(settingsPath, """
        {
          "foreign_fa_demand_average_salary": 900000
        }
        """);

        global::KboFlags.MigrateLegacyKboSettingsFromFlags(ConfigPath, settingsPath);

        var flags = global::KboFlags.ReadKboRawConfig(ConfigPath);
        flags.Keys.Should().Contain(["enable_launcher_injection", "enable_kbo_profiler"]);
        flags.Keys.Should().NotContain([
            "foreign_fa_demand_minimum_salary",
            "foreign_fa_demand_average_salary",
            "independent_acquisition_foreign_seller_transfer_fee",
            "intl_established_fa_multiplier"]);

        using var settingsDoc = JsonDocument.Parse(File.ReadAllText(settingsPath));
        settingsDoc.RootElement.GetProperty("foreign_fa_demand_minimum_salary").GetInt32().Should().Be(700000);
        settingsDoc.RootElement.GetProperty("foreign_fa_demand_average_salary").GetInt32().Should().Be(900000);
        settingsDoc.RootElement.GetProperty("independent_acquisition_foreign_seller_transfer_fee").GetInt32().Should().Be(80000);
        settingsDoc.RootElement.GetProperty("intl_established_fa_multiplier").GetInt32().Should().Be(18);
    }

    [Theory]
    [InlineData(true)]
    [InlineData(false)]
    public void WriteKboSingleDivisionAllstarEventsFlag_UpdatesAllNativeGateFlags(bool enabled)
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true
        }
        """);

        global::KboFlags.WriteKboSingleDivisionAllstarEventsFlag(ConfigPath, enabled);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        flags["enable_launcher_injection"].Should().BeTrue();
        flags["enable_single_division_allstar_runtime_patches"].Should().Be(enabled);
        flags["enable_single_division_allstar_settings_patch"].Should().Be(enabled);
        flags["enable_single_division_allstar_voting_hook"].Should().Be(enabled);
        flags["enable_single_division_allstar_events"].Should().Be(enabled);
    }

    [Fact]
    public void EnsureDefaultKboRuntimeFlags_SeedsFreshInstallCoreFeatureFlags()
    {
        global::KboFlags.EnsureDefaultKboRuntimeFlags(ConfigPath);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        flags["enable_experimental_runtime_hooks"].Should().BeTrue();
        flags["enable_foreign_waiver_ai"].Should().BeTrue();
        flags["enable_foreign_waiver_background_scanner"].Should().BeTrue();
        flags["enable_launcher_injection"].Should().BeTrue();
        flags["enable_foreign_ai_roster_management"].Should().BeTrue();
        flags["enable_single_division_allstar_runtime_patches"].Should().BeFalse();
        flags["enable_single_division_allstar_settings_patch"].Should().BeFalse();
        flags["enable_single_division_allstar_voting_hook"].Should().BeFalse();
        flags["enable_single_division_allstar_events"].Should().BeFalse();
        flags["enable_kbo_foreign_trade_check_patch"].Should().BeTrue();
        flags["enable_kbo_ai_fa_fallback_patch"].Should().BeTrue();
        flags["enable_kbo_player_team_signability_patch"].Should().BeTrue();
        flags["enable_kbo_offer_eligibility_patch"].Should().BeTrue();
        flags["enable_kbo_callup_foreign_limit_patch"].Should().BeTrue();
        flags["enable_kbo_current_date_tick_watchpoint"].Should().BeTrue();
        flags["enable_kbo_season_phase_monitor"].Should().BeFalse();
        flags["disable_kbo_fa_salary_opening_day_snapshot"].Should().BeFalse();
        flags["disable_kbo_no_minor_contract_patch"].Should().BeFalse();
        flags.ContainsKey("disable_kbo_no_minor_contract_experimental_patch").Should().BeFalse();
    }

    [Fact]
    public void EnsureDefaultKboRuntimeFlags_PreservesOrdinaryOptOutsButRepairsPinnedFlags()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_kbo_foreign_trade_check_patch": false,
          "enable_experimental_runtime_hooks": false,
          "enable_foreign_ai_controller": false,
          "enable_kbo_current_date_tick_watchpoint": false
        }
        """);

        global::KboFlags.EnsureDefaultKboRuntimeFlags(ConfigPath);

        global::KboFlags.ReadKboFlag(ConfigPath, "enable_kbo_foreign_trade_check_patch.txt").Should().BeFalse();
        global::KboFlags.ReadKboFlag(ConfigPath, "enable_experimental_runtime_hooks.txt").Should().BeTrue();
        global::KboFlags.ReadKboFlag(ConfigPath, "enable_foreign_ai_controller.txt").Should().BeTrue();
        global::KboFlags.ReadKboFlag(ConfigPath, "enable_kbo_current_date_tick_watchpoint.txt").Should().BeTrue();
    }

    [Fact]
    public void ReadKboFlagConfig_MissingOrMalformedFileFailsClosed()
    {
        global::KboFlags.ReadKboFlagConfig(ConfigPath).Should().BeEmpty();
        global::KboFlags.ReadKboFlag(ConfigPath, "enable_launcher_injection.txt").Should().BeFalse();
        global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt").Should().BeTrue();

        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, "{ nope");

        global::KboFlags.ReadKboFlagConfig(ConfigPath).Should().BeEmpty();
        global::KboFlags.ReadKboFlag(ConfigPath, "enable_launcher_injection.txt").Should().BeFalse();
        global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt").Should().BeTrue();
    }

    [Fact]
    public void ReadKboFlagDefaultEnabled_EnableLauncherInjectionMissing_ReturnsTrue()
    {
        global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt").Should().BeTrue();
    }

    [Fact]
    public void ReadKboFlagDefaultEnabled_EnableLauncherInjectionMalformedConfig_ReturnsTrue()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, "{ nope");

        global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt").Should().BeTrue();
    }

    [Fact]
    public void ReadKboFlagDefaultEnabled_EnableLauncherInjectionExplicitTrue_ReturnsTrue()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true
        }
        """);

        global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt").Should().BeTrue();
    }

    [Fact]
    public void ReadKboFlagDefaultEnabled_OnlyDisablesWhenConfigExplicitlySaysFalse()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_foreign_waiver_ai": true,
          "enable_launcher_injection": false
        }
        """);

        global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_foreign_waiver_ai.txt").Should().BeTrue();
        global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "missing_flag.txt").Should().BeTrue();
        global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt").Should().BeFalse();
    }

    [Theory]
    [InlineData("10", 10)]
    [InlineData("\" 5 \"", 5)]
    [InlineData("0", 1)]
    [InlineData("99", 20)]
    public void ReadKboIntSetting_ReadsAndClampsSupportedValues(string jsonValue, int expected)
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, $$"""
        {
          "intl_established_fa_multiplier": {{jsonValue}}
        }
        """);

        global::KboFlags.ReadKboIntSetting(ConfigPath, "intl_established_fa_multiplier", 1, 1, 20).Should().Be(expected);
    }

    [Theory]
    [InlineData("\"nope\"")]
    [InlineData("true")]
    [InlineData("{}")]
    public void ReadKboIntSetting_IgnoresUnsupportedValues(string jsonValue)
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, $$"""
        {
          "intl_established_fa_multiplier": {{jsonValue}}
        }
        """);

        global::KboFlags.ReadKboIntSetting(ConfigPath, "intl_established_fa_multiplier", 7, 1, 20).Should().Be(7);
    }

    [Fact]
    public void ReadKboIntSetting_PrefersSettingsFileOverLegacyFlags()
    {
        Directory.CreateDirectory(tempDir);
        var settingsPath = Path.Combine(tempDir, "kbo_settings.json");
        File.WriteAllText(settingsPath, """
        {
          "intl_established_fa_multiplier": 8
        }
        """);
        File.WriteAllText(ConfigPath, """
        {
          "intl_established_fa_multiplier": 12
        }
        """);

        global::KboFlags.ReadKboIntSetting(
            settingsPath,
            ConfigPath,
            "intl_established_fa_multiplier",
            1,
            1,
            20).Should().Be(8);
    }

    [Fact]
    public void ReadKboIntSetting_FallsBackToLegacyFlagsWhenSettingsMissing()
    {
        Directory.CreateDirectory(tempDir);
        var settingsPath = Path.Combine(tempDir, "kbo_settings.json");
        File.WriteAllText(ConfigPath, """
        {
          "intl_established_fa_multiplier": 12
        }
        """);

        global::KboFlags.ReadKboIntSetting(
            settingsPath,
            ConfigPath,
            "intl_established_fa_multiplier",
            1,
            1,
            20).Should().Be(12);
    }

    [Theory]
    [InlineData("true", true)]
    [InlineData("false", false)]
    [InlineData("1", true)]
    [InlineData("0", false)]
    [InlineData("-2", true)]
    [InlineData("\"YES\"", true)]
    [InlineData("\"off\"", false)]
    [InlineData("\" enabled \"", true)]
    public void TryReadJsonBool_AcceptsSupportedBooleanForms(string json, bool expected)
    {
        using var doc = JsonDocument.Parse(json);

        global::KboFlags.TryReadJsonBool(doc.RootElement, out var actual).Should().BeTrue();
        actual.Should().Be(expected);
    }

    [Theory]
    [InlineData("{}")]
    [InlineData("[]")]
    [InlineData("\"maybe\"")]
    [InlineData("1.5")]
    public void TryReadJsonBool_RejectsUnsupportedValues(string json)
    {
        using var doc = JsonDocument.Parse(json);

        global::KboFlags.TryReadJsonBool(doc.RootElement, out _).Should().BeFalse();
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
