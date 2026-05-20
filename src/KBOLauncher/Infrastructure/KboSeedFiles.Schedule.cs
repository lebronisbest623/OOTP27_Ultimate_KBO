using System.Text.RegularExpressions;

internal static partial class KboSeedFiles
{
    public static void EnsureKboScheduleAllstarGameLines(string? ootpExePath)
    {
        foreach (var directory in ResolveKboScheduleDirectories(ootpExePath).Distinct(StringComparer.OrdinalIgnoreCase))
        {
            EnsureKboScheduleAllstarGameLinesInDirectory(directory);
        }
    }

    internal static void EnsureKboScheduleAllstarGameLinesInDirectory(string scheduleDirectory)
    {
        if (string.IsNullOrWhiteSpace(scheduleDirectory) || !Directory.Exists(scheduleDirectory))
        {
            return;
        }

        foreach (var path in Directory.EnumerateFiles(scheduleDirectory, OotpProduct.KboScheduleSearchPattern))
        {
            try
            {
                if (EnsureKboScheduleAllstarGameLine(path))
                {
                    Console.WriteLine($"KBO schedule all-star game: repaired {path}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"KBO schedule all-star game: failed to repair {path}: {ex.Message}");
            }
        }
    }

    internal static bool EnsureKboScheduleAllstarGameLine(string path)
    {
        var text = File.ReadAllText(path);
        var gamesMatch = Regex.Match(text, "<GAMES>\\s*", RegexOptions.IgnoreCase);
        var gamesEndMatch = Regex.Match(text, "</GAMES>", RegexOptions.IgnoreCase);
        var hasTypeFourInsideGames = false;
        if (gamesMatch.Success && gamesEndMatch.Success && gamesEndMatch.Index > gamesMatch.Index)
        {
            var gamesBlock = text.Substring(gamesMatch.Index, gamesEndMatch.Index - gamesMatch.Index);
            hasTypeFourInsideGames = gamesBlock.Contains("type=\"4\"", StringComparison.OrdinalIgnoreCase);
        }
        else if (text.Contains("type=\"4\"", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        var changed = false;
        if (!hasTypeFourInsideGames)
        {
            var dayMatch = Regex.Match(text, "allstar_game_day\\s*=\\s*\"(?<day>\\d+)\"", RegexOptions.IgnoreCase);
            if (!dayMatch.Success || !int.TryParse(dayMatch.Groups["day"].Value, out var day) || day <= 0)
            {
                return false;
            }

            var newline = text.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
            var allstarLine = $"<Game day=\"{day}\" time=\"1830\" away=\"0\" home=\"0\" type=\"4\" />{newline}";
            if (gamesMatch.Success)
            {
                var insertAt = gamesMatch.Index + gamesMatch.Length;
                text = text.Insert(insertAt, allstarLine);
            }
            else
            {
                var scheduleHeaderEnd = text.IndexOf('>');
                if (scheduleHeaderEnd < 0)
                {
                    return false;
                }
                text = text.Insert(scheduleHeaderEnd + 1, newline + allstarLine);
            }
            changed = true;
        }

        var cleaned = RemoveKboAllstarGameLinesOutsideGamesBlock(text);
        if (!string.Equals(cleaned, text, StringComparison.Ordinal))
        {
            text = cleaned;
            changed = true;
        }

        if (changed)
        {
            File.WriteAllText(path, text);
        }
        return changed;
    }

    private static string RemoveKboAllstarGameLinesOutsideGamesBlock(string text)
    {
        var gamesStart = Regex.Match(text, "<GAMES>\\s*", RegexOptions.IgnoreCase);
        var gamesEnd = Regex.Match(text, "</GAMES>", RegexOptions.IgnoreCase);
        if (!gamesStart.Success || !gamesEnd.Success || gamesEnd.Index <= gamesStart.Index)
        {
            return text;
        }

        var before = text[..gamesStart.Index];
        var games = text[gamesStart.Index..gamesEnd.Index];
        var after = text[gamesEnd.Index..];
        const string strayAllstarGameLine = "^[ \\t]*<Game\\b[^\\r\\n>]*type\\s*=\\s*\"4\"[^\\r\\n>]*>\\s*(?:\\r?\\n)?";
        before = Regex.Replace(before, strayAllstarGameLine, "", RegexOptions.IgnoreCase | RegexOptions.Multiline);
        after = Regex.Replace(after, strayAllstarGameLine, "", RegexOptions.IgnoreCase | RegexOptions.Multiline);
        return before + games + after;
    }

    private static IEnumerable<string> ResolveKboScheduleDirectories(string? ootpExePath)
    {
        if (!string.IsNullOrWhiteSpace(ootpExePath))
        {
            var ootpDir = Path.GetDirectoryName(ootpExePath);
            if (!string.IsNullOrWhiteSpace(ootpDir))
            {
                yield return Path.Combine(ootpDir, OotpProduct.DataDirectoryName, OotpProduct.SchedulesDirectoryName);
            }
        }

        var documents = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
        if (!string.IsNullOrWhiteSpace(documents))
        {
            yield return Path.Combine(
                documents,
                OotpProduct.VendorFolderName,
                OotpProduct.ProductFolderName,
                OotpProduct.SchedulesDirectoryName);
        }
    }
}
