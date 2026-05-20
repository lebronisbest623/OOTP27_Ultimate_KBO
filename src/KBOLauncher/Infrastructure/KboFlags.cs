using System.Text.Json;
using System.Text.Json.Nodes;
using static LauncherPaths;

internal static partial class KboFlags
{
    private static readonly object ConfigWriteLock = new();
    private static readonly string[] PinnedEnabledRuntimeFlags =
    [
        "enable_experimental_runtime_hooks",
        "enable_kbo_current_date_tick_watchpoint",
    ];

    private static readonly string[] SettingsKeysMigratedFromFlags =
    [
        "allow_all_ui_team_actions",
        "asian_games_no_gold_odds_denominator",
        "asian_quota_fa_demand_above_average_salary",
        "asian_quota_fa_demand_average_salary",
        "asian_quota_fa_demand_below_average_salary",
        "asian_quota_fa_demand_fair_salary",
        "asian_quota_fa_demand_good_salary",
        "asian_quota_fa_demand_minimum_salary",
        "asian_quota_fa_demand_poor_salary",
        "asian_quota_fa_demand_star_salary",
        "asian_quota_fa_demand_superstar_salary",
        "asian_quota_salary_limit",
        "custom_news_language",
        "foreign_fa_demand_above_average_salary",
        "foreign_fa_demand_average_salary",
        "foreign_fa_demand_below_average_salary",
        "foreign_fa_demand_fair_salary",
        "foreign_fa_demand_good_salary",
        "foreign_fa_demand_minimum_salary",
        "foreign_fa_demand_poor_salary",
        "foreign_fa_demand_star_salary",
        "foreign_fa_demand_superstar_salary",
        "foreign_fa_non_asian_bullpen_quality_cap",
        "foreign_fa_non_asian_catcher_quality_cap",
        "foreign_fa_non_asian_hitter_quality_cap",
        "foreign_fa_non_asian_pitcher_quality_cap",
        "foreign_fa_non_asian_starter_quality_cap",
        "foreign_fa_quality_cap_enabled",
        "independent_acquisition_domestic_cash_cost",
        "independent_acquisition_domestic_seller_transfer_fee",
        "independent_acquisition_foreign_cash_cost",
        "independent_acquisition_foreign_seller_transfer_fee",
        "intl_established_fa_multiplier",
    ];

    public static void WriteKboFlag(string fileName, string label, bool enabled)
    {
        var key = NormalizeKboFlagKey(fileName);
        var path = GetKboFlagConfigPath();
        WriteKboFlagValue(path, key, enabled);
        Console.WriteLine($"{label}: {path} {key} = {(enabled ? "enabled" : "disabled")}");
    }

    internal static void WriteKboFlagValue(string configPath, string fileName, bool enabled)
    {
        WriteKboFlagValues(configPath, [fileName], enabled);
    }

    internal static void WriteKboFlagValues(string configPath, IEnumerable<string> fileNames, bool enabled)
    {
        lock (ConfigWriteLock)
        {
            var flags = ReadKboRawConfig(configPath);
            foreach (var fileName in fileNames)
            {
                flags[NormalizeKboFlagKey(fileName)] = JsonValue.Create(enabled);
            }
            WriteRawConfigAtomically(configPath, flags);
        }
    }

    public static void EnsureDefaultKboRuntimeFlags()
    {
        EnsureDefaultKboRuntimeFlags(GetKboFlagConfigPath());
    }

    public static void MigrateLegacyKboSettingsFromFlags()
    {
        MigrateLegacyKboSettingsFromFlags(GetKboFlagConfigPath(), GetKboSettingsConfigPath());
    }

