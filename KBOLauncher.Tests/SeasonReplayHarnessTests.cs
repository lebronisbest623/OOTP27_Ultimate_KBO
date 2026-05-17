namespace KBOLauncher.Tests;

using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;
using FluentAssertions;
using Xunit;

public sealed class SeasonReplayHarnessTests
{
    private const string CustomEventForeignPriorityKind = "custom_event.foreign_priority";
    private const string CaptainPreseasonSelectionKind = "captain.preseason_selection";
    private const int KoreaNationId = 177;

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };

    public static IEnumerable<object[]> CustomEventScenarios()
    {
        return LoadScenarios("custom_events");
    }

    public static IEnumerable<object[]> CaptainSelectionScenarios()
    {
        return LoadScenarios("captain");
    }

    [Theory]
    [MemberData(nameof(CustomEventScenarios))]
    public void ForeignPriorityCustomEventSchedule_ReplaysExpectedEvents(SeasonReplayScenario scenario)
    {
        scenario.Kind.Should().Be(CustomEventForeignPriorityKind);

        var root = FindRepoRoot();
        var catalogPath = Path.Combine(root, "data", "seeds", "custom_events", "custom_event_catalog.json");
        var catalog = ReadJson<CustomEventCatalog>(catalogPath);
        var foreignPolicy = ReadJson<ForeignPlayerPolicy>(
            Path.Combine(root, "data", "seeds", "foreign_players", "foreign_player_policy.json"));

        AssertCustomEventCatalogFlatKeys(catalogPath, catalog);
        var actual = ReplayForeignPriorityEvents(scenario, catalog, foreignPolicy);

        actual.Should().BeEquivalentTo(
            scenario.ExpectedEvents,
            options => options.WithStrictOrdering());
    }

    [Theory]
    [MemberData(nameof(CaptainSelectionScenarios))]
    public void CaptainPreseasonSelection_ReplaysExpectedCaptain(SeasonReplayScenario scenario)
    {
        scenario.Kind.Should().Be(CaptainPreseasonSelectionKind);

        var root = FindRepoRoot();
        var policy = ReadJson<CaptainSelectionPolicy>(
            Path.Combine(root, "data", "seeds", "captain", "captain_selection_policy.json"));

        var actual = ReplayCaptainSelection(scenario, policy);

        actual.Should().BeEquivalentTo(
            scenario.ExpectedCaptains,
            options => options.WithStrictOrdering());
    }

    private static IEnumerable<object[]> LoadScenarios(string folder)
    {
        var root = FindRepoRoot();
        var scenarioDir = Path.Combine(root, "tests", "scenarios", "season_replay", folder);
        foreach (var path in Directory.EnumerateFiles(scenarioDir, "*.json").OrderBy(path => path, StringComparer.OrdinalIgnoreCase))
        {
            yield return new object[] { ReadJson<SeasonReplayScenario>(path) };
        }
    }

    private static IReadOnlyList<ReplayExpectedEvent> ReplayForeignPriorityEvents(
        SeasonReplayScenario scenario,
        CustomEventCatalog catalog,
        ForeignPlayerPolicy foreignPolicy)
    {
        var anchor = ParseDate(scenario.Inputs.AnchorDate);
        var eventsByKind = catalog.Events.ToDictionary(e => e.Kind, StringComparer.OrdinalIgnoreCase);
        var schedule = catalog.Schedule.ForeignPriority;

        return
        [
            BuildExpectedEvent(eventsByKind, "foreign_priority_open", anchor),
            BuildExpectedEvent(eventsByKind, "foreign_priority_close", anchor.AddDays(foreignPolicy.WaiverWindowDays)),
            BuildExpectedEvent(eventsByKind, "fa_declaration", anchor.AddDays(schedule.FaDeclarationOffsetDays)),
            BuildExpectedEvent(eventsByKind, "intl_established_fa", anchor.AddDays(schedule.IntlEstablishedFaOffsetDays)),
            BuildExpectedEvent(eventsByKind, "military_selection", anchor.AddMonths(schedule.MilitarySelectionOffsetMonths)),
        ];
    }

    private static void AssertCustomEventCatalogFlatKeys(string catalogPath, CustomEventCatalog catalog)
    {
        using var document = JsonDocument.Parse(File.ReadAllText(catalogPath));
        var root = document.RootElement;

        root.GetProperty("schedule.foreign_priority.fa_declaration_offset_days")
            .GetInt32()
            .Should()
            .Be(catalog.Schedule.ForeignPriority.FaDeclarationOffsetDays);
        root.GetProperty("schedule.foreign_priority.intl_established_fa_offset_days")
            .GetInt32()
            .Should()
            .Be(catalog.Schedule.ForeignPriority.IntlEstablishedFaOffsetDays);
        root.GetProperty("schedule.foreign_priority.military_selection_offset_months")
            .GetInt32()
            .Should()
            .Be(catalog.Schedule.ForeignPriority.MilitarySelectionOffsetMonths);

        foreach (var eventDefinition in catalog.Events)
        {
            root.GetProperty($"event.{eventDefinition.Kind}.title_key")
                .GetString()
                .Should()
                .Be(eventDefinition.TitleKey);
        }
    }

    private static ReplayExpectedEvent BuildExpectedEvent(
        IReadOnlyDictionary<string, CustomEventCatalogEvent> eventsByKind,
        string kind,
        DateOnly date)
    {
        var definition = eventsByKind[kind];
        return new ReplayExpectedEvent
        {
            Kind = kind,
            Date = FormatDate(date),
            TitleKey = definition.TitleKey,
        };
    }

    private static IReadOnlyList<CaptainSelection> ReplayCaptainSelection(
        SeasonReplayScenario scenario,
        CaptainSelectionPolicy policy)
    {
        var teams = scenario.Teams.ToDictionary(team => team.TeamId);
        var selected = new Dictionary<int, CaptainSelection>();

        foreach (var player in scenario.Players)
        {
            if (!CaptainPlayerEligible(player, policy))
            {
                continue;
            }

            var teamId = player.ActiveTeamId != 0 ? player.ActiveTeamId : player.CurrentTeamId;
            if (!teams.ContainsKey(teamId) && player.CurrentTeamId != teamId)
            {
                teamId = player.CurrentTeamId;
            }
            if (!teams.TryGetValue(teamId, out var team))
            {
                continue;
            }

            var candidate = new CaptainSelection
            {
                TeamId = team.TeamId,
                TeamName = team.Name,
                PlayerId = player.PlayerId,
                PlayerName = player.Name,
                Score = ScoreCaptainCandidate(player, team.TeamId, policy),
                Seeded = player.Seeded,
                SeedPriority = player.SeedPriority,
                Reason = player.Seeded
                    ? $"seed:{(string.IsNullOrWhiteSpace(player.SeedKey) ? "player_id" : player.SeedKey)}"
                    : "heuristic:veteran_value_salary_domestic_team_tenure",
            };

            if (!selected.TryGetValue(team.TeamId, out var current)
                    || CaptainCandidateShouldReplace(current, candidate))
            {
                selected[team.TeamId] = candidate;
            }
        }

        return selected
            .OrderBy(item => item.Key)
            .Select(item => item.Value)
            .ToArray();
    }

    private static bool CaptainPlayerEligible(CaptainPlayer player, CaptainSelectionPolicy policy)
    {
        return !player.Retired
            && player.Age >= policy.EligibleAgeMin
            && player.Age <= policy.EligibleAgeMax;
    }

    private static int ScoreCaptainCandidate(
        CaptainPlayer player,
        int teamId,
        CaptainSelectionPolicy policy)
    {
        var salaryScore = player.Salary <= 0
            ? 0
            : Math.Min(player.Salary / policy.SalaryScoreDivisor, policy.SalaryScoreMax);
        var score = player.ValueScore
            + salaryScore
            + CaptainAgeScore(player.Age, policy)
            + (player.NationId == KoreaNationId ? policy.DomesticBonus : -policy.ForeignPenalty)
            + (player.ActiveTeamId == teamId ? policy.ActiveTeamBonus : 0)
            + (player.CurrentTeamId == teamId ? policy.CurrentTeamBonus : 0)
            + CaptainSameTeamSeasonsScore(player.SameTeamSeasons, policy);
        score -= player.Dfa ? policy.DfaPenalty : 0;
        score -= player.Restricted ? policy.RestrictedPenalty : 0;
        score -= player.Injured ? policy.InjuredPenalty : 0;
        return score;
    }

    private static int CaptainAgeScore(int age, CaptainSelectionPolicy policy)
    {
        if (age >= policy.AgeCoreMin && age <= policy.AgeCoreMax)
        {
            return policy.AgeCoreScore;
        }
        if (age >= policy.AgeExtendedMin && age <= policy.AgeExtendedMax)
        {
            return policy.AgeExtendedScore;
        }
        if (age >= policy.AgeDepthMin && age <= policy.AgeDepthMax)
        {
            return policy.AgeDepthScore;
        }
        return 0;
    }

    private static int CaptainSameTeamSeasonsScore(int sameTeamSeasons, CaptainSelectionPolicy policy)
    {
        if (sameTeamSeasons <= 0)
        {
            return -policy.SameTeamUnknownPenalty;
        }
        if (sameTeamSeasons < policy.SameTeamMinSeasons)
        {
            return -policy.SameTeamShortPenalty;
        }
        return Math.Min(
            sameTeamSeasons * policy.SameTeamBonusPerSeason,
            policy.SameTeamBonusMax);
    }

    private static bool CaptainCandidateShouldReplace(
        CaptainSelection current,
        CaptainSelection candidate)
    {
        if (candidate.Seeded != current.Seeded)
        {
            return candidate.Seeded;
        }
        if (candidate.Seeded && candidate.SeedPriority != current.SeedPriority)
        {
            return candidate.SeedPriority > current.SeedPriority;
        }
        if (candidate.Score != current.Score)
        {
            return candidate.Score > current.Score;
        }
        return candidate.PlayerId < current.PlayerId;
    }

    private static T ReadJson<T>(string path)
    {
        return JsonSerializer.Deserialize<T>(File.ReadAllText(path), JsonOptions)
            ?? throw new InvalidOperationException($"Could not read JSON: {path}");
    }

    private static DateOnly ParseDate(string value)
    {
        return DateOnly.ParseExact(value, "yyyy-MM-dd", CultureInfo.InvariantCulture);
    }

    private static string FormatDate(DateOnly date)
    {
        return date.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture);
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

    public sealed class SeasonReplayScenario
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = "";

        [JsonPropertyName("kind")]
        public string Kind { get; set; } = "";

        [JsonPropertyName("inputs")]
        public ReplayInputs Inputs { get; set; } = new();

        [JsonPropertyName("expected_events")]
        public List<ReplayExpectedEvent> ExpectedEvents { get; set; } = [];

        [JsonPropertyName("teams")]
        public List<CaptainTeam> Teams { get; set; } = [];

        [JsonPropertyName("players")]
        public List<CaptainPlayer> Players { get; set; } = [];

        [JsonPropertyName("expected_captains")]
        public List<CaptainSelection> ExpectedCaptains { get; set; } = [];
    }

    public sealed class ReplayInputs
    {
        [JsonPropertyName("season")]
        public int Season { get; set; }

        [JsonPropertyName("date")]
        public string Date { get; set; } = "";

        [JsonPropertyName("anchor_date")]
        public string AnchorDate { get; set; } = "";

        [JsonPropertyName("league_id")]
        public int LeagueId { get; set; }
    }

    public sealed class ReplayExpectedEvent
    {
        [JsonPropertyName("kind")]
        public string Kind { get; set; } = "";

        [JsonPropertyName("date")]
        public string Date { get; set; } = "";

        [JsonPropertyName("title_key")]
        public string TitleKey { get; set; } = "";
    }

    public sealed class CustomEventCatalog
    {
        [JsonPropertyName("schedule")]
        public CustomEventSchedule Schedule { get; set; } = new();

        [JsonPropertyName("events")]
        public List<CustomEventCatalogEvent> Events { get; set; } = [];
    }

    public sealed class CustomEventSchedule
    {
        [JsonPropertyName("foreign_priority")]
        public ForeignPrioritySchedule ForeignPriority { get; set; } = new();
    }

    public sealed class ForeignPrioritySchedule
    {
        [JsonPropertyName("fa_declaration_offset_days")]
        public int FaDeclarationOffsetDays { get; set; }

        [JsonPropertyName("intl_established_fa_offset_days")]
        public int IntlEstablishedFaOffsetDays { get; set; }

        [JsonPropertyName("military_selection_offset_months")]
        public int MilitarySelectionOffsetMonths { get; set; }
    }

    public sealed class CustomEventCatalogEvent
    {
        [JsonPropertyName("kind")]
        public string Kind { get; set; } = "";

        [JsonPropertyName("title_key")]
        public string TitleKey { get; set; } = "";
    }

    public sealed class ForeignPlayerPolicy
    {
        [JsonPropertyName("waiver_window_days")]
        public int WaiverWindowDays { get; set; }
    }

    public sealed class CaptainSelectionPolicy
    {
        [JsonPropertyName("eligible_age_min")]
        public int EligibleAgeMin { get; set; }

        [JsonPropertyName("eligible_age_max")]
        public int EligibleAgeMax { get; set; }

        [JsonPropertyName("salary_score_divisor")]
        public int SalaryScoreDivisor { get; set; }

        [JsonPropertyName("salary_score_max")]
        public int SalaryScoreMax { get; set; }

        [JsonPropertyName("age_core_min")]
        public int AgeCoreMin { get; set; }

        [JsonPropertyName("age_core_max")]
        public int AgeCoreMax { get; set; }

        [JsonPropertyName("age_core_score")]
        public int AgeCoreScore { get; set; }

        [JsonPropertyName("age_extended_min")]
        public int AgeExtendedMin { get; set; }

        [JsonPropertyName("age_extended_max")]
        public int AgeExtendedMax { get; set; }

        [JsonPropertyName("age_extended_score")]
        public int AgeExtendedScore { get; set; }

        [JsonPropertyName("age_depth_min")]
        public int AgeDepthMin { get; set; }

        [JsonPropertyName("age_depth_max")]
        public int AgeDepthMax { get; set; }

        [JsonPropertyName("age_depth_score")]
        public int AgeDepthScore { get; set; }

        [JsonPropertyName("domestic_bonus")]
        public int DomesticBonus { get; set; }

        [JsonPropertyName("foreign_penalty")]
        public int ForeignPenalty { get; set; }

        [JsonPropertyName("active_team_bonus")]
        public int ActiveTeamBonus { get; set; }

        [JsonPropertyName("current_team_bonus")]
        public int CurrentTeamBonus { get; set; }

        [JsonPropertyName("same_team_min_seasons")]
        public int SameTeamMinSeasons { get; set; }

        [JsonPropertyName("same_team_bonus_per_season")]
        public int SameTeamBonusPerSeason { get; set; }

        [JsonPropertyName("same_team_bonus_max")]
        public int SameTeamBonusMax { get; set; }

        [JsonPropertyName("same_team_short_penalty")]
        public int SameTeamShortPenalty { get; set; }

        [JsonPropertyName("same_team_unknown_penalty")]
        public int SameTeamUnknownPenalty { get; set; }

        [JsonPropertyName("dfa_penalty")]
        public int DfaPenalty { get; set; }

        [JsonPropertyName("restricted_penalty")]
        public int RestrictedPenalty { get; set; }

        [JsonPropertyName("injured_penalty")]
        public int InjuredPenalty { get; set; }
    }

    public sealed class CaptainTeam
    {
        [JsonPropertyName("team_id")]
        public int TeamId { get; set; }

        [JsonPropertyName("code")]
        public string Code { get; set; } = "";

        [JsonPropertyName("name")]
        public string Name { get; set; } = "";
    }

    public sealed class CaptainPlayer
    {
        [JsonPropertyName("player_id")]
        public int PlayerId { get; set; }

        [JsonPropertyName("name")]
        public string Name { get; set; } = "";

        [JsonPropertyName("team_id")]
        public int TeamId { get; set; }

        [JsonPropertyName("current_team_id")]
        public int CurrentTeamId { get; set; }

        [JsonPropertyName("active_team_id")]
        public int ActiveTeamId { get; set; }

        [JsonPropertyName("nation_id")]
        public int NationId { get; set; }

        [JsonPropertyName("age")]
        public int Age { get; set; }

        [JsonPropertyName("salary")]
        public int Salary { get; set; }

        [JsonPropertyName("value_score")]
        public int ValueScore { get; set; }

        [JsonPropertyName("same_team_seasons")]
        public int SameTeamSeasons { get; set; }

        [JsonPropertyName("retired")]
        public bool Retired { get; set; }

        [JsonPropertyName("dfa")]
        public bool Dfa { get; set; }

        [JsonPropertyName("restricted")]
        public bool Restricted { get; set; }

        [JsonPropertyName("injured")]
        public bool Injured { get; set; }

        [JsonPropertyName("seeded")]
        public bool Seeded { get; set; }

        [JsonPropertyName("seed_priority")]
        public int SeedPriority { get; set; }

        [JsonPropertyName("seed_source")]
        public string SeedSource { get; set; } = "";

        [JsonPropertyName("seed_key")]
        public string SeedKey { get; set; } = "";
    }

    public sealed class CaptainSelection
    {
        [JsonPropertyName("team_id")]
        public int TeamId { get; set; }

        [JsonPropertyName("team_name")]
        public string TeamName { get; set; } = "";

        [JsonPropertyName("player_id")]
        public int PlayerId { get; set; }

        [JsonPropertyName("player_name")]
        public string PlayerName { get; set; } = "";

        [JsonPropertyName("score")]
        public int Score { get; set; }

        [JsonPropertyName("seeded")]
        public bool Seeded { get; set; }

        [JsonPropertyName("seed_priority")]
        public int SeedPriority { get; set; }

        [JsonPropertyName("reason")]
        public string Reason { get; set; } = "";
    }
}
