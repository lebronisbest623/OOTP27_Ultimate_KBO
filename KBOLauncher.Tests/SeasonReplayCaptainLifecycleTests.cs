namespace KBOLauncher.Tests;

using System.Globalization;
using System.Text.Json;
using FluentAssertions;
using Xunit;

public sealed partial class SeasonReplayCaptainLifecycleTests
{
    private const string CaptainMaintenanceDecisionKind = "captain.maintenance_decision";
    private const string CaptainInseasonRepairKind = "captain.inseason_repair";
    private const int KoreaNationId = 177;

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };

    public static IEnumerable<object[]> CaptainMaintenanceScenarios()
    {
        return LoadScenarios("captain_maintenance");
    }

    public static IEnumerable<object[]> CaptainRepairScenarios()
    {
        return LoadScenarios("captain_repair");
    }

    [Theory]
    [MemberData(nameof(CaptainMaintenanceScenarios))]
    public void CaptainMaintenanceDecision_ReplaysExpectedAction(SeasonReplayScenario scenario)
    {
        scenario.Kind.Should().Be(CaptainMaintenanceDecisionKind);

        var actual = ReplayCaptainMaintenanceDecision(scenario.Inputs);

        actual.Should().BeEquivalentTo(
            scenario.ExpectedMaintenance,
            options => options.WithStrictOrdering());
    }

    [Theory]
    [MemberData(nameof(CaptainRepairScenarios))]
    public void CaptainInseasonRepair_ReplaysExpectedMergedCaptains(SeasonReplayScenario scenario)
    {
        scenario.Kind.Should().Be(CaptainInseasonRepairKind);

        var root = FindRepoRoot();
        var policy = ReadJson<CaptainSelectionPolicy>(
            Path.Combine(root, "data", "seeds", "captain", "captain_selection_policy.json"));

        var actual = ReplayCaptainInseasonRepair(scenario, policy);

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

    private static IReadOnlyList<CaptainMaintenanceAction> ReplayCaptainMaintenanceDecision(ReplayInputs inputs)
    {
        if (!inputs.LeaguePtrAvailable)
        {
            return ReplayCaptainSeedStartupWithoutLeaguePtr(inputs);
        }

        var date = ParseDate(inputs.Date);
        var effectiveSeason = CaptainEffectiveSeason(date, inputs.LeagueSeason);
        if (effectiveSeason < 1982 || effectiveSeason > 2200)
        {
            return [];
        }

        var calendarRecovery = CaptainCalendarSeasonRecoveryActive(date, inputs.LeagueSeason, inputs.Phase);
        var seedStartup = CaptainSeedStartupWindowActive(date, effectiveSeason) && inputs.SeedAvailable;
        var preseasonFirstDay = CaptainPreseasonFirstDayActive(date, inputs.LeagueSeason, inputs.Phase);
        var actions = new List<CaptainMaintenanceAction>();

        if (inputs.CsvExists && inputs.SummaryRowsAvailable)
        {
            actions.Add(new CaptainMaintenanceAction
            {
                Action = "emit_initial_selection_news",
                Season = effectiveSeason,
                Reason = "csv_summary",
            });
        }

        if (seedStartup && !inputs.CsvExists)
        {
            actions.Add(CaptainBootstrapAction(effectiveSeason, "seed_startup_missing_csv"));
            return actions;
        }
        if (preseasonFirstDay && !inputs.CsvExists)
        {
            actions.Add(CaptainBootstrapAction(effectiveSeason, "preseason_first_day_missing_csv"));
            return actions;
        }
        if (inputs.Phase == 3)
        {
            if (inputs.CsvExists)
            {
                actions.Add(new CaptainMaintenanceAction
                {
                    Action = "inseason_repair",
                    Season = effectiveSeason,
                    Reason = "regular_season_existing_csv",
                });
            }

            return actions;
        }
        if (calendarRecovery && inputs.CsvExists)
        {
            actions.Add(new CaptainMaintenanceAction
            {
                Action = "inseason_repair",
                Season = effectiveSeason,
                Reason = "calendar_year_recovery",
            });
        }

        return actions;
    }

    private static IReadOnlyList<CaptainMaintenanceAction> ReplayCaptainSeedStartupWithoutLeaguePtr(ReplayInputs inputs)
    {
        var date = ParseDate(inputs.Date);
        var season = date.Year;
        if (!CaptainSeedStartupWindowActive(date, season) || !inputs.SeedAvailable)
        {
            return [];
        }

        return
        [
            inputs.CsvExists
                ? new CaptainMaintenanceAction
                {
                    Action = "emit_initial_selection_news",
                    Season = season,
                    Reason = "seed_startup_summary_no_league_ptr",
                }
                : CaptainBootstrapAction(season, "seed_startup_no_league_ptr_missing_csv"),
        ];
    }

    private static CaptainMaintenanceAction CaptainBootstrapAction(int season, string reason)
    {
        return new CaptainMaintenanceAction
        {
            Action = "write_missing_selection_csv",
            Season = season,
            Reason = reason,
        };
    }

    private static IReadOnlyList<CaptainSelection> ReplayCaptainInseasonRepair(
        SeasonReplayScenario scenario,
        CaptainSelectionPolicy policy)
    {
        var candidateRows = ReplayCaptainSelection(scenario, policy)
            .ToDictionary(row => row.TeamId);
        var currentRows = scenario.CurrentCaptains.ToDictionary(row => row.TeamId);
        var merged = new List<CaptainSelection>();

        foreach (var team in scenario.Teams.OrderBy(team => team.TeamId))
        {
            currentRows.TryGetValue(team.TeamId, out var current);
            if (current is not null && current.StillWithTeam)
            {
                merged.Add(current.ToSelection());
                continue;
            }

            if (candidateRows.TryGetValue(team.TeamId, out var candidate))
            {
                candidate.Reason = current is not null && current.PlayerId != 0
                    ? $"inseason_replacement:departed:{current.PlayerId}"
                    : "inseason_replacement:missing_row";
                merged.Add(candidate);
            }
            else if (current is not null)
            {
                merged.Add(current.ToSelection());
            }
        }

        return merged;
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

    private static int CaptainEffectiveSeason(DateOnly date, int leagueSeason)
    {
        if (!CaptainYearPlausible(leagueSeason))
        {
            return date.Year;
        }
        return date.Year > leagueSeason ? date.Year : leagueSeason;
    }

    private static bool CaptainCalendarSeasonRecoveryActive(DateOnly date, int leagueSeason, int phase)
    {
        return CaptainYearPlausible(leagueSeason)
            && date.Year > leagueSeason
            && phase != 2
            && phase != 3;
    }

    private static bool CaptainPreseasonFirstDayActive(DateOnly date, int leagueSeason, int phase)
    {
        return phase == 2
            && date.Year == CaptainEffectiveSeason(date, leagueSeason)
            && MonthDay(date) == 301;
    }

    private static bool CaptainSeedStartupWindowActive(DateOnly date, int season)
    {
        return CaptainYearPlausible(season)
            && date.Year == season
            && MonthDay(date) >= 301
            && MonthDay(date) <= 415;
    }

    private static bool CaptainYearPlausible(int year)
    {
        return year >= 1982 && year <= 2200;
    }

    private static int MonthDay(DateOnly date)
    {
        return (date.Month * 100) + date.Day;
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