    internal static void MigrateLegacyKboSettingsFromFlags(string flagsPath, string settingsPath)
    {
        lock (ConfigWriteLock)
        {
            var flags = ReadKboRawConfig(flagsPath);
            if (flags.Count == 0)
            {
                return;
            }

            var settings = ReadKboRawConfig(settingsPath);
            var flagsChanged = false;
            var settingsChanged = false;

            foreach (var rawKey in SettingsKeysMigratedFromFlags)
            {
                var key = NormalizeKboFlagKey(rawKey);
                if (!flags.TryGetValue(key, out var value))
                {
                    continue;
                }

                if (!settings.ContainsKey(key))
                {
                    settings[key] = value?.DeepClone();
                    settingsChanged = true;
                }
                flags.Remove(key);
                flagsChanged = true;
            }

            if (settingsChanged)
            {
                WriteRawConfigAtomically(settingsPath, settings);
            }
            if (flagsChanged)
            {
                WriteRawConfigAtomically(flagsPath, flags);
            }
        }
    }

    internal static void EnsureDefaultKboRuntimeFlags(string configPath)
    {
        var raw = ReadKboRawConfig(configPath);
        var changed = false;

        foreach (var flag in RuntimeFlags)
        {
            if (flag.DefaultValue is not null)
            {
                changed |= EnsureMissingFlag(raw, flag.Key, flag.DefaultValue.Value);
            }
        }
        foreach (var key in PinnedEnabledRuntimeFlags)
        {
            changed |= EnsureFlagValue(raw, key, true);
        }
        changed |= EnsureMissingFlag(raw, "enable_intl_established_fa_quality_probe_patch", true);

        if (!changed)
        {
            return;
        }

        lock (ConfigWriteLock)
        {
            WriteRawConfigAtomically(configPath, raw);
        }
    }

    private static bool EnsureMissingFlag(SortedDictionary<string, JsonNode?> flags, string key, bool value)
    {
        key = NormalizeKboFlagKey(key);
        if (flags.ContainsKey(key))
        {
            return false;
        }

        flags[key] = JsonValue.Create(value);
        return true;
    }

    private static bool EnsureFlagValue(SortedDictionary<string, JsonNode?> flags, string key, bool value)
    {
        key = NormalizeKboFlagKey(key);
        if (flags.TryGetValue(key, out var existing)
                && existing is JsonValue existingValue
                && existingValue.TryGetValue<bool>(out var existingBool)
                && existingBool == value)
        {
            return false;
        }

        flags[key] = JsonValue.Create(value);
        return true;
    }

    public static bool ReadKboFlag(string fileName)
    {
        return ReadKboFlag(GetKboFlagConfigPath(), fileName);
    }

    public static bool ReadKboFlagDefaultEnabled(string fileName)
    {
        return ReadKboFlagDefaultEnabled(GetKboFlagConfigPath(), fileName);
    }

    internal static bool ReadKboFlag(string configPath, string fileName)
    {
        return ReadKboFlagConfig(configPath).TryGetValue(NormalizeKboFlagKey(fileName), out var enabled)
            && enabled;
    }

    internal static bool ReadKboFlagDefaultEnabled(string configPath, string fileName)
    {
        var key = NormalizeKboFlagKey(fileName);
        return !ReadKboFlagConfig(configPath).TryGetValue(key, out var enabled)
            || enabled;
    }

    public static SortedDictionary<string, bool> ReadKboFlagConfig()
    {
        return ReadKboFlagConfig(GetKboFlagConfigPath());
    }

    internal static SortedDictionary<string, bool> ReadKboFlagConfig(string configPath)
    {
        var flags = new SortedDictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
        if (!File.Exists(configPath))
        {
            return flags;
        }

        try
        {
            using var doc = JsonDocument.Parse(File.ReadAllText(configPath));
            var root = doc.RootElement;
            if (root.ValueKind == JsonValueKind.Object
                    && root.TryGetProperty("flags", out var nestedFlags)
                    && nestedFlags.ValueKind == JsonValueKind.Object)
            {
                root = nestedFlags;
            }
            if (root.ValueKind != JsonValueKind.Object)
            {
                return flags;
            }

            foreach (var property in root.EnumerateObject())
            {
                if (TryReadJsonBool(property.Value, out var value))
                {
                    flags[NormalizeKboFlagKey(property.Name)] = value;
                }
            }
        }
        catch
        {
            return flags;
        }

        return flags;
    }

