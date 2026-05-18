namespace KBOLauncher.Tests;

using System.Text.Json;
using System.Text.RegularExpressions;
using FluentAssertions;
using Xunit;

public sealed class OotpSupportedBuildManifestTests
{
    [Fact]
    public void SupportedBuildManifest_MatchesGeneratedManagedAndNativeLists()
    {
        var manifestBuilds = ReadManifestBuilds();
        var managedBuilds = global::OotpSupportedBuilds.All
            .Select(build => new SupportedBuildRow(
                build.Timestamp,
                build.SizeOfImage,
                build.Label,
                build.ExperimentalSignature,
                build.NativePatchesSupported))
            .ToArray();
        var nativeBuilds = ReadNativeGeneratedBuilds();

        managedBuilds.Should().Equal(manifestBuilds);
        nativeBuilds.Should().Equal(manifestBuilds.Select(build => build.NativeRow).ToArray());
    }

    [Fact]
    public void SupportedBuildManifest_HasNoAmbiguousTimestampSizePairs()
    {
        var manifestBuilds = ReadManifestBuilds();

        var duplicate = manifestBuilds
            .GroupBy(build => new { build.Timestamp, build.SizeOfImage })
            .FirstOrDefault(group => group.Count() > 1);

        duplicate.Should().BeNull();
    }

    [Fact]
    public void NativeBuildSpecificRvaResolver_CoversEveryOfficialExperimentalBuildForCriticalRvas()
    {
        var officialExperimentalBuilds = ReadManifestBuilds()
            .Where(build => build.ExperimentalSignature && build.Label.Contains("Official", StringComparison.OrdinalIgnoreCase))
            .ToArray();
        var resolverText = File.ReadAllText(RepoPath("native", "src", "build_verify", "build_verify.c"));
        var criticalRvas = new[]
        {
            "OOTP27_CREATE_LEAGUE_EVENT_RVA",
            "OOTP27_PISD_STRING_ASSIGN_RVA",
            "OOTP27_LEAGUE_NEWS_REAL_ADD_RVA",
            "OOTP27_NEWS_OBJECT_CTOR_RVA",
            "OOTP27_NEWS_STRING_ENSURE_RVA",
            "OOTP27_CREATE_MESSAGE_CORE_RVA",
            "OOTP27_UI_OPERATOR_NEW_RVA",
            "OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA",
        };

        foreach (var build in officialExperimentalBuilds)
        {
            var token = ToNativeBuildMacroToken(build.Label);
            resolverText.Should().Contain($"KBO_SUPPORTED_OOTP_BUILD_{token}_TIMESTAMP");
            resolverText.Should().Contain($"KBO_SUPPORTED_OOTP_BUILD_{token}_SIZE_OF_IMAGE");
        }

        foreach (var rva in criticalRvas)
        {
            Regex.Matches(resolverText, $"case {rva}:").Count.Should().Be(officialExperimentalBuilds.Length);
        }
    }

    [Fact]
    public void OfficialBuilds_OnlyEnableNativePatchesWhenDirectFunctionRvasAreMapped()
    {
        var officialBuilds = ReadManifestBuilds()
            .Where(build => build.ExperimentalSignature && build.Label.Contains("Official", StringComparison.OrdinalIgnoreCase))
            .ToArray();
        var resolverText = File.ReadAllText(RepoPath("native", "src", "build_verify", "build_verify.c"));
        var requiredDirectFunctionRvas = new[]
        {
            "OOTP27_ALLSTAR_TEAM_SETUP_FUNC_RVA",
            "OOTP27_ALLSTAR_CANDIDATE_REBUILD_FUNC_RVA",
            "OOTP27_MAKE_ALLSTAR_GAME_EVENTS_RVA",
        };

        if (officialBuilds.Length == 0)
        {
            resolverText.Should().NotContain("KBO_SUPPORTED_OOTP_BUILD_OFFICIAL");
            return;
        }

        foreach (var build in officialBuilds.Where(build => build.NativePatchesSupported))
        {
            var token = ToNativeBuildMacroToken(build.Label);
            resolverText.Should().Contain($"KBO_SUPPORTED_OOTP_BUILD_{token}_TIMESTAMP");
            foreach (var rva in requiredDirectFunctionRvas)
            {
                resolverText.Should().Contain($"case {rva}:");
            }
        }
    }

    private static SupportedBuildRow[] ReadManifestBuilds()
    {
        using var doc = JsonDocument.Parse(File.ReadAllText(RepoPath("config", "ootp-supported-builds.json")));
        return doc.RootElement.GetProperty("builds")
            .EnumerateArray()
            .Select(build => new SupportedBuildRow(
                ParseHexUInt32(build.GetProperty("timestamp").GetString()!),
                ParseHexUInt32(build.GetProperty("sizeOfImage").GetString()!),
                build.GetProperty("label").GetString()!,
                build.TryGetProperty("experimentalSignature", out var experimentalSignature) && experimentalSignature.GetBoolean(),
                build.TryGetProperty("nativePatchesSupported", out var nativePatchesSupported) && nativePatchesSupported.GetBoolean()))
            .ToArray();
    }

    private static NativeBuildRow[] ReadNativeGeneratedBuilds()
    {
        var text = File.ReadAllText(RepoPath("native", "src", "build_verify", "supported_builds.generated.c"));
        return Regex.Matches(
                text,
                "\\{0x(?<timestamp>[0-9A-Fa-f]{8})u, 0x(?<size>[0-9A-Fa-f]{8})u, \"(?<label>[^\"]+)\", (?<native>[01])\\},")
            .Select(match => new NativeBuildRow(
                Convert.ToUInt32(match.Groups["timestamp"].Value, 16),
                Convert.ToUInt32(match.Groups["size"].Value, 16),
                match.Groups["label"].Value,
                match.Groups["native"].Value == "1"))
            .ToArray();
    }

    private static uint ParseHexUInt32(string value)
    {
        value.Should().MatchRegex("^0x[0-9A-Fa-f]{1,8}$");
        return Convert.ToUInt32(value[2..], 16);
    }

    private static string RepoPath(params string[] parts)
    {
        var dir = AppContext.BaseDirectory;
        while (dir is not null)
        {
            var candidate = Path.Combine(new[] { dir }.Concat(parts).ToArray());
            if (File.Exists(candidate))
            {
                return candidate;
            }
            dir = Directory.GetParent(dir)?.FullName;
        }

        throw new FileNotFoundException("Could not find repository file.", Path.Combine(parts));
    }

    private static string ToNativeBuildMacroToken(string label)
    {
        var version = Regex.Match(label, @"\d+\.\d+\.\d+");
        if (label.Contains("Official", StringComparison.OrdinalIgnoreCase) && version.Success)
        {
            return "OFFICIAL_" + version.Value.Replace('.', '_');
        }

        if (label.Contains("Steam", StringComparison.OrdinalIgnoreCase))
        {
            return "STEAM_" + Regex.Replace(label, @"[^0-9]+", "_").Trim('_');
        }

        return Regex.Replace(label.ToUpperInvariant(), "[^A-Z0-9]+", "_").Trim('_');
    }

    private sealed record SupportedBuildRow(
        uint Timestamp,
        uint SizeOfImage,
        string Label,
        bool ExperimentalSignature,
        bool NativePatchesSupported)
    {
        public NativeBuildRow NativeRow => new(Timestamp, SizeOfImage, Label, NativePatchesSupported);
    }

    private sealed record NativeBuildRow(uint Timestamp, uint SizeOfImage, string Label, bool NativePatchesSupported);
}
