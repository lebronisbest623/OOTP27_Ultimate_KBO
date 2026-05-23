namespace KBOLauncher.Tests;

using FluentAssertions;
using System.Text.Json;
using Xunit;

public sealed class KboSeedFilesTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-seed-file-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void EnsureKboLeagueIdConfig_CopiesFirstExistingCandidate()
    {
        var localDir = Path.Combine(tempDir, "local");
        var missing = Path.Combine(tempDir, "missing", "kbo_league_id.txt");
        var candidate = Path.Combine(tempDir, "candidate", "kbo_league_id.txt");
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        File.WriteAllText(candidate, "100");

        global::KboSeedFiles.EnsureKboLeagueIdConfig(localDir, [missing, candidate]);

        ReadBundleFile(localDir, "kbo_league_id.txt").Should().Be("100");
    }

    [Fact]
    public void EnsureKboLeagueIdConfig_DoesNotRewriteMatchingTrimmedValue()
    {
        var localDir = Path.Combine(tempDir, "local");
        var candidate = Path.Combine(tempDir, "candidate", "kbo_league_id.txt");
        var localPath = Path.Combine(localDir, "config", "kbo_league_id.txt");
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        Directory.CreateDirectory(Path.GetDirectoryName(localPath)!);
        File.WriteAllText(candidate, "100\r\n");
        File.WriteAllText(localPath, "100");
        var before = File.GetLastWriteTimeUtc(localPath);
        File.SetLastWriteTimeUtc(localPath, before.AddMinutes(-5));
        before = File.GetLastWriteTimeUtc(localPath);

        global::KboSeedFiles.EnsureKboLeagueIdConfig(localDir, [candidate]);

        File.ReadAllText(localPath).Should().Be("100");
        File.GetLastWriteTimeUtc(localPath).Should().Be(before);
    }

    [Fact]
    public void EnsureBundledKboDataFile_CopiesWhenMissingAndWhenCandidateIsNewer()
    {
        var localDir = Path.Combine(tempDir, "local");
        var candidate = Path.Combine(tempDir, "candidate", "allstar_teams.csv");
        var localPath = Path.Combine(localDir, "config", "allstar_teams.csv");
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        Directory.CreateDirectory(Path.GetDirectoryName(localPath)!);
        File.WriteAllText(candidate, "team_id,name\n1,A");

        global::KboSeedFiles.EnsureBundledKboDataFile(localDir, "allstar_teams.csv", "All-Star seed", [candidate]);

        File.ReadAllText(localPath).Should().Be("team_id,name\n1,A");

        File.WriteAllText(candidate, "team_id,name\n1,A\n2,B");
        File.SetLastWriteTimeUtc(candidate, File.GetLastWriteTimeUtc(localPath).AddMinutes(1));

        global::KboSeedFiles.EnsureBundledKboDataFile(localDir, "allstar_teams.csv", "All-Star seed", [candidate]);

        File.ReadAllText(localPath).Should().Be("team_id,name\n1,A\n2,B");
    }

    [Fact]
    public void EnsureBundledKboDataFile_DoesNotRewriteUnchangedLocalFile()
    {
        var localDir = Path.Combine(tempDir, "local");
        var candidate = Path.Combine(tempDir, "candidate", "fa_rules.json");
        var localPath = Path.Combine(localDir, "config", "fa_rules.json");
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        Directory.CreateDirectory(Path.GetDirectoryName(localPath)!);
        File.WriteAllText(candidate, "{}");
        File.WriteAllText(localPath, "{}");
        var stamp = new DateTime(2026, 5, 1, 0, 0, 0, DateTimeKind.Utc);
        File.SetLastWriteTimeUtc(candidate, stamp);
        File.SetLastWriteTimeUtc(localPath, stamp.AddMinutes(1));
        var before = File.GetLastWriteTimeUtc(localPath);

        global::KboSeedFiles.EnsureBundledKboDataFile(localDir, "fa_rules.json", "FA rules", [candidate]);

        File.ReadAllText(localPath).Should().Be("{}");
        File.GetLastWriteTimeUtc(localPath).Should().Be(before);
    }

    [Fact]
    public void EnsureBundledKboDataFile_CopiesSameLengthContentChangeEvenWhenLocalIsNewer()
    {
        var localDir = Path.Combine(tempDir, "local");
        var candidate = Path.Combine(tempDir, "candidate", "kbo_nations.json");
        var localPath = Path.Combine(localDir, "config", "kbo_nations.json");
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        Directory.CreateDirectory(Path.GetDirectoryName(localPath)!);
        File.WriteAllText(candidate, """{"nations":[{"id":177}]}""");
        File.WriteAllText(localPath, """{"nations":[{"id":176}]}""");
        File.SetLastWriteTimeUtc(candidate, new DateTime(2026, 5, 1, 0, 0, 0, DateTimeKind.Utc));
        File.SetLastWriteTimeUtc(localPath, new DateTime(2026, 5, 2, 0, 0, 0, DateTimeKind.Utc));

        global::KboSeedFiles.EnsureBundledKboDataFile(localDir, "kbo_nations.json", "Nation display table", [candidate]);

        File.ReadAllText(localPath).Should().Be("""{"nations":[{"id":177}]}""");
    }

    [Fact]
    public void EnsureBundledKboDataFile_CopiesNestedRelativePath()
    {
        var localDir = Path.Combine(tempDir, "local");
        var candidate = Path.Combine(tempDir, "candidate", "news_templates", "ko", "captain.json");
        var localPath = Path.Combine(localDir, "config", "news_templates", "ko", "captain.json");
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        File.WriteAllText(candidate, "{\"captain.summary.title\":\"A\"}");

        global::KboSeedFiles.EnsureBundledKboDataFile(
            localDir,
            Path.Combine("news_templates", "ko", "captain.json"),
            "Captain news template",
            [candidate]);

        File.ReadAllText(localPath).Should().Be("{\"captain.summary.title\":\"A\"}");
    }

    [Fact]
    public void EnsureBundledKboDataManifest_CopiesGroupedSeedsAndRetiresOldSeed()
    {
        var localDir = Path.Combine(tempDir, "local");
        var dataRoot = Path.Combine(tempDir, "data", "seeds");
        var manifestPath = Path.Combine(dataRoot, "seed_manifest.json");
        var captainSeed = Path.Combine(dataRoot, "captain", "captain_seed.csv");
        var uiText = Path.Combine(dataRoot, "ui_text", "en", "hotkey_window.json");
        Directory.CreateDirectory(Path.GetDirectoryName(uiText)!);
        Directory.CreateDirectory(Path.GetDirectoryName(captainSeed)!);
        File.WriteAllText(captainSeed, "team_code,player_name\nLG,Park");
        File.WriteAllText(uiText, "{}");
        File.WriteAllText(manifestPath, """
        {
          "version": 1,
          "groups": [
            {
              "id": "captain",
              "label": "Captain",
              "files": [
                { "path": "captain_seed.csv", "source": "captain/captain_seed.csv", "label": "Captain seed", "kind": "seed" },
                { "path": "ui_text/en/hotkey_window.json", "label": "UI text", "kind": "ui_text" }
              ]
            }
          ],
          "retiredFiles": [
            {
              "path": "foreign_replacement_players_seed.csv",
              "label": "Foreign replacement player seed",
              "safeRemoveAllLinesStartWithAny": [ "verhadr01,", "olougja01," ]
            }
          ]
        }
        """);

        Directory.CreateDirectory(localDir);
        File.WriteAllText(Path.Combine(localDir, "foreign_replacement_players_seed.csv"), """
        verhadr01,regular,Drew VerHagen
        olougja01,regular,Jack O'Loughlin
        """);

        global::KboSeedFiles.EnsureBundledKboDataManifest(localDir, manifestPath, dataRoot);

        ReadBundleFile(localDir, "captain_seed.csv").Should().Be("team_code,player_name\nLG,Park");
        ReadBundleFile(localDir, "ui_text/en/hotkey_window.json").Should().Be("{}");
        File.Exists(Path.Combine(localDir, "config", "foreign_replacement_players_seed.csv")).Should().BeFalse();
    }

    [Fact]
    public void EnsureBundledKboDataDirectory_CopiesNestedFilesWhenMissingOrChanged()
    {
        var localDir = Path.Combine(tempDir, "local");
        var candidate = Path.Combine(tempDir, "candidate", "news_templates");
        var sourcePath = Path.Combine(candidate, "ko", "captain.json");
        var localPath = Path.Combine(localDir, "config", "news_templates", "ko", "captain.json");
        Directory.CreateDirectory(Path.GetDirectoryName(sourcePath)!);
        File.WriteAllText(sourcePath, "{\"captain.summary.title\":\"A\"}");

        global::KboSeedFiles.EnsureBundledKboDataDirectory(localDir, "news_templates", "News templates", [candidate]);

        File.ReadAllText(localPath).Should().Be("{\"captain.summary.title\":\"A\"}");

        File.WriteAllText(sourcePath, "{\"captain.summary.title\":\"B\"}");
        File.SetLastWriteTimeUtc(sourcePath, File.GetLastWriteTimeUtc(localPath).AddMinutes(1));

        global::KboSeedFiles.EnsureBundledKboDataDirectory(localDir, "news_templates", "News templates", [candidate]);

        File.ReadAllText(localPath).Should().Be("{\"captain.summary.title\":\"B\"}");
    }

    [Fact]
    public void EnsureKboScheduleAllstarGameLine_AddsMissingTypeFourGame()
    {
        Directory.CreateDirectory(tempDir);
        var path = Path.Combine(tempDir, "korean_baseball_organization_int_c_2026.lsdl");
        File.WriteAllText(path, """
        <SCHEDULE start_month="3" start_day="28" allstar_game_day="106">
        <GAMES>
        <GAME day="1" time="1400" away="1" home="2" />
        </GAMES>
        </SCHEDULE>
        """);

        var repaired = global::KboSeedFiles.EnsureKboScheduleAllstarGameLine(path);

        var text = File.ReadAllText(path);
        repaired.Should().BeTrue();
        text.Should().Contain("<Game day=\"106\" time=\"1830\" away=\"0\" home=\"0\" type=\"4\" />");
        (text.IndexOf("type=\"4\"", StringComparison.Ordinal) < text.IndexOf("day=\"1\"", StringComparison.Ordinal)).Should().BeTrue();
    }

    [Fact]
    public void EnsureKboScheduleAllstarGameLine_DoesNotDuplicateExistingTypeFourGame()
    {
        Directory.CreateDirectory(tempDir);
        var path = Path.Combine(tempDir, "korean_baseball_organization_int_c_2026.lsdl");
        File.WriteAllText(path, """
        <SCHEDULE start_month="3" start_day="28" allstar_game_day="106">
        <GAMES>
        <Game day="106" time="1830" away="0" home="0" type="4" />
        </GAMES>
        </SCHEDULE>
        """);

        var repaired = global::KboSeedFiles.EnsureKboScheduleAllstarGameLine(path);

        repaired.Should().BeFalse();
        File.ReadAllText(path).Split("type=\"4\"").Should().HaveCount(2);
    }

    [Fact]
    public void EnsureKboScheduleAllstarGameLine_RepairsTypeFourGameOutsideGamesBlock()
    {
        Directory.CreateDirectory(tempDir);
        var path = Path.Combine(tempDir, "korean_baseball_organization_int_c_2026.lsdl");
        File.WriteAllText(path, """
        <SCHEDULE start_month="3" start_day="28" allstar_game_day="106">
        <GAMES>
        <GAME day="1" time="1400" away="1" home="2" />
        </GAMES>
        <Game day="106" time="1830" away="0" home="0" type="4" />
        </SCHEDULE>
        """);

        var repaired = global::KboSeedFiles.EnsureKboScheduleAllstarGameLine(path);

        var text = File.ReadAllText(path);
        repaired.Should().BeTrue();
        (text.IndexOf("type=\"4\"", StringComparison.Ordinal) < text.IndexOf("day=\"1\"", StringComparison.Ordinal)).Should().BeTrue();
        text.Split("type=\"4\"").Should().HaveCount(2);
    }

    [Fact]
    public void EnsureKboScheduleAllstarGameLine_SkipsZeroAllstarDay()
    {
        Directory.CreateDirectory(tempDir);
        var path = Path.Combine(tempDir, "korean_baseball_organization_int_c_2026.lsdl");
        File.WriteAllText(path, """
        <SCHEDULE start_month="3" start_day="28" allstar_game_day="0">
        <GAMES>
        </GAMES>
        </SCHEDULE>
        """);

        var repaired = global::KboSeedFiles.EnsureKboScheduleAllstarGameLine(path);

        repaired.Should().BeFalse();
        File.ReadAllText(path).Should().NotContain("type=\"4\"");
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

    private static string ReadBundleFile(string localDir, string relativePath)
    {
        using var document = JsonDocument.Parse(File.ReadAllText(Path.Combine(localDir, "kbo_data_bundle.json")));
        return document.RootElement
            .GetProperty("Files")
            .GetProperty(relativePath)
            .GetString()!;
    }
}