    internal static SortedDictionary<string, JsonNode?> ReadKboRawConfig(string configPath)
    {
        var values = new SortedDictionary<string, JsonNode?>(StringComparer.OrdinalIgnoreCase);
        if (!File.Exists(configPath))
        {
            return values;
        }

        try
        {
            var node = JsonNode.Parse(File.ReadAllText(configPath));
            var root = node as JsonObject;
            if (root is not null
                    && root.TryGetPropertyValue("flags", out var nestedFlags)
                    && nestedFlags is JsonObject nestedObject)
            {
                root = nestedObject;
            }
            if (root is null)
            {
                return values;
            }

            foreach (var property in root)
            {
                values[NormalizeKboFlagKey(property.Key)] = property.Value?.DeepClone();
            }
        }
        catch
        {
            return values;
        }

        return values;
    }

    public static bool TryReadJsonBool(JsonElement value, out bool result)
    {
        result = false;
        switch (value.ValueKind)
        {
            case JsonValueKind.True:
                result = true;
                return true;
            case JsonValueKind.False:
                return true;
            case JsonValueKind.Number:
                if (value.TryGetInt32(out var number))
                {
                    result = number != 0;
                    return true;
                }
                return false;
            case JsonValueKind.String:
                var text = value.GetString()?.Trim();
                return TryReadBooleanText(text, out result);
            default:
                return false;
        }
    }

    internal static bool TryReadBooleanText(string? text, out bool result)
    {
        result = false;
        text = text?.Trim();
        if (string.IsNullOrEmpty(text))
        {
            return false;
        }
        if (text.Equals("1", StringComparison.OrdinalIgnoreCase)
                || text.Equals("true", StringComparison.OrdinalIgnoreCase)
                || text.Equals("yes", StringComparison.OrdinalIgnoreCase)
                || text.Equals("on", StringComparison.OrdinalIgnoreCase)
                || text.Equals("enabled", StringComparison.OrdinalIgnoreCase))
        {
            result = true;
            return true;
        }
        if (text.Equals("0", StringComparison.OrdinalIgnoreCase)
                || text.Equals("false", StringComparison.OrdinalIgnoreCase)
                || text.Equals("no", StringComparison.OrdinalIgnoreCase)
                || text.Equals("off", StringComparison.OrdinalIgnoreCase)
                || text.Equals("disabled", StringComparison.OrdinalIgnoreCase))
        {
            result = false;
            return true;
        }
        return false;
    }

    public static int ReadKboIntlEstablishedFaMultiplier()
    {
        var defaultValue = ReadKboSeedIntDefault("economic_defaults.json", "intl_established_fa_multiplier", fallback: 0);
        return ReadKboIntSetting(
            GetKboSettingsConfigPath(),
            GetKboFlagConfigPath(),
            "intl_established_fa_multiplier",
            defaultValue,
            minValue: 1,
            maxValue: 20);
    }

    private static int ReadKboSeedIntDefault(string fileName, string key, int fallback)
    {
        var path = GetKboLocalDataPath(fileName);
        if (!File.Exists(path))
        {
            return fallback;
        }

        try
        {
            using var doc = JsonDocument.Parse(File.ReadAllText(path));
            if (doc.RootElement.ValueKind == JsonValueKind.Object
                    && doc.RootElement.TryGetProperty(key, out var value)
                    && TryReadJsonInt(value, out var parsed))
            {
                return parsed;
            }
        }
        catch
        {
            return fallback;
        }

        return fallback;
    }

    internal static int ReadKboIntSetting(string configPath, string key, int defaultValue, int minValue, int maxValue)
    {
        return TryReadKboIntSetting(configPath, key, minValue, maxValue, out var value)
            ? value
            : defaultValue;
    }

