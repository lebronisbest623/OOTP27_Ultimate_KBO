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

        managedBuilds.Should().Equal(manifestBuilds.Select(build => build.ManagedRow));
        nativeBuilds.Should().Equal(manifestBuilds.Select(build => build.NativeRow));
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
    public void BuildRvaManifest_MatchesGeneratedNativeRvaTableAndCanonicalHeader()
    {
        var nativePatchBuilds = ReadManifestBuilds()
            .Where(build => build.NativePatchesSupported)
            .ToArray();
        var manifestRvas = ReadRvaManifestRows();
        var expectedNativeRows = nativePatchBuilds
            .SelectMany(build => manifestRvas.Select(rva => new NativeRvaRow(
                build.Timestamp,
                build.SizeOfImage,
                rva.CanonicalRva,
                rva.BuildRvas[build.Id],
                rva.Name)))
            .ToArray();
        var expectedCanonicalRvas = manifestRvas.ToDictionary(row => row.Name, row => row.CanonicalRva);

        ReadNativeGeneratedRvaRows().Should().Equal(expectedNativeRows);
        ReadGeneratedCanonicalRvas().Should().Equal(expectedCanonicalRvas);
    }

    [Fact]
    public void ManuallyVerifiedRvas_MatchExecutableAnchors()
    {
        var rvas = ReadRvaManifestRows().ToDictionary(row => row.Name);

        rvas["OOTP27_PLAYER_TOOLTIP_CURRENT_GLOBAL_RVA"].BuildRvas["steam_2026_06_09"].Should().Be(
            0x031F1740u,
            "the June tooltip render function loads the current-tooltip global from image base + 0x031F1740 before dereferencing it");
        rvas["OOTP27_PLAYER_TOOLTIP_CURRENT_GLOBAL_RVA"].BuildRvas["steam_2026_06_23"].Should().Be(
            0x031F2760u,
            "the June 23 tooltip render function loads the current-tooltip global from image base + 0x031F2760 before dereferencing it");
        rvas["OOTP27_INTL_ESTABLISHED_FA_PROGRESS_TITLE_RVA"].BuildRvas["steam_2026_06_09"].Should().Be(
            0x02AD5D10u,
            "the June executable stores the Creating International Free Agents title string at image base + 0x02AD5D10");
        rvas["OOTP27_ARBITRATION_AI_OFFER_WRITE_6827CD_SUPERSTAR_SOURCE_RVA"].BuildRvas["steam_2026_06_23"].Should().Be(
            0x02ABCBD8u,
            "the June 23 arbitration 6827cd patch resolves its superstar source from the RIP-relative LEA at image base + 0x00682C43");
        rvas["OOTP27_UI_CHECKBOX_SET_BOOL_RVA"].CanonicalRva.Should().Be(
            0x01E717D0u,
            "the May all-star settings hook calls the checkbox setter at image base + 0x01E717D0");
        rvas["OOTP27_UI_CHECKBOX_SET_BOOL_RVA"].BuildRvas["steam_2026_06_09"].Should().Be(
            0x01E789A0u,
            "the June all-star settings hook calls the same checkbox setter at image base + 0x01E789A0");
        rvas["OOTP27_UI_CHECKBOX_SET_BOOL_RVA"].BuildRvas["steam_2026_06_23"].Should().Be(
            0x01E79740u,
            "the June 23 all-star settings hook calls the same checkbox setter at image base + 0x01E79740");
    }

    [Fact]
    public void NativePatchSupportedBuilds_HaveCriticalRvasMapped()
    {
        var nativePatchBuilds = ReadManifestBuilds()
            .Where(build => build.NativePatchesSupported)
            .ToArray();
        var manifestRvas = ReadRvaManifestRows().ToDictionary(row => row.Name);
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
            "OOTP27_ALLSTAR_TEAM_SETUP_FUNC_RVA",
            "OOTP27_ALLSTAR_CANDIDATE_REBUILD_FUNC_RVA",
            "OOTP27_MAKE_ALLSTAR_GAME_EVENTS_RVA",
            "OOTP27_AMATEUR_GENERATION_TEAM_ADD_CALLER_00A30BA0_RVA",
            "OOTP27_ARBITRATION_NON_TENDER_RETURN_006820C6_RVA",
        };

        foreach (var rva in criticalRvas)
        {
            manifestRvas.Should().ContainKey(rva);
            foreach (var build in nativePatchBuilds)
            {
                manifestRvas[rva].BuildRvas.Should().ContainKey(build.Id);
            }
        }
    }

    private static SupportedBuildManifestRow[] ReadManifestBuilds()
    {
        using var doc = JsonDocument.Parse(File.ReadAllText(RepoPath("config", "ootp-supported-builds.json")));
        return doc.RootElement.GetProperty("builds")
            .EnumerateArray()
            .Select(build => new SupportedBuildManifestRow(
                build.GetProperty("id").GetString()!,
                ParseHexUInt32(build.GetProperty("timestamp").GetString()!),
                ParseHexUInt32(build.GetProperty("sizeOfImage").GetString()!),
                build.GetProperty("label").GetString()!,
                build.TryGetProperty("experimentalSignature", out var experimentalSignature) && experimentalSignature.GetBoolean(),
                build.TryGetProperty("nativePatchesSupported", out var nativePatchesSupported) && nativePatchesSupported.GetBoolean()))
            .ToArray();
    }

    private static RvaManifestRow[] ReadRvaManifestRows()
    {
        using var doc = JsonDocument.Parse(File.ReadAllText(RepoPath("config", "ootp-build-rvas.json")));
        doc.RootElement.GetProperty("canonicalBuildId").GetString().Should().NotBeNullOrWhiteSpace();
        return doc.RootElement.GetProperty("rvas")
            .EnumerateArray()
            .Select(row => new RvaManifestRow(
                row.GetProperty("name").GetString()!,
                ParseHexUInt32(row.GetProperty("canonicalRva").GetString()!),
                row.GetProperty("builds")
                    .EnumerateObject()
                    .ToDictionary(build => build.Name, build => ParseHexUInt32(build.Value.GetString()!))))
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

    private static NativeRvaRow[] ReadNativeGeneratedRvaRows()
    {
        var text = File.ReadAllText(RepoPath("native", "src", "build_verify", "build_rvas.generated.c"));
        return Regex.Matches(
                text,
                "\\{0x(?<timestamp>[0-9A-Fa-f]{8})u, 0x(?<size>[0-9A-Fa-f]{8})u, 0x(?<canonical>[0-9A-Fa-f]{8})u, 0x(?<build>[0-9A-Fa-f]{8})u, \"(?<name>[^\"]+)\"\\},")
            .Select(match => new NativeRvaRow(
                Convert.ToUInt32(match.Groups["timestamp"].Value, 16),
                Convert.ToUInt32(match.Groups["size"].Value, 16),
                Convert.ToUInt32(match.Groups["canonical"].Value, 16),
                Convert.ToUInt32(match.Groups["build"].Value, 16),
                match.Groups["name"].Value))
            .ToArray();
    }

    private static Dictionary<string, uint> ReadGeneratedCanonicalRvas()
    {
        var text = File.ReadAllText(RepoPath("native", "src", "bootstrap", "abi", "ootp_rvas.generated.h"));
        return Regex.Matches(
                text,
                "#define (?<name>OOTP27_[A-Z0-9_]+_RVA) 0x(?<rva>[0-9A-Fa-f]{8})u")
            .ToDictionary(
                match => match.Groups["name"].Value,
                match => Convert.ToUInt32(match.Groups["rva"].Value, 16));
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

    private sealed record SupportedBuildManifestRow(
        string Id,
        uint Timestamp,
        uint SizeOfImage,
        string Label,
        bool ExperimentalSignature,
        bool NativePatchesSupported)
    {
        public SupportedBuildRow ManagedRow => new(Timestamp, SizeOfImage, Label, ExperimentalSignature, NativePatchesSupported);

        public NativeBuildRow NativeRow => new(Timestamp, SizeOfImage, Label, NativePatchesSupported);
    }

    private sealed record SupportedBuildRow(
        uint Timestamp,
        uint SizeOfImage,
        string Label,
        bool ExperimentalSignature,
        bool NativePatchesSupported);

    private sealed record NativeBuildRow(uint Timestamp, uint SizeOfImage, string Label, bool NativePatchesSupported);

    private sealed record RvaManifestRow(string Name, uint CanonicalRva, Dictionary<string, uint> BuildRvas);

    private sealed record NativeRvaRow(uint Timestamp, uint SizeOfImage, uint CanonicalRva, uint BuildRva, string Name);
}