    internal static int ReadKboIntSetting(
        string configPath,
        string legacyConfigPath,
        string key,
        int defaultValue,
        int minValue,
        int maxValue)
    {
        if (TryReadKboIntSetting(configPath, key, minValue, maxValue, out var value))
        {
            return value;
        }
        if (!string.Equals(configPath, legacyConfigPath, StringComparison.OrdinalIgnoreCase)
                && TryReadKboIntSetting(legacyConfigPath, key, minValue, maxValue, out value))
        {
            return value;
        }
        return defaultValue;
    }

    private static bool TryReadKboIntSetting(string configPath, string key, int minValue, int maxValue, out int result)
    {
        result = 0;
        if (!File.Exists(configPath))
        {
            return false;
        }

        try
        {
            using var doc = JsonDocument.Parse(File.ReadAllText(configPath));
            var root = doc.RootElement;
            if (root.ValueKind == JsonValueKind.Object
                    && root.TryGetProperty("flags", out var nestedFlags)
                    && nestedFlags.ValueKind == JsonValueKind.Object)
            {
                root = nestedFlags;
            }
            if (root.ValueKind != JsonValueKind.Object)
            {
                return false;
            }

            foreach (var property in root.EnumerateObject())
            {
                if (NormalizeKboFlagKey(property.Name).Equals(key, StringComparison.OrdinalIgnoreCase)
                        && TryReadJsonInt(property.Value, out var value))
                {
                    result = Math.Clamp(value, minValue, maxValue);
                    return true;
                }
            }
        }
        catch
        {
            return false;
        }

        return false;
    }

    internal static void WriteKboIntSetting(string configPath, string key, int value, int minValue, int maxValue)
    {
        lock (ConfigWriteLock)
        {
            var values = ReadKboRawConfig(configPath);
            values[NormalizeKboFlagKey(key)] = JsonValue.Create(Math.Clamp(value, minValue, maxValue));
            WriteRawConfigAtomically(configPath, values);
        }
    }

    private static void WriteRawConfigAtomically(string configPath, SortedDictionary<string, JsonNode?> values)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(configPath)!);
        var json = JsonSerializer.Serialize(values, new JsonSerializerOptions { WriteIndented = true });
        var tempPath = Path.Combine(
            Path.GetDirectoryName(configPath)!,
            $".{Path.GetFileName(configPath)}.{Environment.ProcessId}.{Guid.NewGuid():N}.tmp");

        File.WriteAllText(tempPath, json + Environment.NewLine);
        if (File.Exists(configPath))
        {
            File.Replace(tempPath, configPath, null);
        }
        else
        {
            File.Move(tempPath, configPath);
        }
    }

    public static bool TryReadJsonInt(JsonElement value, out int result)
    {
        result = 0;
        switch (value.ValueKind)
        {
            case JsonValueKind.Number:
                return value.TryGetInt32(out result);
            case JsonValueKind.String:
                return int.TryParse(value.GetString()?.Trim(), out result);
            default:
                return false;
        }
    }

    public static string NormalizeKboFlagKey(string fileName)
    {
        var key = Path.GetFileName(fileName);
        return key.EndsWith(".txt", StringComparison.OrdinalIgnoreCase)
            ? key[..^4]
            : key;
    }
    
    public static void WriteKboForeignWaiverAiFlag(bool enabled)
    {
        WriteKboFlag("enable_foreign_waiver_ai.txt", "Foreign waiver AI flag", enabled);
    }
    
    public static void WriteKboSingleDivisionAllstarEventsFlag(bool enabled)
    {
        var path = GetKboFlagConfigPath();
        WriteKboSingleDivisionAllstarEventsFlag(path, enabled);
        Console.WriteLine($"Single-division all-star flags: {path} = {(enabled ? "enabled" : "disabled")}");
    }

    internal static void WriteKboSingleDivisionAllstarEventsFlag(string configPath, bool enabled)
    {
        WriteKboFlagValues(configPath, SingleDivisionAllstarFlagFiles, enabled);
    }
}
